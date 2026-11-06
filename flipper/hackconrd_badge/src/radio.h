// CC1101 driver, register- and packet-compatible with the physical badge.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PROFILE_PACKET_GFSK = 0, // 9-byte frames, Learning lab
    PROFILE_FOX_OOK_RSSI = 1, // live RSSI, fox hunting
    PROFILE_FRIENDS_GFSK_32B = 2, // 32-byte frames, Friends and THE BOX
} RadioProfile;

// The channel real badges use for Friends and THE BOX. It is hardcoded in their firmware: the badge's
// Frequency setting only moves the hunt.
#define RADIO_BADGE_FREQ_HZ 433000000UL

typedef struct {
    uint8_t payload[32];
    int8_t rssi_dbm;
    uint8_t lqi;
    bool crc_ok;
    uint32_t ms;
    bool is_virtual; // injected by Solo Mode, never on the air
} RxFrame;

void radio_power_up(RadioProfile profile);
void radio_power_down(void);
void radio_apply_profile(RadioProfile profile);
RadioProfile radio_profile(void);
bool radio_is_on(void);

// Frequency actually used by the packet profiles; see radio.c for why it can differ from 433.00.
uint32_t radio_packet_freq_hz(void);
bool radio_freq_tx_allowed(uint32_t hz);
// Packets the firmware's regional lock refused since boot. Shown in Settings > Radio Info.
uint32_t radio_tx_blocked_count(void);

// Live tuning used by the spectrum lesson. Leaves the radio idle; call radio_enter_rx() next.
void radio_tune_hz(uint32_t hz);
void radio_enter_rx(void);
void radio_idle(void);
int16_t radio_rssi_dbm(void);
// IDLE -> flush -> RX, wait for a valid reading. What the badge's fox loop and LBT do.
int16_t radio_rssi_sample(void);

void radio_rx_service(void);
bool radio_rx_pop(RxFrame* out);
void radio_rx_push(const RxFrame* in);

bool radio_send_fixed9(const uint8_t payload[9]);
bool radio_send_fixed32(const uint8_t payload[32]);

// Steady carrier for the CTFs Bearer. `pa` is a CC1101 PATABLE value. Returns false if the firmware's
// regional check refuses the frequency. Stopping restores the current profile in RX.
bool radio_carrier_start(uint32_t hz, uint8_t pa);
void radio_carrier_stop(void);
