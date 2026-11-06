#include "radio.h"
#include "badge.h"
#include "prefs.h"

// The firmware's packet helpers (furi_hal_subghz_read_packet/write_packet) assume a
// variable-length format with a leading length byte. The badge uses fixed-length frames, so
// FIFO access goes straight over SPI. Tuning and the TX gate still go through furi_hal_subghz,
// so the device's regional TX restrictions keep applying.

// Registers
#define CC_IOCFG0   0x02
#define CC_PKTLEN   0x06
#define CC_PKTCTRL1 0x07
#define CC_PKTCTRL0 0x08
#define CC_MDMCFG4  0x10
#define CC_MDMCFG3  0x11
#define CC_MDMCFG2  0x12
#define CC_MDMCFG1  0x13
#define CC_DEVIATN  0x15
#define CC_MCSM1    0x17
#define CC_MCSM0    0x18
#define CC_FOCCFG   0x19
#define CC_AGCCTRL2 0x1B
#define CC_FREND0   0x22
// Strobes
#define CC_SRX   0x34
#define CC_SIDLE 0x36
#define CC_SFRX  0x3A
#define CC_SFTX  0x3B
// Access flags and addresses
#define CC_READ         0x80
#define CC_BURST        0x40
#define CC_FIFO         0x3F
#define CC_ST_MARCSTATE 0x35
#define CC_ST_RXBYTES   0x3B
// MARCSTATE values
#define MARC_IDLE        0x01
#define MARC_TXUNDERFLOW 0x16

#define PA_MAX 0xC0

// Badge channel. Most firmware regions start the 433 MHz band at 433.05, so a Flipper may not be
// allowed to transmit on 433.00. Those devices use the band edge instead, which lets them talk to
// each other. It does not reach real badges: tested against one, 50 kHz of offset failed in both
// directions, too much for +/-58 kHz of frequency compensation once crystal error is added.
// The regional lock itself is never bypassed.
#define PACKET_FREQ_FALLBACK_HZ 433050000UL

// Each preset matches the badge's ccConfigure*() on top of CC1101 reset defaults.
static const uint8_t preset_gfsk_9b[] = {
    CC_MDMCFG4, 0x77, CC_MDMCFG3, 0x83, CC_MDMCFG2, 0x12, CC_MDMCFG1, 0x40, CC_DEVIATN, 0x15,
    CC_PKTLEN,  9,    CC_PKTCTRL1, 0x0C, CC_PKTCTRL0, 0x04, CC_MCSM1, 0x0C, CC_MCSM0, 0x18,
    CC_FOCCFG,  0x16, CC_AGCCTRL2, 0x43,
    0,          0,
    PA_MAX,     0,    0, 0, 0, 0, 0, 0,
};

static const uint8_t preset_gfsk_32b[] = {
    CC_MDMCFG4, 0x77, CC_MDMCFG3, 0x83, CC_MDMCFG2, 0x12, CC_MDMCFG1, 0x40, CC_DEVIATN, 0x15,
    CC_PKTLEN,  32,   CC_PKTCTRL1, 0x0C, CC_PKTCTRL0, 0x04, CC_MCSM1, 0x0C, CC_MCSM0, 0x18,
    CC_FOCCFG,  0x16, CC_AGCCTRL2, 0x43,
    0,          0,
    PA_MAX,     0,    0, 0, 0, 0, 0, 0,
};

static const uint8_t preset_fox_ook[] = {
    CC_MDMCFG4, 0x77, CC_MDMCFG3, 0x83, CC_MDMCFG2, 0x30, CC_MDMCFG1, 0x40, CC_DEVIATN, 0x00,
    CC_PKTLEN,  0xFF, CC_PKTCTRL1, 0x04, CC_PKTCTRL0, 0x00, CC_MCSM1, 0x0C, CC_MCSM0, 0x18,
    0,          0,
    PA_MAX,     0,    0, 0, 0, 0, 0, 0,
};

// Asynchronous OOK with the data pin held high gives an unmodulated carrier, which is how the
// firmware's own carrier test works. The badge's hunt samples RSSI at single moments, so a pulsed
// signal would make its bar jump between the signal and the noise floor. A '1' uses PATABLE[1].
static const uint8_t preset_carrier[] = {
    CC_IOCFG0,  0x0D, CC_MDMCFG4, 0x77, CC_MDMCFG3, 0x83, CC_MDMCFG2, 0x30, CC_MDMCFG1, 0x40,
    CC_DEVIATN, 0x00, CC_PKTCTRL1, 0x04, CC_PKTCTRL0, 0x32, CC_MCSM1, 0x0C, CC_MCSM0, 0x18,
    CC_FREND0,  0x11,
    0,          0,
    0x00,       PA_MAX, 0, 0, 0, 0, 0, 0,
};
#define PRESET_CARRIER_PA_INDEX (sizeof(preset_carrier) - 7)

#define RXQ_SIZE 8

static struct {
    bool on;
    bool carrier;
    RadioProfile profile;
    RxFrame rxq[RXQ_SIZE];
    uint8_t rxq_w, rxq_r;
    uint32_t tx_blocked;
} radio;

static const FuriHalSpiBusHandle* const spi = &furi_hal_spi_bus_handle_subghz;

// --- SPI primitives. Acquiring the handle asserts CSN, releasing it deasserts. ---

static void cc_wait_ready(void) {
    for(uint32_t i = 0; i < 100000 && furi_hal_gpio_read(spi->miso); i++) {
    }
}

static void cc_strobe(uint8_t strobe) {
    uint8_t rx;
    furi_hal_spi_acquire(spi);
    cc_wait_ready();
    furi_hal_spi_bus_trx(spi, &strobe, &rx, 1, 100);
    furi_hal_spi_release(spi);
}

static uint8_t cc_read_status_once(uint8_t addr) {
    uint8_t tx[2] = {(uint8_t)(addr | CC_READ | CC_BURST), 0};
    uint8_t rx[2] = {0, 0};
    furi_hal_spi_acquire(spi);
    cc_wait_ready();
    furi_hal_spi_bus_trx(spi, tx, rx, 2, 100);
    furi_hal_spi_release(spi);
    return rx[1];
}

// CC1101 errata: live status registers can glitch while updating, so read until two reads agree.
static uint8_t cc_read_status(uint8_t addr) {
    uint8_t a = cc_read_status_once(addr);
    for(uint8_t i = 0; i < 8; i++) {
        uint8_t b = cc_read_status_once(addr);
        if(a == b) break;
        a = b;
    }
    return a;
}

static void cc_write_fifo(const uint8_t* data, uint8_t len) {
    uint8_t tx[33];
    uint8_t rx[33];
    furi_check(len <= 32);
    tx[0] = CC_FIFO | CC_BURST;
    memcpy(&tx[1], data, len);
    furi_hal_spi_acquire(spi);
    cc_wait_ready();
    furi_hal_spi_bus_trx(spi, tx, rx, len + 1, 100);
    furi_hal_spi_release(spi);
}

static void cc_read_fifo(uint8_t* out, uint8_t len) {
    uint8_t tx[35];
    uint8_t rx[35];
    furi_check(len <= 34);
    memset(tx, 0, sizeof(tx));
    tx[0] = CC_FIFO | CC_READ | CC_BURST;
    furi_hal_spi_acquire(spi);
    cc_wait_ready();
    furi_hal_spi_bus_trx(spi, tx, rx, len + 1, 100);
    furi_hal_spi_release(spi);
    memcpy(out, &rx[1], len);
}

static void cc_wait_marcstate(uint8_t state, uint32_t timeout_ms) {
    uint32_t t0 = millis();
    while(millis() - t0 < timeout_ms) {
        if((cc_read_status(CC_ST_MARCSTATE) & 0x1F) == state) return;
    }
}

static inline int16_t convert_rssi(uint8_t raw) {
    int16_t r = raw;
    if(r >= 128) r -= 256;
    return (r / 2) - 74;
}

// --- Control ---------------------------------------------------------------

uint32_t radio_packet_freq_hz(void) {
    return furi_hal_region_is_frequency_allowed(RADIO_BADGE_FREQ_HZ) ? RADIO_BADGE_FREQ_HZ :
                                                                       PACKET_FREQ_FALLBACK_HZ;
}

bool radio_freq_tx_allowed(uint32_t hz) {
    return furi_hal_region_is_frequency_allowed(hz);
}

uint32_t radio_tx_blocked_count(void) {
    return radio.tx_blocked;
}

void radio_idle(void) {
    cc_strobe(CC_SIDLE);
    cc_wait_marcstate(MARC_IDLE, 10);
}

void radio_enter_rx(void) {
    radio_idle();
    cc_strobe(CC_SFRX);
    cc_strobe(CC_SRX);
}

void radio_tune_hz(uint32_t hz) {
    radio_idle();
    furi_hal_subghz_set_frequency_and_path(hz);
}

void radio_apply_profile(RadioProfile profile) {
    radio.profile = profile;
    const uint8_t* preset;
    uint32_t freq;
    switch(profile) {
    case PROFILE_PACKET_GFSK:
        preset = preset_gfsk_9b;
        freq = radio_packet_freq_hz();
        break;
    case PROFILE_FRIENDS_GFSK_32B:
        preset = preset_gfsk_32b;
        freq = radio_packet_freq_hz();
        break;
    default:
        preset = preset_fox_ook;
        freq = prefs.fox_freq100 * 10000UL;
        break;
    }
    furi_hal_subghz_load_custom_preset(preset); // resets the chip first
    radio_tune_hz(freq);
    radio.rxq_w = radio.rxq_r = 0;
    radio_enter_rx();
}

void radio_power_up(RadioProfile profile) {
    radio_apply_profile(profile);
    radio.on = true;
}

static void carrier_release_pin(void) {
    radio.carrier = false;
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void radio_power_down(void) {
    if(!radio.on) return;
    radio.on = false;
    radio_idle();
    if(radio.carrier) carrier_release_pin();
    cc_strobe(CC_SFRX);
    cc_strobe(CC_SFTX);
    furi_hal_subghz_sleep();
}

RadioProfile radio_profile(void) {
    return radio.profile;
}

bool radio_is_on(void) {
    return radio.on;
}

int16_t radio_rssi_dbm(void) {
    return (int16_t)furi_hal_subghz_get_rssi();
}

int16_t radio_rssi_sample(void) {
    radio_enter_rx();
    // FS_AUTOCAL recalibrates on IDLE->RX (~720 us), then RSSI needs time to settle.
    furi_delay_us(1500);
    return radio_rssi_dbm();
}

// --- RX --------------------------------------------------------------------

void radio_rx_push(const RxFrame* in) {
    uint8_t next = (radio.rxq_w + 1) % RXQ_SIZE;
    if(next == radio.rxq_r) radio.rxq_r = (radio.rxq_r + 1) % RXQ_SIZE; // drop oldest
    radio.rxq[radio.rxq_w] = *in;
    radio.rxq_w = next;
}

bool radio_rx_pop(RxFrame* out) {
    if(radio.rxq_w == radio.rxq_r) return false;
    *out = radio.rxq[radio.rxq_r];
    radio.rxq_r = (radio.rxq_r + 1) % RXQ_SIZE;
    return true;
}

void radio_rx_service(void) {
    if(!radio.on || radio.profile == PROFILE_FOX_OOK_RSSI) return;

    uint8_t raw = cc_read_status(CC_ST_RXBYTES);
    if(raw & 0x80) { // overflow
        radio_enter_rx();
        return;
    }

    uint8_t bytes = raw & 0x7F;
    const uint8_t pkt_len = (radio.profile == PROFILE_FRIENDS_GFSK_32B) ? 32 : 9;
    const uint8_t total = pkt_len + 2; // appended RSSI + LQI/CRC

    while(bytes >= total) {
        uint8_t pkt[34];
        cc_read_fifo(pkt, total);

        RxFrame f;
        memset(&f, 0, sizeof(f));
        memcpy(f.payload, pkt, pkt_len);
        f.crc_ok = (pkt[pkt_len + 1] & 0x80) != 0;
        f.lqi = pkt[pkt_len + 1] & 0x7F;
        f.rssi_dbm = (int8_t)convert_rssi(pkt[pkt_len]);
        f.ms = millis();
        radio_rx_push(&f);

        raw = cc_read_status(CC_ST_RXBYTES);
        if(raw & 0x80) {
            radio_enter_rx();
            return;
        }
        bytes = raw & 0x7F;
    }
}

// --- TX --------------------------------------------------------------------

static bool radio_send(const uint8_t* payload, uint8_t len, uint32_t timeout_ms) {
    if(!radio.on) return false;

    radio_idle();
    cc_strobe(CC_SFTX);
    cc_write_fifo(payload, len);

    // furi_hal_subghz_tx() is the regional gate: it refuses when the tuned frequency is not
    // allowed for this device.
    if(!furi_hal_subghz_tx()) {
        radio.tx_blocked++;
        cc_strobe(CC_SFTX);
        radio_enter_rx();
        return false;
    }

    uint8_t state = 0;
    uint32_t t0 = millis();
    while(millis() - t0 < timeout_ms) {
        state = cc_read_status(CC_ST_MARCSTATE) & 0x1F;
        if(state == MARC_IDLE || state == MARC_TXUNDERFLOW) break;
        furi_delay_ms(2);
    }

    bool ok = (state != MARC_TXUNDERFLOW);
    radio_idle();
    if(!ok) cc_strobe(CC_SFTX);
    radio_enter_rx();
    return ok;
}

bool radio_send_fixed9(const uint8_t payload[9]) {
    return radio_send(payload, 9, 600);
}

bool radio_send_fixed32(const uint8_t payload[32]) {
    // Listen before talk, same parameters as ccSendFixed32().
    uint8_t tries = 0;
    while(radio.on && radio_rssi_sample() >= -80 && tries < 10) {
        furi_delay_ms(10 + (rand_u32() % 40));
        tries++;
    }
    return radio_send(payload, 32, 800);
}

bool radio_carrier_start(uint32_t hz, uint8_t pa) {
    if(!radio.on || radio.carrier) return false;

    uint8_t preset[sizeof(preset_carrier)];
    memcpy(preset, preset_carrier, sizeof(preset));
    preset[PRESET_CARRIER_PA_INDEX] = pa;
    furi_hal_subghz_load_custom_preset(preset);
    radio_tune_hz(hz);

    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, true);

    // Same regional gate as packets.
    if(!furi_hal_subghz_tx()) {
        radio.tx_blocked++;
        carrier_release_pin();
        radio_apply_profile(radio.profile);
        return false;
    }
    radio.carrier = true;
    return true;
}

void radio_carrier_stop(void) {
    if(!radio.carrier) return;
    radio_idle();
    carrier_release_pin();
    radio_apply_profile(radio.profile);
}
