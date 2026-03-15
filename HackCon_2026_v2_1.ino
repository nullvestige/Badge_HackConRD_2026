// =====================================================
// RF-CTF 433 (HackConRD26) - UI MATRIX THEME (v2.1)
// =====================================================

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <FastLED.h>
#include <Preferences.h>
#include <QRCodeGenerator.h>

// ======================= TIPOS + PROTOTIPOS (OBLIGATORIO ARRIBA EN .INO) =======================
struct ButtonsEdge { bool up=false, down=false, ok=false, back=false, any=false; };

enum RadioProfile : uint8_t {
  PROFILE_PACKET_GFSK   = 0,
  PROFILE_FOX_OOK_RSSI  = 1,
  PROFILE_FRIENDS_GFSK_32B = 2
};

enum LedSet : uint8_t;
enum LedTheme : uint8_t;

enum ScreenMode : uint8_t {
  MODE_MENU = 0,
  MODE_FOX_HUNT,
  MODE_FRIENDS_MENU,
  MODE_FRIENDS_SCAN,
  MODE_FRIENDS_REQUESTING,
  MODE_FRIENDS_LIST,
  MODE_FRIEND_REQ_PROMPT,
  MODE_THE_BOX,
  MODE_LEARNING,
  MODE_SETTINGS,
  MODE_THE_KEY,
  MODE_FIRST_BOOT,
  MODE_GAMES_MENU,
  MODE_DINO,
  MODE_MAZE,
  MODE_HACKMON,
};
static ScreenMode mode = MODE_MENU;

// ---- Forward declarations (needed due to Arduino .ino ordering) ----
static void frfBackgroundTick();
static void screensaverTick();
static void exitScreensaver();
static void goTo(ScreenMode m, bool skipIntro = false);
static inline bool frfWantedForMode(ScreenMode m);
static void drawFriendsList();


enum RfkcPhase : uint8_t {
  RFKC_MENU = 0,
  RFKC_RECON,
  RFKC_ANALYSIS,
  RFKC_INTERFERENCE,
  RFKC_EXPLOIT,
  RFKC_FEEDBACK,
  RFKC_SUMMARY,
};

// ==== Badge profile types ====
enum BadgeRole : uint8_t {
  ROLE_SCRPTK1D = 0,
  ROLE_H4CK3R  = 1,
  ROLE_PHR3AK  = 2,
  ROLE_R3DT34M = 3,
  ROLE_PWN3D   = 4
};


// ======================= RFLAB HEADER (MUST BE AT TOP) =======================

#ifndef RFLAB_HEADER_GUARD
#define RFLAB_HEADER_GUARD

// CC1101 regs (define if missing)
#ifndef CC1101_RXBYTES
  #define CC1101_RXBYTES   0x3B
#endif
#ifndef CC1101_MARCSTATE
  #define CC1101_MARCSTATE 0x35
#endif
#ifndef CC1101_FIFO
  #define CC1101_FIFO 0x3F
#endif

#ifndef CC1101_WRITE_BURST
  #define CC1101_WRITE_BURST 0x40
#endif
#ifndef CC1101_READ_BURST
  #define CC1101_READ_BURST  0xC0
#endif

// ======================= RFLAB PHASE TYPE (GLOBAL) =======================
enum RFLabPhase2 : uint8_t {
  RFL_MENU = 0,
  RFL_INTRO,
  RFL_QR,
  RFL_L1,
  RFL_L2,
  RFL_L3,
  RFL_L4,
  RFL_L5,
};
// ========================================================================

// ---------- Learning protocol ----------
static const uint8_t RFL_MAGIC = 0xA6;
static const uint8_t RFL_BCAST = 0xFF;

enum RFL_MsgType : uint8_t {
  RFL_BEACON     = 1,
  RFL_FRAME      = 2,
  RFL_FRAME_TEST = 3,
  RFL_REPLAY     = 4,
  RFL_REPLAY_ACK = 5,
};

// ---------- RX frame struct ----------
struct RFL_RxFrame {
  uint8_t payload[32];
  int8_t  rssiDbm;
  uint8_t lqi;
  bool    crcOk;
  uint32_t ms;
};

// ---------- Prototypes (so .ino reorder can't break types) ----------
static void scanHitsSortByRssiDesc();  // forward decl (used by Friends UI)
static bool ccWriteBurst(uint8_t addr, const uint8_t* data, uint8_t len);
static bool ccReadBurst(uint8_t addr, uint8_t* out, uint8_t len);
static void ccFlushRx();
static bool ccSendFixed9(const uint8_t payload9[9]);

static void rflRxService();
static bool rflRxPop(RFL_RxFrame &out);

#endif // RFLAB_HEADER_GUARD
// ======================= END RFLAB HEADER =======================


// Forward decl required functions
static void ccEnterRx();
static void ccApplyProfile(RadioProfile p);
static RadioProfile loadRadioProfile();
static void saveRadioProfile(RadioProfile p);

static LedSet   loadLedSetPrincipal();
static void     saveLedSetPrincipal(LedSet s);
static LedTheme loadLedTheme();
static void     saveLedTheme(LedTheme t);

static bool    loadBuzzerEnabled();
static void    saveBuzzerEnabled(bool e);
static uint8_t loadBuzzerVolume();
static void    saveBuzzerVolume(uint8_t v);

// ======================= PINES =======================
#define TFT_CS    7
#define TFT_RST   0
#define TFT_DC    3
#define TFT_MOSI  6
#define TFT_SCLK  4

#define RF_CS_PIN     10
#define RF_MISO_PIN   5
#define RF_MOSI_PIN   6
#define RF_SCK_PIN    4

#define BUZZER_PIN      21
#define BUZZER_LEDC_RES  8   // 8-bit: 0-255

#define PUSH1PIN  1   // UP
#define PUSH2PIN  2   // DOWN
#define PUSH4PIN  8   // OK
#define PUSH3PIN  9   // BACK

#define LED_PIN     20
#define NUM_LEDS    12
CRGB leds[NUM_LEDS];

#define LED_BRIGHT_MAX_SAFE  50
#define LED_MAX_MA           350

// ======================= CC1101 REGISTROS / STROBES =======================
#define CC1101_PKTLEN       0x06
#define CC1101_PKTCTRL1     0x07
#define CC1101_PKTCTRL0     0x08

#define CC1101_FREQ2        0x0D
#define CC1101_FREQ1        0x0E
#define CC1101_FREQ0        0x0F
#define CC1101_MDMCFG4      0x10
#define CC1101_MDMCFG3      0x11
#define CC1101_MDMCFG2      0x12
#define CC1101_MDMCFG1      0x13
#define CC1101_DEVIATN      0x15

#define CC1101_MCSM1        0x17
#define CC1101_MCSM0        0x18

#define CC1101_RSSI         0x34
#define CC1101_PATABLE      0x3E

#define CC1101_SRES         0x30
#define CC1101_SRX          0x34
#define CC1101_STX          0x35
#define CC1101_SIDLE        0x36
#define CC1101_SFRX         0x3A
#define CC1101_SFTX         0x3B
#define CC1101_SPWD         0x39

// ======================= TFT =======================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ======================= NVS =======================
Preferences prefs;
uint8_t ledBrightness = 20;

// ======================= UI THEMES =======================
enum UiTheme : uint8_t {
  UI_MATRIX = 0,
  UI_RED,
  UI_BLUE,
  UI_WHITE,
  UI_THEME_COUNT
};
static UiTheme uiTheme = UI_MATRIX;
static UiTheme editUiTheme = UI_MATRIX;
static const char* UI_THEME_NAMES[UI_THEME_COUNT] = { "MATRIX", "RED", "BLUE", "WHITE" };

// ======================= COLORES =======================
static const uint16_t COLBLACK   = 0x0000;
static const uint16_t COLWHITE   = 0xFFFF;
static uint16_t COLNEON    = 0x07E0;
static uint16_t COLDIM     = 0x03E0;
static uint16_t COLDARK    = 0x0120;
static const uint16_t COLCYAN    = 0x07FF;
static const uint16_t COLRED     = 0xF800;

// ======================= ESTADO RX =======================
int16_t lastRssiDbm = -120;
static float    foxSmoothedRssi = -120.0f;
static float    foxMaxRssi      = -120.0f;
static uint32_t foxMaxMs        = 0;

// ======================= POWER MGMT =======================
static uint32_t lastActivityMs = 0;
static bool ledsSuspended = false;

static inline void noteActivity(bool any) {
  if (any) lastActivityMs = millis();
}

// ======================= SPI SETTINGS =======================
SPISettings rfSpiSet(1000000, MSBFIRST, SPI_MODE0);

// ======================= BUZZER =======================
static bool    buzzerEnabled = false;
static uint8_t buzzerVolume  = 1;   // 0=Low  1=Medium  2=High

static inline uint8_t _buzzerDuty() {
  if (buzzerVolume == 0) return 2;    // ~0.8%  Low
  if (buzzerVolume == 1) return 6;    // ~2.4%  Medium
  return 16;                           // ~6.3%  High
}

static inline void beep(uint16_t ms) {
  if (!buzzerEnabled) return;
  ledcChangeFrequency(BUZZER_PIN, 1000, BUZZER_LEDC_RES);
  ledcWrite(BUZZER_PIN, _buzzerDuty());
  delay(ms);
  ledcWrite(BUZZER_PIN, 0);
}

static inline void buzzerTone(uint16_t freq) {
  if (!buzzerEnabled || freq == 0) { ledcWrite(BUZZER_PIN, 0); return; }
  ledcChangeFrequency(BUZZER_PIN, freq, BUZZER_LEDC_RES);
  ledcWrite(BUZZER_PIN, _buzzerDuty());
}

static inline void buzzerNoTone() {
  ledcWrite(BUZZER_PIN, 0);
}

static inline int16_t convertRssi(uint8_t raw) {
  int16_t r = raw;
  if (r >= 128) r -= 256;
  return (r / 2) - 74;
}

// ======================= RADIO PARAMS =======================
static RadioProfile radioProfile = PROFILE_FOX_OOK_RSSI;
static uint32_t foxFreq100 = 43390;

// ======================= BUS SPI COMPARTIDO =======================
static inline void busForRF() {
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(RF_CS_PIN, OUTPUT);
  digitalWrite(RF_CS_PIN, HIGH);
}

static inline void busForTFT() {
  pinMode(RF_CS_PIN, OUTPUT);
  digitalWrite(RF_CS_PIN, HIGH);
}

static bool ccSelect() {
  busForRF();
  SPI.beginTransaction(rfSpiSet);
  digitalWrite(RF_CS_PIN, LOW);

  uint32_t t0 = micros();
  while (digitalRead(RF_MISO_PIN) == HIGH) {
    if (micros() - t0 > 3000) {
      digitalWrite(RF_CS_PIN, HIGH);
      SPI.endTransaction();
      return false;
    }
  }
  return true;
}

static void ccDeselect() {
  digitalWrite(RF_CS_PIN, HIGH);
  SPI.endTransaction();
}

static void ccStrobe(uint8_t strobe) {
  if (!ccSelect()) return;
  SPI.transfer(strobe);
  ccDeselect();
}

static void ccWriteReg(uint8_t addr, uint8_t val) {
  if (!ccSelect()) return;
  SPI.transfer(addr);
  SPI.transfer(val);
  ccDeselect();
}

static uint8_t ccReadStatus(uint8_t addr) {
  if (!ccSelect()) return 0;
  uint8_t header = 0xC0 | (addr & 0x3F);
  SPI.transfer(header);
  uint8_t v = SPI.transfer(0);
  ccDeselect();
  return v;
}

static void ccSetPATable(uint8_t pa) {
  if (!ccSelect()) return;
  uint8_t header = 0x40 | (CC1101_PATABLE & 0x3F);
  SPI.transfer(header);
  SPI.transfer(pa);
  ccDeselect();
}

static void ccSetFrequencyMHz100(uint32_t mhz100) {
  uint32_t freq_hz = mhz100 * 10000UL;
  uint32_t reg = (uint32_t)(( (uint64_t)freq_hz << 16 ) / 26000000ULL);

  ccWriteReg(CC1101_FREQ2, (reg >> 16) & 0xFF);
  ccWriteReg(CC1101_FREQ1, (reg >>  8) & 0xFF);
  ccWriteReg(CC1101_FREQ0, (reg >>  0) & 0xFF);
}

// ======================= CC1101 CONFIG =======================
static void ccConfigurePacketGFSK_9B() {
  ccStrobe(CC1101_SRES);
  delay(40);

  ccSetFrequencyMHz100(43300);

  ccWriteReg(CC1101_MDMCFG4, 0x77);
  ccWriteReg(CC1101_MDMCFG3, 0x83);
  ccWriteReg(CC1101_MDMCFG2, 0x12);
  ccWriteReg(CC1101_MDMCFG1, 0x40);
  ccWriteReg(CC1101_DEVIATN, 0x15);

  ccWriteReg(CC1101_PKTLEN, 9);
  ccWriteReg(CC1101_PKTCTRL1, 0x0C);
  ccWriteReg(CC1101_PKTCTRL0, 0x04);

  ccWriteReg(CC1101_MCSM1, 0x0C);
  ccWriteReg(CC1101_MCSM0, 0x18);
  ccWriteReg(0x19, 0x16);
  ccWriteReg(0x1B, 0x43);

  ccSetPATable(0xC0);
  delay(10);
}

static void ccConfigurePacketGFSK_32B() {
  ccStrobe(CC1101_SRES);
  delay(40);

  ccSetFrequencyMHz100(43300);

  ccWriteReg(CC1101_MDMCFG4, 0x77);
  ccWriteReg(CC1101_MDMCFG3, 0x83);
  ccWriteReg(CC1101_MDMCFG2, 0x12);
  ccWriteReg(CC1101_MDMCFG1, 0x40);
  ccWriteReg(CC1101_DEVIATN, 0x15);

  ccWriteReg(CC1101_PKTLEN, 32);
  ccWriteReg(CC1101_PKTCTRL1, 0x0C);
  ccWriteReg(CC1101_PKTCTRL0, 0x04);

  ccWriteReg(CC1101_MCSM1, 0x0C);
  ccWriteReg(CC1101_MCSM0, 0x18);
  ccWriteReg(0x19, 0x16);
  ccWriteReg(0x1B, 0x43);

  ccSetPATable(0xC0);
  delay(10);
}

static void ccConfigureFoxOOK_RssiLive() {
  ccStrobe(CC1101_SRES);
  delay(40);

  ccSetFrequencyMHz100(foxFreq100);

  ccWriteReg(CC1101_MDMCFG4, 0x77);
  ccWriteReg(CC1101_MDMCFG3, 0x83);
  ccWriteReg(CC1101_MDMCFG2, 0x30);
  ccWriteReg(CC1101_MDMCFG1, 0x40);
  ccWriteReg(CC1101_DEVIATN, 0x00);

  ccWriteReg(CC1101_PKTLEN, 0xFF);
  ccWriteReg(CC1101_PKTCTRL1, 0x04);
  ccWriteReg(CC1101_PKTCTRL0, 0x00);

  ccWriteReg(CC1101_MCSM1, 0x0C);
  ccWriteReg(CC1101_MCSM0, 0x18);

  ccSetPATable(0xC0);
  delay(10);
}

static void ccEnterRx() {
  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFRX);
  ccStrobe(CC1101_SRX);
}

static void ccRadioIdle() {
  ccStrobe(CC1101_SIDLE);  
  ccStrobe(CC1101_SFRX);   
  ccStrobe(CC1101_SFTX);  
}

static void ccApplyProfile(RadioProfile p) {
  radioProfile = p;

  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFRX);
  ccStrobe(CC1101_SFTX);

  if (radioProfile == PROFILE_PACKET_GFSK) ccConfigurePacketGFSK_9B();
  else if (radioProfile == PROFILE_FRIENDS_GFSK_32B) ccConfigurePacketGFSK_32B();
  else ccConfigureFoxOOK_RssiLive();

  ccEnterRx();
}

static void ccWake() {
  busForRF();
  digitalWrite(RF_CS_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(RF_CS_PIN, HIGH);
  delayMicroseconds(40);
}

static void rfPowerDown() {
  ccRadioIdle();
  ccStrobe(CC1101_SPWD);
}

static void rfPowerUp() {
  ccWake();                     
  ccApplyProfile(radioProfile);
}


// ======================= RFLAB RF IMPLEMENTATION =======================

static bool ccWriteBurst(uint8_t addr, const uint8_t* data, uint8_t len) {
  if (!ccSelect()) return false;
  SPI.transfer(addr | CC1101_WRITE_BURST);
  for (uint8_t i = 0; i < len; i++) SPI.transfer(data[i]);
  ccDeselect();
  return true;
}

static bool ccReadBurst(uint8_t addr, uint8_t* out, uint8_t len) {
  if (!ccSelect()) return false;
  SPI.transfer(addr | CC1101_READ_BURST);
  for (uint8_t i = 0; i < len; i++) out[i] = SPI.transfer(0x00);
  ccDeselect();
  return true;
}

static void ccFlushRx() {
  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFRX);
  ccStrobe(CC1101_SRX);
}

static bool ccSendFixed9(const uint8_t p[9]) {
  busForRF();
  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFTX);
  ccWriteBurst(CC1101_FIFO, p, 9);
  ccStrobe(CC1101_STX);

  uint32_t t0 = millis();
  while (millis() - t0 < 600) {
    uint8_t st = ccReadStatus(CC1101_MARCSTATE) & 0x1F;
    if (st == 0x01) break;   // IDLE
    if (st == 0x16) break;   // TXFIFO_UNDERFLOW
  }

  uint8_t st2 = ccReadStatus(CC1101_MARCSTATE) & 0x1F;
  if (st2 == 0x16) {
    ccStrobe(CC1101_SIDLE);
    ccStrobe(CC1101_SFTX);
  } else {
    ccStrobe(CC1101_SIDLE);
  }
  ccStrobe(CC1101_SRX);
  return (st2 != 0x16);
}

static bool ccIsChannelFree() {
  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFRX);
  ccStrobe(CC1101_SRX);
  delayMicroseconds(500); // Wait for RSSI valid
  uint8_t rssiRaw = ccReadStatus(CC1101_RSSI);
  int16_t rssiDbm = convertRssi(rssiRaw);
  return rssiDbm < -80; // Free if under -80 dBm
}

static bool ccSendFixed32(const uint8_t p[32]) {
  busForRF();

  // Listen Before Talk (LBT / CSMA)
  uint8_t tries = 0;
  while (!ccIsChannelFree() && tries < 10) {
    delay(10 + (esp_random() % 40));
    tries++;
  }

  ccStrobe(CC1101_SIDLE);
  ccStrobe(CC1101_SFTX);
  ccWriteBurst(CC1101_FIFO, p, 32);
  ccStrobe(CC1101_STX);

  uint32_t t0 = millis();
  while (millis() - t0 < 800) {
    uint8_t st = ccReadStatus(CC1101_MARCSTATE) & 0x1F;
    if (st == 0x01) break;   // IDLE
    if (st == 0x16) break;   // TXFIFO_UNDERFLOW
  }

  uint8_t st2 = ccReadStatus(CC1101_MARCSTATE) & 0x1F;
  if (st2 == 0x16) {
    ccStrobe(CC1101_SIDLE);
    ccStrobe(CC1101_SFTX);
  } else {
    ccStrobe(CC1101_SIDLE);
  }
  ccStrobe(CC1101_SRX);
  return (st2 != 0x16);
}

static void frfSendAckBurst(uint16_t to, uint8_t nonce) {
  uint32_t d0 = 20 + (esp_random() % 60); // 20–79ms
  delay(d0);
  frfSendAckTo(to, nonce);
  delay(70);
  frfSendAckTo(to, nonce);
  delay(70);
  frfSendAckTo(to, nonce);
}

static void frfSendNackBurst(uint16_t to, uint8_t nonce) {
  uint32_t d0 = 20 + (esp_random() % 60);
  delay(d0);
  frfSendNackTo(to, nonce);
  delay(70);
  frfSendNackTo(to, nonce);
  delay(70);
  frfSendNackTo(to, nonce);
}


// RX state (ONLY here)
static volatile bool rfl_rxHas = false;
static RFL_RxFrame   rfl_rxLast;
static const uint8_t RXQ_SZ = 8;
static RFL_RxFrame rxq[RXQ_SZ];
static volatile uint8_t rxq_w = 0, rxq_r = 0;

static inline bool rxq_full()  { return (uint8_t)(rxq_w + 1) % RXQ_SZ == rxq_r; }
static inline bool rxq_empty() { return rxq_w == rxq_r; }

static inline void rflRxPush(const RFL_RxFrame &in) {
  uint8_t next = (uint8_t)(rxq_w + 1) % RXQ_SZ;
  if (next == rxq_r) {
    // cola llena: descarta el más viejo (o descarta el nuevo; aquí descartamos el viejo)
    rxq_r = (uint8_t)(rxq_r + 1) % RXQ_SZ;
  }
  rxq[rxq_w] = in;
  rxq_w = next;
}

static void rflRxService() {
  uint8_t rxRaw = ccReadStatus(CC1101_RXBYTES);
  if (rxRaw & 0x80) {          // overflow
    ccStrobe(CC1101_SIDLE);
    ccStrobe(CC1101_SFRX);
    ccStrobe(CC1101_SRX);
    return;
  }

  uint8_t bytes = rxRaw & 0x7F;
  // Determine packet length based on current profile
  uint8_t pktLen = (radioProfile == PROFILE_FRIENDS_GFSK_32B) ? 32 : 9;
  uint8_t totalBytes = pktLen + 2; // +2 for RSSI and LQI/CRC

  while (bytes >= totalBytes) {
    uint8_t pkt[34];
    if (!ccReadBurst(CC1101_FIFO, pkt, totalBytes)) return;

    RFL_RxFrame f;
    memset(f.payload, 0, 32); // Clear buffer
    memcpy(f.payload, pkt, pktLen);

    uint8_t status0 = pkt[pktLen];          // RSSI
    uint8_t status1 = pkt[pktLen + 1];         // LQI + CRC_OK
    f.crcOk   = (status1 & 0x80) != 0;
    f.lqi     = status1 & 0x7F;
    f.rssiDbm = convertRssi(status0);
    f.ms      = millis();

    rflRxPush(f);                      

    ccStrobe(CC1101_SRX);

    rxRaw = ccReadStatus(CC1101_RXBYTES);
    if (rxRaw & 0x80) {                // overflow mid-drain
      ccStrobe(CC1101_SIDLE);
      ccStrobe(CC1101_SFRX);
      ccStrobe(CC1101_SRX);
      return;
    }
    bytes = rxRaw & 0x7F;
  }
}

static bool rflRxPop(RFL_RxFrame &out) {
  if (rxq_empty()) return false;
  out = rxq[rxq_r];
  rxq_r = (uint8_t)(rxq_r + 1) % RXQ_SZ;
  return true;
}

// ======================= NVS helpers =======================
static uint8_t loadLedBrightness() {
  prefs.begin("hackcon", true);
  uint8_t b = prefs.getUChar("ledbr", 50);
  prefs.end();
  if (b > LED_BRIGHT_MAX_SAFE) b = LED_BRIGHT_MAX_SAFE;
  return b;
}
static void saveLedBrightness(uint8_t b) {
  if (b > LED_BRIGHT_MAX_SAFE) b = LED_BRIGHT_MAX_SAFE;
  prefs.begin("hackcon", false);
  prefs.putUChar("ledbr", b);
  prefs.end();
}
static RadioProfile loadRadioProfile() {
  prefs.begin("hackcon", true);
  uint8_t v = prefs.getUChar("rfprof", (uint8_t)PROFILE_FOX_OOK_RSSI);
  prefs.end();
  if (v > (uint8_t)PROFILE_FOX_OOK_RSSI) v = (uint8_t)PROFILE_FOX_OOK_RSSI;
  return (RadioProfile)v;
}
static void saveRadioProfile(RadioProfile p) {
  prefs.begin("hackcon", false);
  prefs.putUChar("rfprof", (uint8_t)p);
  prefs.end();
}
static uint32_t loadFoxFreq100() {
  prefs.begin("hackcon", true);
  uint32_t v = prefs.getUInt("foxf100", 43390);
  prefs.end();
  if (v < 43000) v = 43000;
  if (v > 44000) v = 44000;
  return v;
}
static void saveFoxFreq100(uint32_t v) {
  prefs.begin("hackcon", false);
  prefs.putUInt("foxf100", v);
  prefs.end();
}
static bool loadBuzzerEnabled() {
  prefs.begin("hackcon", true);
  bool e = prefs.getBool("buzz", true);
  prefs.end();
  return e;
}
static void saveBuzzerEnabled(bool e) {
  prefs.begin("hackcon", false);
  prefs.putBool("buzz", e);
  prefs.end();
}
static uint8_t loadBuzzerVolume() {
  prefs.begin("hackcon", true);
  uint8_t v = prefs.getUChar("buzzVol", 1);
  prefs.end();
  if (v > 2) v = 1;
  return v;
}
static void saveBuzzerVolume(uint8_t v) {
  prefs.begin("hackcon", false);
  prefs.putUChar("buzzVol", v);
  prefs.end();
}

static UiTheme loadUiTheme() {
  prefs.begin("hackcon", true);
  uint8_t t = prefs.getUChar("uitheme", (uint8_t)UI_MATRIX);
  prefs.end();
  if (t >= UI_THEME_COUNT) t = UI_MATRIX;
  return (UiTheme)t;
}
static void saveUiTheme(UiTheme t) {
  prefs.begin("hackcon", false);
  prefs.putUChar("uitheme", (uint8_t)t);
  prefs.end();
}
static void applyUiTheme(UiTheme t) {
  uiTheme = t;
  switch (t) {
    case UI_RED:   COLNEON = 0xF800; COLDIM = 0xB800; COLDARK = 0x5000; break;
    case UI_BLUE:  COLNEON = 0x041F; COLDIM = 0x021F; COLDARK = 0x0010; break;
    case UI_WHITE: COLNEON = 0xFFFF; COLDIM = 0xBDF7; COLDARK = 0x5AEB; break;
    case UI_MATRIX:
    default:       COLNEON = 0x07E0; COLDIM = 0x03E0; COLDARK = 0x0120; break;
  }
}

// ======================= UI buttons edges =======================
static ButtonsEdge readButtonsEdge() {
  static uint8_t p1=HIGH, p2=HIGH, p3=HIGH, p4=HIGH;
  ButtonsEdge b;

  uint8_t n1 = digitalRead(PUSH1PIN);
  uint8_t n2 = digitalRead(PUSH2PIN);
  uint8_t n3 = digitalRead(PUSH3PIN);
  uint8_t n4 = digitalRead(PUSH4PIN);

  b.up   = (p1==HIGH && n1==LOW);
  b.down = (p2==HIGH && n2==LOW);
  b.ok   = (p3==HIGH && n3==LOW);
  b.back = (p4==HIGH && n4==LOW);
  b.any  = b.up || b.down || b.ok || b.back;

  p1=n1; p2=n2; p3=n3; p4=n4;
  return b;
}
static inline bool isPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

// ======================= UI primitives =======================
static void uiFrame() {
  busForTFT();
  tft.drawRect(0, 0, 240, 240, COLNEON);
  tft.drawRect(2, 2, 236, 236, COLDARK);
}
static void uiScanline(uint8_t y, uint16_t col) {
  busForTFT();
  tft.drawFastHLine(4, y, 232, col);
}
static void uiTitle(const char* s) {
  busForTFT();
  tft.fillRect(4, 4, 232, 25, COLBLACK);
  tft.setTextSize(2);
  tft.setTextColor(COLNEON);
  tft.setCursor(10, 10);
  tft.print(s);
  uiScanline(30, COLDARK);
}
static void uiFooter(const char* s) {
  busForTFT();
  tft.fillRect(4, 218, 232, 18, COLBLACK);
  uiScanline(214, COLDARK);
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 222);
  tft.print(s);
}

static void uiFlashStatus(const char* title, const char* msg, uint16_t ms) {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle(title);

  tft.setTextSize(3);
  tft.setTextColor(COLNEON);

  // Center-ish (simple, stable)
  tft.setCursor(14, 110);
  tft.print(msg);

  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    screensaverTick();
    powerLedsTick();
    ledTick(); // Safe here as no manual colors are set
    delay(5);
  }
}
static void uiMsgCenter(const char* s, uint8_t y, uint16_t col, uint8_t sz) {
  busForTFT();
  tft.setTextSize(sz);
  tft.setTextColor(col);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int x = (240 - (int)w) / 2;
  tft.setCursor(x, y);
  tft.print(s);
}

static void uiSmallStatusRX() {
  busForTFT();
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.fillRect(156, 8, 76, 18, COLBLACK);

  tft.setCursor(160, 10);
  tft.print("FREQ ");

  uint32_t mhz = foxFreq100 / 100;
  uint32_t dec = foxFreq100 % 100;
  tft.print(mhz); tft.print(".");
  if (dec < 10) tft.print("0");
  tft.print(dec);
}


#define SAVER_W 240
#define SAVER_H 240

// ======================= SAVER REGISTRY =======================
// Para agregar un nuevo saver: incluir el .h, declarar el array y
// añadir una entrada en SAVERS[]. Nada más.

static const uint8_t saverBits_itachi[] PROGMEM = {
#include "img/itachi_saver.h"
};
static const uint8_t saverBits_id[] PROGMEM = {
#include "img/id_saver.h"
};
static const uint8_t saverBits_nika[] PROGMEM = {
#include "img/nika_saver.h"
};
static const uint8_t saverBits_rickmorty[] PROGMEM = {
#include "img/rickmorty_saver.h"
};
static const uint8_t saverBits_bulbasaur[] PROGMEM = {
#include "img/bulbasaur_saver.h"
};
static const uint8_t saverBits_boot_sp[] PROGMEM = {
#include "img/boot_sp.h"
};


struct SaverDef {
  const char*    name;
  const uint8_t* bits;        // apunta a datos PROGMEM
  bool           showOverlay; // true = mostrar nick/role/id sobre la imagen
  bool           isMono;      // true = dibujar como bitmap 1-bit con colores del badge
};

static const SaverDef SAVERS[] = {
  // name         bits                  overlay  mono
  { "Itachi",     saverBits_itachi,     false, false },
  { "ID Card",    saverBits_id,         true,  true  },
  { "Nika",       saverBits_nika,       false, false },
  { "RickMorty",  saverBits_rickmorty,      false, false },
  { "Bulbasaur",  saverBits_bulbasaur,  false, false },
  { "Sponsors",   saverBits_boot_sp,    false, false },
};

static const uint8_t SAVER_COUNT = sizeof(SAVERS) / sizeof(SAVERS[0]);

static uint8_t selectedSaver = 0;

static uint8_t loadSaverIndex() {
  prefs.begin("hackcon", true);
  uint8_t v = prefs.getUChar("saver", 5);  // default: Sponsors
  prefs.end();
  if (v >= SAVER_COUNT) v = 5;
  return v;
}
static void saveSaverIndex(uint8_t v) {
  if (v >= SAVER_COUNT) v = 0;
  prefs.begin("hackcon", false);
  prefs.putUChar("saver", v);
  prefs.end();
}

// ===== Boot logo bitmap (Color) =====
static const int BOOT_LOGO_W = 140;
static const int BOOT_LOGO_H = 140;

static const uint8_t bootLogoColor[] PROGMEM = {
#include "img/logo.h"
};

static void drawBootLogoCentered() {
  busForTFT();
  tft.fillScreen(COLBLACK);

  const int x = (240 - BOOT_LOGO_W) / 2;
  const int y = (240 - BOOT_LOGO_H) / 2 - 2;

  // Draw the image line by line to correct the byte order (Endianness)
  uint16_t lineBuf[BOOT_LOGO_W];
  for (int i = 0; i < BOOT_LOGO_H; i++) {
    for (int j = 0; j < BOOT_LOGO_W; j++) {
      int idx = (i * BOOT_LOGO_W + j) * 2;
      // The image converter outputs high byte followed by low byte
      uint8_t hi = bootLogoColor[idx];
      uint8_t lo = bootLogoColor[idx + 1];

      // Reconstruct the 16-bit color correctly
      lineBuf[j] = (hi << 8) | lo;
    }
    tft.drawRGBBitmap(x, y + i, lineBuf, BOOT_LOGO_W, 1);
  }
}

// ===== Boot splash (Color, 240x240) =====
static const uint8_t bootSplashColor[] PROGMEM = {
#include "img/boot_sp.h"
};

static void drawBootSplash() {
  busForTFT();
  uint16_t lineBuf[240];
  tft.startWrite();
  tft.setAddrWindow(0, 0, 240, 240);
  for (int i = 0; i < 240; i++) {
    for (int j = 0; j < 240; j++) {
      int idx = (i * 240 + j) * 2;
      uint8_t hi = pgm_read_byte(&bootSplashColor[idx]);
      uint8_t lo = pgm_read_byte(&bootSplashColor[idx + 1]);
      lineBuf[j] = (hi << 8) | lo;
    }
    tft.writePixels(lineBuf, 240);
  }
  tft.endWrite();
}

// ======================= Splash =======================
static void bootSequence() {
  drawBootLogoCentered();
  delay(1500);

  drawBootSplash();
  delay(1500);

  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("HackConRD26");
  tft.setTextSize(1);
  tft.setTextColor(COLNEON);
  tft.setCursor(10, 42);
  tft.print("Initializing RF...");
  delay(260);
}

static void foxHuntPollLiveRssi() {
  if (radioProfile != PROFILE_FOX_OOK_RSSI) return;

  // Poll multiple times to catch pulses
  int16_t currentMax = -127;
  for(int i=0; i<4; i++) {
    ccStrobe(CC1101_SIDLE);
    ccStrobe(CC1101_SFRX);
    ccStrobe(CC1101_SRX);
    delayMicroseconds(500); 
    uint8_t raw = ccReadStatus(CC1101_RSSI);
    int16_t r = convertRssi(raw);
    if (r > currentMax) currentMax = r;
  }
  
  lastRssiDbm = currentMax;

  // Smooth for bar movement
  float alpha = 0.25f;
  if (foxSmoothedRssi < -119) foxSmoothedRssi = (float)lastRssiDbm;
  foxSmoothedRssi = (alpha * (float)lastRssiDbm) + ((1.0f - alpha) * foxSmoothedRssi);

  // Peak hold logic (3 seconds)
  if ((float)lastRssiDbm > foxMaxRssi || (millis() - foxMaxMs > 3000)) {
    foxMaxRssi = (float)lastRssiDbm;
    foxMaxMs = millis();
  }
}

// ===== FastLED show throttle =====
static bool     ledsDirty = false;
static uint32_t ledsLastShowMs = 0;

static const uint16_t LED_SHOW_MIN_MS = 50;

static inline void ledsMarkDirty() {
  ledsDirty = true;
}

static inline void ledsCommitIfNeeded() {
  if (!ledsDirty) return;

  uint32_t now = millis();
  if ((uint32_t)(now - ledsLastShowMs) < LED_SHOW_MIN_MS) return;

  FastLED.show();
  ledsLastShowMs = now;
  ledsDirty = false;
}

// ===== Power Led =====
static void powerLedsTick() {
  const uint32_t IDLE_DIM_MS = 10000;

  bool idle = (millis() - lastActivityMs) > IDLE_DIM_MS;

  if (idle && !ledsSuspended) {
    ledsSuspended = true;

    // Adapt idle brightness based on user's current valid brightness
    uint8_t idleBright = 0;
    if (ledBrightness >= 30) {
      idleBright = 20;
    } else if (ledBrightness >= 20) {
      idleBright = 10;
    } else if (ledBrightness > 0) {
      idleBright = ledBrightness;
    } else {
      idleBright = 0;
    }

    FastLED.setBrightness(idleBright);
    ledsMarkDirty();
    ledsCommitIfNeeded();
  } else if (!idle && ledsSuspended) {
    ledsSuspended = false;
    applyLedBrightness();             
  }
}

// ======================= LED THEMES + LED SETS =======================
enum LedTheme : uint8_t {
  THEME_NEON = 0,
  THEME_RGB,
  THEME_SUNSET,
  THEME_ICE,
  THEME_PARTY,
  THEME_MATRIX,
  THEME_COUNT
};

enum LedSet : uint8_t {
  LEDSET_DRIP = 0,
  LEDSET_COMET_DUAL,
  LEDSET_SPARKLE_MIX,
  LEDSET_PULSE_BARS,
  LEDSET_ORBIT_SAFE,
  LEDSET_COUNT
};

static LedTheme ledTheme = THEME_MATRIX;
static LedSet   ledSetPrincipal = LEDSET_DRIP;
static LedSet   currentLedSet = LEDSET_DRIP;

static const char* LEDSET_NAMES[LEDSET_COUNT] = { "DRIP", "COMET", "SPARK", "PULSE", "ORBIT" };
static const char* THEME_NAMES[THEME_COUNT]   = { "NEON", "RGB", "SUNSET", "FROZEN", "PARTY", "MATRIX" };

static LedTheme loadLedTheme() {
  prefs.begin("hackcon", true);
  uint8_t v = prefs.getUChar("ledth", (uint8_t)THEME_MATRIX);
  prefs.end();
  if (v >= (uint8_t)THEME_COUNT) v = (uint8_t)THEME_MATRIX;
  return (LedTheme)v;
}
static void saveLedTheme(LedTheme t) {
  prefs.begin("hackcon", false);
  prefs.putUChar("ledth", (uint8_t)t);
  prefs.end();
}
static LedSet loadLedSetPrincipal() {
  prefs.begin("hackcon", true);
  uint8_t v = prefs.getUChar("ledset", (uint8_t)LEDSET_DRIP);
  prefs.end();
  if (v >= (uint8_t)LEDSET_COUNT) v = (uint8_t)LEDSET_DRIP;
  return (LedSet)v;
}
static void saveLedSetPrincipal(LedSet s) {
  prefs.begin("hackcon", false);
  prefs.putUChar("ledset", (uint8_t)s);
  prefs.end();
}

static CRGB themeColor(uint8_t idx) {
  if (mode == MODE_FOX_HUNT) {
    // Recalibrated for event: 1-2 meters + backpack attenuation
    // -105 (Blue: nothing) -> -85 (Green: walk closer) -> -55 (Red: You found them!)
    int16_t r = constrain(lastRssiDbm, -105, -45);
    uint8_t hue = (uint8_t)map(r, -105, -45, 160, 0);
    return CHSV(hue + (idx * 5), 255, 255);
  }
  
  idx %= 5;
  switch (ledTheme) {
    case THEME_NEON: {
      static const CRGB c[5] = { CRGB(0,255,80), CRGB(0,180,255), CRGB(180,0,255), CRGB(255,80,0), CRGB(0,255,200) };
      return c[idx];
    }
    case THEME_RGB: {
      static const CRGB c[5] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Magenta };
      return c[idx];
    }
    case THEME_SUNSET: {
      static const CRGB c[5] = { CRGB(255,40,0), CRGB(255,120,0), CRGB(255,0,120), CRGB(140,0,255), CRGB(255,200,60) };
      return c[idx];
    }
    case THEME_ICE: {
      static const CRGB c[5] = { CRGB(0,120,255), CRGB(0,255,200), CRGB(120,200,255), CRGB(0,60,160), CRGB(180,255,255) };
      return c[idx];
    }
    case THEME_PARTY: {
      static const CRGB c[5] = { CRGB(255,0,60), CRGB(0,255,120), CRGB(0,120,255), CRGB(255,220,0), CRGB(180,0,255) };
      return c[idx];
    }
    case THEME_MATRIX: {
      static const CRGB c[5] = { CRGB(0,255,0), CRGB(0,200,40), CRGB(0,140,0), CRGB(0,255,90), CRGB(40,255,0) };
      return c[idx];
    }
    default: return CRGB::Green;
  }
}

static inline void ledsDimAll(uint8_t scale) {
  for (uint8_t i=0;i<NUM_LEDS;i++) leds[i].nscale8(scale);
}

static void ledTickDrip() {
  static uint32_t last=0; static uint8_t pos=0;
  if (millis()-last < 90) return;
  last = millis();

  ledsDimAll(210);

  CRGB head = themeColor(0); head.nscale8_video(220);
  CRGB tail = themeColor(0); tail.nscale8_video(80);

  leds[pos] += head;
  leds[(pos+3) % NUM_LEDS] += tail;

  ledsMarkDirty();
  ledsCommitIfNeeded();
  pos = (pos + 1) % NUM_LEDS;
}

static void ledTickCometDual() {
  static uint32_t last=0; static uint8_t pos=0;
  if (millis()-last < 80) return;
  last = millis();

  ledsDimAll(200);
  CRGB a = themeColor(0); a.nscale8_video(200);
  CRGB b = themeColor(2); b.nscale8_video(170);

  leds[pos] += a;
  leds[(pos + (NUM_LEDS/2)) % NUM_LEDS] += b;

  ledsMarkDirty();
  ledsCommitIfNeeded();
  pos = (pos + 1) % NUM_LEDS;
}

static void ledTickSparkleMix() {
  static uint32_t last=0;
  if (millis()-last < 105) return;
  last = millis();

  ledsDimAll(220);
  uint8_t i = random8(NUM_LEDS);
  CRGB c = themeColor(random8(5));
  c.nscale8_video(200);
  leds[i] += c;

  ledsMarkDirty();
  ledsCommitIfNeeded();
}

static void ledTickPulseBars() {
  static uint32_t last=0; static uint8_t phase=0;
  if (millis()-last < 85) return;
  last = millis();

  uint8_t v = sin8(phase);
  phase += 8;

  for (uint8_t i=0;i<NUM_LEDS;i++) {
    uint8_t g = (i % 3 == 0) ? v : (v / 4);
    CRGB c = themeColor(i);
    c.nscale8_video(g);
    leds[i] = c;
  }
  ledsMarkDirty();
  ledsCommitIfNeeded();
}

static void ledTickOrbitSafe() {
  static uint32_t last=0; static uint8_t pos=0;
  if (millis()-last < 80) return;
  last = millis();

  ledsDimAll(205);

  uint8_t j = (pos + (NUM_LEDS/2)) % NUM_LEDS;

  CRGB c1 = themeColor(1); c1.nscale8_video(210);
  CRGB c2 = themeColor(4); c2.nscale8_video(170);

  leds[pos] += c1;
  leds[j]   += c2;

  if ((pos & 0x03) == 0) {
    for (uint8_t i=0;i<NUM_LEDS;i+=3) {
      CRGB r = themeColor(3);
      r.nscale8_video(90);
      leds[i] += r;
    }
  }

  ledsMarkDirty();
  ledsCommitIfNeeded();
  pos = (pos + 1) % NUM_LEDS;
}

static void ledTick() {
  switch (currentLedSet) {
    case LEDSET_DRIP:        ledTickDrip();       break;
    case LEDSET_COMET_DUAL:  ledTickCometDual();  break;
    case LEDSET_SPARKLE_MIX: ledTickSparkleMix(); break;
    case LEDSET_PULSE_BARS:  ledTickPulseBars();  break;
    case LEDSET_ORBIT_SAFE:  ledTickOrbitSafe();  break;
    default:                 ledTickDrip();       break;
  }

  ledsCommitIfNeeded();
}

static void applyLedBrightness() {
  uint8_t b = ledBrightness;
  if (b > LED_BRIGHT_MAX_SAFE) b = LED_BRIGHT_MAX_SAFE;
  FastLED.setBrightness(b);
  ledsMarkDirty();
  ledsCommitIfNeeded();
}

static LedSet selectLedSetForMode(ScreenMode m) {
  return ledSetPrincipal;
}

// ======================= BADGE PROFILE (Nick + Role) =======================
static const char* BADGE_ROLE_NAMES[5] = { "SCRPTK1D", "H4CK3R", "PHR3AK", "R3DT34M", "PWN3D" };

static char badgeNick[8] = "AAAAAAA";
static BadgeRole badgeRole = ROLE_SCRPTK1D;
static bool isFirstBoot = false;

static char editNick[8];
static uint8_t editNickPos = 0;

static void loadBadgeProfile() {
  prefs.begin("hackcon", true);
  String n = prefs.getString("nick", "");
  uint8_t r = prefs.getUChar("bro", (uint8_t)ROLE_SCRPTK1D);
  prefs.end();

  n.trim();
  if (n.length() == 0) {
    isFirstBoot = true;
    n = "AAAAAAA";
    memset(editNick, 0, sizeof(editNick));
    strlcpy(editNick, "AAAAAAA", sizeof(editNick));
    editNickPos = 0;
  }
  if (n.length() > 7) n = n.substring(0, 7);

  memset(badgeNick, 0, sizeof(badgeNick));
  n.toCharArray(badgeNick, sizeof(badgeNick));

  if (r > (uint8_t)ROLE_PWN3D) r = (uint8_t)ROLE_SCRPTK1D;
  badgeRole = (BadgeRole)r;
}

static void saveBadgeNick(const char* n8) {
  prefs.begin("hackcon", false);
  prefs.putString("nick", String(n8));
  prefs.end();
}

static void saveBadgeRole(BadgeRole r) {
  prefs.begin("hackcon", false);
  prefs.putUChar("bro", (uint8_t)r);
  prefs.end();
}

static uint16_t selfShortId = 0;

static uint16_t calcSelfShortId() {
  uint64_t mac = ESP.getEfuseMac();
  uint16_t p1 = (uint16_t)(mac & 0xFFFF);
  uint16_t p2 = (uint16_t)((mac >> 16) & 0xFFFF);
  uint16_t p3 = (uint16_t)((mac >> 32) & 0xFFFF);
  uint16_t hash = p1 ^ p2 ^ p3;
  if (hash == 0 || hash == 0xFFFF) hash = 0xBEEF;
  return hash;
}

// ======================= BOOT SAFE (staged init) =======================
static const uint16_t BOOT_STABILIZE_MS = 800;   
static const uint16_t BOOT_STAGE_GAP_MS = 180;    

// ======================= Screen Saver variables =======================
static bool systemSuspended = false;    
static bool screensaverDrawn = false;
static ScreenMode modeBeforeSaver = MODE_MENU;

// ======================= FRIENDS STORAGE =======================
#define FRIENDS_MAX 32

struct FriendEntry {
  uint16_t shortId;
  uint8_t  role;
  char     nick[9];
};

static FriendEntry friends[FRIENDS_MAX];
static uint8_t friendsCount = 0;

static void friendsLoad() {
  prefs.begin("hackcon", true);
  size_t sz = prefs.getBytesLength("frblob");
  if (sz == sizeof(FriendEntry) * FRIENDS_MAX + 1) {
    uint8_t tmp[sizeof(FriendEntry) * FRIENDS_MAX + 1];
    prefs.getBytes("frblob", tmp, sz);
    friendsCount = tmp[0];
    if (friendsCount > FRIENDS_MAX) friendsCount = FRIENDS_MAX;
    memcpy(friends, &tmp[1], sizeof(FriendEntry) * FRIENDS_MAX);
  } else {
    friendsCount = 0;
    memset(friends, 0, sizeof(friends));
  }
  prefs.end();
}

static void friendsSave() {
  prefs.begin("hackcon", false);
  uint8_t tmp[sizeof(FriendEntry) * FRIENDS_MAX + 1];
  tmp[0] = friendsCount;
  memcpy(&tmp[1], friends, sizeof(FriendEntry) * FRIENDS_MAX);
  prefs.putBytes("frblob", tmp, sizeof(tmp));
  prefs.end();
}

static int friendsIndexById(uint16_t sid) {
  for (uint8_t i=0;i<friendsCount;i++) if (friends[i].shortId == sid) return (int)i;
  return -1;
}

static bool friendsAdd(uint16_t sid, const char* nick8, uint8_t bro) {
  if (sid == selfShortId) return false;
  if (friendsIndexById(sid) >= 0) return false;
  if (friendsCount >= FRIENDS_MAX) return false;

  FriendEntry &f = friends[friendsCount++];
  f.shortId = sid;

  f.role = broRoleOnly(bro);
  
  memset(f.nick, 0, sizeof(f.nick));
  strlcpy(f.nick, nick8, sizeof(f.nick));

  friendsSave();
  return true;
}

static bool friendsDeleteAt(uint8_t idx) {
  if (idx >= friendsCount) return false;
  for (uint8_t i=idx+1; i<friendsCount; i++) friends[i-1] = friends[i];
  friendsCount--;
  friendsSave();
  return true;
}

// --- story current page cache (para poder re-dibujar al salir del saver) ---
static const char* gStoryTitle = nullptr;
static const char* gStoryBig1  = nullptr;
static const char* gStoryBig2  = nullptr;
static const char* gStorySmall1= nullptr;
static const char* gStorySmall2= nullptr;
static const char* gStoryFooter= nullptr;
static bool        gStoryBeep  = false;

static void storyFoxPageDraw(const char* title,
                            const char* big1,
                            const char* big2,
                            const char* small1,
                            const char* small2,
                            const char* footer,
                            bool doBeep)
{
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle(title);

  const int boxY = 78;
  const int boxH = 90;
  tft.drawRect(10, boxY, 220, boxH, COLDARK);

  int cy = boxY + 24;

  if (big1 && big1[0]) { uiMsgCenter(big1, cy, COLNEON, 2); cy += 22; }
  if (big2 && big2[0]) { uiMsgCenter(big2, cy, COLNEON, 2); cy += 22; }

  if (small1 && small1[0]) { uiMsgCenter(small1, cy, COLDIM, 1); cy += 14; }
  if (small2 && small2[0]) { uiMsgCenter(small2, cy, COLDIM, 1); }

  uiFooter(footer);
  if (doBeep) beep(18);
}

static bool storyWaitOkOrBackWithSaver() {
  bool ignoreNextEdge = false;

  while (true) {
    // === background ticks (misma idea que tu loop) ===
    frfBackgroundTick();

    powerLedsTick();
    ledTick();

    screensaverTick();

    ButtonsEdge b = readButtonsEdge();

    if (systemSuspended) {
      if (b.any) {
        exitScreensaver();
        return false;
      }
      delay(10);
      continue;
    }

    noteActivity(b.any);

    if (ignoreNextEdge && b.any) {
      ignoreNextEdge = false;
      delay(1);
      continue;
    }

    if (b.back) return false;
    if (b.ok)   return true;

    delay(1);
  }
}

static bool storyFoxPage(const char* title,
                         const char* big1,
                         const char* big2,
                         const char* small1,
                         const char* small2,
                         const char* footer,
                         bool doBeep)
{
  gStoryTitle  = title;
  gStoryBig1   = big1;
  gStoryBig2   = big2;
  gStorySmall1 = small1;
  gStorySmall2 = small2;
  gStoryFooter = footer;
  gStoryBeep   = doBeep;

  storyFoxPageDraw(title, big1, big2, small1, small2, footer, doBeep);
  return storyWaitOkOrBackWithSaver();
}

static bool storyFlagPage(const char* title, const char* flagText) {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle(title);
  uiMsgCenter(flagText, 110, COLNEON, 2);
  uiFooter("OK close");

  return storyWaitOkOrBackWithSaver();
}

static void storyFoxIntroShow() {

  // Page 1
  if (!storyFoxPage(
        "CHAPTER I",
        "THE SIGNAL",
        "",
        "At the end of each signal,",
        "you will find a BEARER.",
        "OK next   BACK skip",
        true
      )) return;

  // Page 2
  if (!storyFoxPage(
        "BEARERS",
        "NOT TARGETS",
        "",
        "They carry a piece of truth.",
        "Find them in the venue.",
        "OK start   BACK skip",
        false
      )) return;
}


// ======================= QUEST: ALIEN (Hidden Challenge) =======================
#define QUEST_ALIEN_TRIGGER_FRIENDS  7
#define QUEST_ALIEN_NEED_MIN         7 
#define QUEST_ALIEN_NEED_MAX         7 

static bool     questAlienArmed = false;
static bool     questAlienDone  = false;
static uint8_t questAlienLeft  = 0;

#define QUEST_ALIEN_MAX_NEED  QUEST_ALIEN_NEED_MAX  // siempre sincronizado con NEED_MAX
static uint16_t questAlienSeen[QUEST_ALIEN_MAX_NEED];
static uint8_t  questAlienSeenCount = 0;

// Bit flag en "bro" (byte de role) para marcar un speaker especial como ALIEN.
static const uint8_t BRO_FLAG_ALIEN = 0x80;

#ifndef BADGE_IS_ALIEN
  #define BADGE_IS_ALIEN 1
#endif

static inline uint8_t broEncode(BadgeRole r) {
  uint8_t b = (uint8_t)r;
  if (BADGE_IS_ALIEN) b |= BRO_FLAG_ALIEN; 
  return b;
}

static inline bool broIsAlien(uint8_t bro) { return (bro & BRO_FLAG_ALIEN) != 0; }
static inline uint8_t broRoleOnly(uint8_t bro) { return (bro & 0x07); }

static void questAlienLoad() {
  prefs.begin("hackcon", true);
  questAlienArmed = prefs.getBool("qA_arm", false);
  questAlienDone  = prefs.getBool("qA_done", false);
  questAlienLeft  = prefs.getUChar("qA_left", 0);
  questAlienSeenCount = prefs.getUChar("qA_sc", 0);
  prefs.getBytes("qA_seen", questAlienSeen, sizeof(questAlienSeen));
  prefs.end();

  if (questAlienSeenCount > QUEST_ALIEN_MAX_NEED) questAlienSeenCount = QUEST_ALIEN_MAX_NEED;

  if (questAlienDone) { questAlienArmed = false; questAlienLeft = 0; questAlienSeenCount = 0; }
  if (!questAlienArmed) { questAlienLeft = 0; questAlienSeenCount = 0; }
}

static void questAlienSave() {
  prefs.begin("hackcon", false);
  prefs.putBool("qA_arm", questAlienArmed);
  prefs.putBool("qA_done", questAlienDone);
  prefs.putUChar("qA_left", questAlienLeft);
  prefs.putUChar("qA_sc", questAlienSeenCount);
  prefs.putBytes("qA_seen", questAlienSeen, sizeof(questAlienSeen));
  prefs.end();
}

static void questAlienArmIfNeeded() {
  if (questAlienDone || questAlienArmed) return;
  if (friendsCount < QUEST_ALIEN_TRIGGER_FRIENDS) return;

  questAlienArmed = true;

  uint8_t need = (uint8_t)(QUEST_ALIEN_NEED_MIN + (esp_random() % (QUEST_ALIEN_NEED_MAX - QUEST_ALIEN_NEED_MIN + 1)));
  questAlienLeft = need;

  questAlienSeenCount = 0;
  memset(questAlienSeen, 0, sizeof(questAlienSeen));
  
  questAlienSave();
}

static bool questAlienAlreadyCounted(uint16_t sid) {
  for (uint8_t i=0; i<questAlienSeenCount; i++) {
    if (questAlienSeen[i] == sid) return true;
  }
  return false;
}

static bool questAlienOnAlienAdded(uint16_t sid) {
  if (!questAlienArmed || questAlienDone) return false;
  if (questAlienLeft == 0) return false;

  if (questAlienAlreadyCounted(sid)) return false;

  if (questAlienSeenCount < QUEST_ALIEN_MAX_NEED) {
    questAlienSeen[questAlienSeenCount++] = sid;
  }

  if (questAlienLeft > 0) questAlienLeft--;

  if (questAlienLeft == 0) {
    lastActivityMs = millis();
    questAlienDone = true;
    questAlienArmed = false;
    questAlienSave();
    questAlienShowCompleted();
    goTo(MODE_MENU);
    return true;
  }

  questAlienSave();
  return false;
}

// ======================= FRIENDS HISTORY =======================
static void questAlertEffect() {
  for (int i=0; i<4; i++) {
    for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = CRGB::White;
    ledsMarkDirty();
    ledsCommitIfNeeded();
    
    if (buzzerEnabled) { ledcChangeFrequency(BUZZER_PIN, 1000, BUZZER_LEDC_RES); ledcWrite(BUZZER_PIN, _buzzerDuty()); }

    uint32_t t0 = millis();
    while (millis() - t0 < 80) {
      screensaverTick();
      powerLedsTick();
      // ledTick(); // Overwrites alert color
      delay(1);
    }

    for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = CRGB::Black;
    ledsMarkDirty();
    ledsCommitIfNeeded();

    if (buzzerEnabled) ledcWrite(BUZZER_PIN, 0);
    
    t0 = millis();
    while (millis() - t0 < 80) {
      screensaverTick();
      powerLedsTick();
      // ledTick(); // Overwrites alert color
      delay(1);
    }
  }

  // Small delay after the effect so it doesn't clash with the story prompt beep
  uint32_t tEnd = millis();
  while (millis() - tEnd < 600) {
    screensaverTick();
    powerLedsTick();
    delay(1);
  }
}

static void questAlienShowUnlocked() {
  questAlertEffect();

  // Page 1
  if (!storyFoxPage(
        "CHAPTER II",
        "THE SECRET",
        "",
        "A researcher forged a KEY,",
        "then vanished without a trace.",
        "OK next   BACK skip",
        true
      )) return;

  // Page 2
  if (!storyFoxPage(
        "CAESARS",
        "FIND HIM",
        "",
        "He blends in with the crowd.",
        "Add him to your Friends list.",
        "OK next   BACK skip",
        false
      )) return;

  // Page 3
  if (!storyFoxPage(
        "REMEMBER",
        "NO HINTS",
        "",
        "He will not explain anything.",
        "Keep what you gain.",
        "OK next   BACK skip",
        false
      )) return;

  // Page 4 (FLAG)
  if (!storyFlagPage("FLAG 1 DISCOVERED", "HCRD{B4DGE-S3RC3T}")) return;
}

static void questAlienShowCompleted() {
  questAlertEffect();

  // Page 1
  if (!storyFoxPage(
        "FOUND",
        "CAESARS",
        "",
        "You found Caesars.",
        "He handed you the KEY.",
        "OK next   BACK skip",
        true
      )) return;

  // Page 2
  if (!storyFoxPage(
        "UNLOCKED",
        "KEY ACQUIRED",
        "",
        "A new module is now available",
        "in the Main Menu.",
        "OK next   BACK skip",
        false
      )) return;

  // Page 3 (FLAG)
  if (!storyFlagPage("FLAG 2 DISCOVERED", "HCRD{CA3S4RS-F0UND}")) return;
}

static void questKeyShowIntro() {

  // Page 1
  if (!storyFoxPage(
        "CHAPTER III",
        "THE KEY",
        "",
        "A cipher was left behind",
        "to protect the BOX.",
        "OK next   BACK skip",
        true
      )) return;

  // Page 2
  if (!storyFoxPage(
        "RULES",
        "ONE TRY",
        "",
        "Enter the six digits.",
        "Hold OK to submit.",
        "OK next   BACK skip",
        false
      )) return;

  // Page 3
  if (!storyFoxPage(
        "NOTICE",
        "NO SHARING",
        "",
        "What unlocks here belongs",
        "to the one who earned it.",
        "OK close   BACK skip",
        false
      )) return;
}

static void storyBoxIntroShow() {

  // Page 1
  if (!storyFoxPage(
        "CHAPTER IV",
        "THE BOX",
        "",
        "A sealed container was found",
        "behind the venue walls.",
        "OK next   BACK skip",
        true
      )) return;

  // Page 2
  if (!storyFoxPage(
        "WARNING",
        "DO NOT OPEN",
        "",
        "It reacts to nearby signals.",
        "Bring the right KEY.",
        "OK close   BACK skip",
        false
      )) return;
}

// ======================= RF FRIENDS (CC1101 Packet) =======================
// Transport: CC1101 fixed 9-byte packets (PROFILE_PACKET_GFSK). No BLE/NimBLE.
// Nickname exchange happens post-handshake (PROFILE frames). Nickname storage/UI is unchanged.

static constexpr uint8_t FRF_MAGIC = 0xF6;

enum FriendsPktType : uint8_t {
  FRF_HELLO   = 1,   // broadcast "I'm here" (no badgeNick)
  FRF_REQ     = 2,   // request add
  FRF_ACK     = 3,   // accept
  FRF_NACK    = 4,   // reject/busy
};

// ---- Scan cache (used by Friends Scan UI) ----
static constexpr uint8_t SCAN_HITS_MAX = 32;

struct ScanHit {
  uint16_t id = 0;
  uint8_t  bro = 0;
  char     nick[9] = {0};
  int8_t   rssiDbm = -127;
  uint32_t lastSeenMs = 0;
};

static ScanHit scanHits[SCAN_HITS_MAX];
static uint8_t scanHitsCount = 0;
static uint8_t frfScanIndex = 0;
static uint32_t frfScanEndMs = 0;

// ---- Reset flags para statics locales en loop() (BUG3/BUG4 fix) ----
static bool frfScanNeedReset = false;
static bool frfListNeedReset = false;
static bool frfReqNeedReset  = false;

static int scanHitFind(uint16_t id) {
  for (uint8_t i=0; i<scanHitsCount; i++) if (scanHits[i].id == id) return (int)i;
  return -1;
}

static void friendsScanClear(uint32_t durationMs) {
  scanHitsCount = 0;
  frfScanIndex = 0;
  frfScanEndMs = millis() + durationMs;
  for (uint8_t i=0; i<SCAN_HITS_MAX; i++) {
    scanHits[i] = ScanHit();
  }
}

static void friendsScanNick(uint16_t id, const char* nick8) {
  const int idx = scanHitFind(id);
  if (idx < 0) return;
  strncpy(scanHits[idx].nick, nick8 ? nick8 : "", 8);
  scanHits[idx].nick[8] = 0;
}

extern bool scanPhaseCollecting;

static void friendsScanSeen(uint16_t id, uint8_t bro, int8_t rssiDbm = -127) {
  if (!scanPhaseCollecting) return;
  const uint32_t now = millis();
  int idx = scanHitFind(id);
  if (idx < 0) {
    if (scanHitsCount >= SCAN_HITS_MAX) return;
    idx = (int)scanHitsCount++;
    scanHits[idx].id = id;
    scanHits[idx].bro = bro;
    scanHits[idx].nick[0] = 0;

    // If already friends, copy nick immediately.
    const int fidx = friendsIndexById(id);
    if (fidx >= 0) { strncpy(scanHits[idx].nick, friends[fidx].nick, 8); scanHits[idx].nick[8] = 0; }  // BUG7 FIX
  }
  scanHits[idx].bro = bro;
  scanHits[idx].rssiDbm = rssiDbm;
  scanHits[idx].lastSeenMs = now;
}

// ---- RF packet build/parse ----
// Layout (9 bytes):
//  [0] magic
//  [1] type
//  [2] a (nonce or seq)
//  [3] bro (role encoding)   (PROFILE: payload byte 0)
//  [4..5] from (LE)          (PROFILE: payload byte 1..2)
//  [6..7] to   (LE)          (PROFILE: payload byte 3..4)
//  [8] data                  (PROFILE: payload byte 5)
//
// Note: PROFILE repurposes bytes [3..8] as 6 payload bytes (two chunks carry badgeNick[8]).

static void frfBuild(uint8_t out[32], uint8_t t, uint16_t from, uint16_t to,
                     uint8_t a, uint8_t bro, uint8_t data) {
  memset(out, 0, 32);
  out[0] = FRF_MAGIC;
  out[1] = (uint8_t)t;
  out[2] = a;
  out[3] = bro;
  out[4] = (uint8_t)(from & 0xFF);
  out[5] = (uint8_t)(from >> 8);
  out[6] = (uint8_t)(to & 0xFF);
  out[7] = (uint8_t)(to >> 8);
  // data is now unused but kept in sig for backwards compat, put in out[8] 
  out[8] = data;
  
  // Copy badgeNick (char[8]) to out[9..16]; zero-fill the remaining 6 bytes
  memcpy((char*)&out[9], badgeNick, 8);
  memset((char*)&out[17], 0, 6);
}

static bool frfParse(const uint8_t p[32], uint8_t &t, uint16_t &from, uint16_t &to,
                     uint8_t &a, uint8_t &bro, uint8_t &data, char nickOut[15] = nullptr) {
  if (p[0] != FRF_MAGIC) return false;
  t = (uint8_t)p[1];
  a = p[2];
  bro = p[3];
  from = (uint16_t)p[4] | ((uint16_t)p[5] << 8);
  to   = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
  data = p[8];
  
  if (nickOut) {
    strncpy(nickOut, (const char*)&p[9], 14);
    nickOut[14] = 0;
  }
  return true;
}

// ---- RX bridging (reuse existing CC1101 RX ring) ----

static void frfRxService() { rflRxService(); }

static bool frfRxPop(RFL_RxFrame &out) {
  return rflRxPop(out);
}

// ---- Request/prompt state ----
static uint32_t frfHelloNextMs = 0;

static uint16_t lastIncomingReqFrom = 0;
static uint32_t incomingReqIgnoreUntil = 0;

static ScreenMode modeBeforePrompt = MODE_MENU;
static bool     frfSeenReq = false;
static uint32_t frfReqPromptStartMs = 0;
static uint16_t frfReqFrom = 0;
static bool frfSeenAck  = false;
static uint16_t frfAckFrom = 0;
static bool frfSeenNack = false;
static uint16_t frfNackFrom = 0;
static uint8_t  frfReqNonce = 0;
static uint8_t  frfReqBro = 0;
static char     frfReqNick[9] = {0};
static uint16_t frfOutReqTo = 0;
static uint8_t  frfOutReqNonce = 0;



// ---- TX helpers ----
static inline uint8_t frfMakeNonce() {
  uint8_t n = (uint8_t)(esp_random() & 0xFF);
  return (n == 0) ? 1 : n;
}

static void frfSend(const uint8_t p[32]) {
  if (radioProfile != PROFILE_FRIENDS_GFSK_32B) return;
  ccSendFixed32(p);
}

static void frfSendHello() {
  uint8_t p[32];
  frfBuild(p, FRF_HELLO, selfShortId, 0xFFFF, 0, broEncode(badgeRole), 0);
  frfSend(p);
}

static void frfSendReqTo(uint16_t to, uint32_t /*ms*/ = 0) {
  if (frfOutReqTo != to || frfOutReqNonce == 0) {
    frfOutReqTo = to;
    frfOutReqNonce = frfMakeNonce();
  }

  uint8_t p[32];
  frfBuild(p, FRF_REQ, selfShortId, to, frfOutReqNonce, broEncode(badgeRole), 0);
  frfSend(p);
}

static void frfSendAckTo(uint16_t to, uint8_t nonce) {
  uint8_t p[32];
  frfBuild(p, FRF_ACK, selfShortId, to, nonce, broEncode(badgeRole), 0);
  frfSend(p);
}

static void frfSendNackTo(uint16_t to, uint8_t nonce) {
  uint8_t p[32];
  frfBuild(p, FRF_NACK, selfShortId, to, nonce, broEncode(badgeRole), 0);
  frfSend(p);
}
// ---- TX helpers ----

// ---- Hello scheduler ----
static void frfEnsureHello() {
  if (radioProfile != PROFILE_FRIENDS_GFSK_32B) return;
  const uint32_t now = millis();
  if ((int32_t)(now - frfHelloNextMs) < 0) return;
  frfHelloNextMs = now + 1500 + (uint32_t)(esp_random() % 500);
  frfSendHello();
}

// ---- Background processing ----
static void frfBackgroundTick() {
  frfRxService();

  RFL_RxFrame fr;
  while (frfRxPop(fr)) {
    uint8_t t; uint16_t from, to; uint8_t a, bro, data;
    char rxNick[15] = {0};
    if (!frfParse(fr.payload, t, from, to, a, bro, data, rxNick)) continue;

    if (to != 0xFFFF && to != selfShortId) continue;
    if (from == 0 || from == selfShortId) continue;

    if (t == FRF_HELLO) {
      if (fr.rssiDbm >= -70) { // Only show badges that are close! LBT proximity filter
        friendsScanSeen(from, bro, fr.rssiDbm);
        if (rxNick[0]) friendsScanNick(from, rxNick);
      }
      continue;
    }

    if (t == FRF_REQ) {
      const uint32_t now = millis();
      if (from == lastIncomingReqFrom && (int32_t)(now - incomingReqIgnoreUntil) < 0) continue;

      if (friendsIndexById(from) >= 0) {
        frfSendAckTo(from, a);
        continue;
      }

      // BUG2 FIX: rechazar si ya estamos manejando un prompt, esperando ACK, o ya hay un REQ pendiente
      if (mode == MODE_FRIEND_REQ_PROMPT || mode == MODE_FRIENDS_REQUESTING) {
        frfSendNackTo(from, a);
        continue;
      }

      // BUG3 FIX: si ya hay un REQ pendiente sin resolver, rechazar el nuevo
      if (frfSeenReq) {
        frfSendNackTo(from, a);
        continue;
      }

      frfSeenReq = true;
      frfReqPromptStartMs = millis();
      frfReqFrom = from;
      frfReqNonce = a;
      frfReqBro = bro;

      // Use the nick directly from the request frame
      frfReqNick[0] = 0;
      if (rxNick[0]) {
          strncpy(frfReqNick, rxNick, 8);
          frfReqNick[8] = 0;
      } else {
        // fallback: short id as hex (no UI changes, just data)
        snprintf(frfReqNick, sizeof(frfReqNick), "%04X", (unsigned)from);
      }

      lastIncomingReqFrom = from;
      incomingReqIgnoreUntil = now + 4000;

      modeBeforePrompt = mode;
      goTo(MODE_FRIEND_REQ_PROMPT);
      continue;
    }    if (t == FRF_ACK || t == FRF_NACK) {
      if (mode != MODE_FRIENDS_REQUESTING) continue;
      if (from != frfOutReqTo) continue;
      if (a != frfOutReqNonce) continue;

      if (t == FRF_ACK) {
        frfSeenAck = true;
        frfAckFrom = from;
      } else {
        frfSeenNack = true;
        frfNackFrom = from;
      }
      continue;
    }
  }

  // Outgoing request retry handled entirely in loop() via resendIntervals
}

// =====================================================
// RF LAB (Learning Module) - GFSK Learning Network (9B)
// =====================================================

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

// ---------- Self ID (1 byte) ----------
static inline uint8_t rflSelfId() {
  return selfShortId;
}

static uint8_t rflTxCounter = 0;

static void rflBuildFrame(uint8_t out9[9], uint8_t lesson, RFL_MsgType type,
                          uint8_t from, uint8_t target, uint8_t ctr,
                          bool stateful, uint8_t data) {
  out9[0] = RFL_MAGIC;
  out9[1] = lesson;
  out9[2] = (uint8_t)type;
  out9[3] = from;
  out9[4] = target;
  out9[5] = ctr;
  out9[6] = stateful ? 0x01 : 0x00;
  out9[7] = data;
  out9[8] = 0x00;
}

static bool rflTx(uint8_t lesson, RFL_MsgType type, uint8_t target, bool stateful, uint8_t data,
                  uint8_t *outSent9 = nullptr) {
  uint8_t p[9];
  uint8_t ctr = rflTxCounter++;
  rflBuildFrame(p, lesson, type, rflSelfId(), target, ctr, stateful, data);
  if (outSent9) memcpy(outSent9, p, 9);
  return ccSendFixed9(p);
}

// =====================================================
// TX scheduler (non-blocking UX): queue + repeats + gaps
// =====================================================

struct RFL_TxReq {
  uint8_t payload[9];
  uint8_t repeats = 1;
  uint16_t gapMs = 0;
  uint8_t remaining = 0;
  uint32_t nextMs = 0;
  bool active = false;
};

static const uint8_t RFL_TXQ_MAX = 6;
static RFL_TxReq rfl_txq[RFL_TXQ_MAX];
static uint8_t rfl_txqHead = 0;
static uint8_t rfl_txqTail = 0;
static uint32_t rfl_txCooldownUntil = 0;

static inline bool rflTxqEmpty() { return rfl_txqHead == rfl_txqTail; }
static inline uint8_t rflTxqNext(uint8_t i) { return (uint8_t)((i + 1) % RFL_TXQ_MAX); }

static void rflTxqClear() {
  rfl_txqHead = rfl_txqTail = 0;
}

// Coalesce policy: for the same (lesson,type,from,target) keep only the newest payload.
static bool rflTxqCoalesce(const uint8_t p[9], uint8_t repeats, uint16_t gapMs) {
  // key = bytes 1..4 (lesson,type,from,target)
  uint8_t i = rfl_txqHead;
  while (i != rfl_txqTail) {
    if (rfl_txq[i].active &&
        rfl_txq[i].payload[1] == p[1] &&
        rfl_txq[i].payload[2] == p[2] &&
        rfl_txq[i].payload[3] == p[3] &&
        rfl_txq[i].payload[4] == p[4]) {
      memcpy(rfl_txq[i].payload, p, 9);
      rfl_txq[i].repeats = repeats;
      rfl_txq[i].gapMs = gapMs;
      if (rfl_txq[i].remaining == 0) rfl_txq[i].remaining = repeats;
      return true;
    }
    i = rflTxqNext(i);
  }
  return false;
}

static bool rflTxEnqueuePayload(const uint8_t p[9], uint8_t repeats, uint16_t gapMs) {
  if (rflTxqCoalesce(p, repeats, gapMs)) return true;

  uint8_t nextTail = rflTxqNext(rfl_txqTail);
  if (nextTail == rfl_txqHead) {
    // queue full -> drop oldest
    rfl_txqHead = rflTxqNext(rfl_txqHead);
  }

  RFL_TxReq &q = rfl_txq[rfl_txqTail];
  memcpy(q.payload, p, 9);
  q.repeats = repeats;
  q.gapMs = gapMs;
  q.remaining = repeats;
  q.nextMs = millis();
  q.active = true;

  rfl_txqTail = nextTail;
  return true;
}

static bool rflTxRequest(uint8_t lesson, RFL_MsgType type, uint8_t target,
                         bool stateful, uint8_t data,
                         uint8_t repeats = 1, uint16_t gapMs = 0) {
  uint8_t p[9];
  uint8_t ctr = rflTxCounter++;
  rflBuildFrame(p, lesson, type, rflSelfId(), target, ctr, stateful, data);
  return rflTxEnqueuePayload(p, repeats, gapMs);
}

// For replay: enqueue an already-built 9B frame (e.g., captured packet).
static bool rflTxRequestRaw9(const uint8_t payload9[9], uint8_t repeats = 1, uint16_t gapMs = 0) {
  return rflTxEnqueuePayload(payload9, repeats, gapMs);
}

static void rflTxTick() {
  uint32_t now = millis();
  if (now < rfl_txCooldownUntil) return;
  if (rflTxqEmpty()) return;

  RFL_TxReq &q = rfl_txq[rfl_txqHead];
  if (!q.active) { rfl_txqHead = rflTxqNext(rfl_txqHead); return; }
  if (now < q.nextMs) return;

  // Send 1 packet, then schedule next (stay RX-dominant).
  ccSendFixed9(q.payload);

  if (q.remaining > 0) q.remaining--;
  if (q.remaining == 0) {
    q.active = false;
    rfl_txqHead = rflTxqNext(rfl_txqHead);
    rfl_txCooldownUntil = now + 35;
  } else {
    q.nextMs = now + q.gapMs;
    rfl_txCooldownUntil = now + 20;
  }
}


static RFLabPhase2 rflab = RFL_MENU;

static bool downHoldDetect(uint16_t holdMs) {
  static uint32_t t0 = 0;
  if (isPressed(PUSH2PIN)) {              // DOWN
    if (t0 == 0) t0 = millis();
    if (millis() - t0 >= holdMs) { t0 = 0; return true; }
  } else {
    t0 = 0;
  }
  return false;
}

// Layout 240x240 (match Main Menu box size)
static const int BOX_X = 10;
static const int BOX_Y = 42;
static const int BOX_W = 220;
static const int BOX_H = 160;

static inline int C_X() { return BOX_X + 8; }
static inline int C_Y() { return BOX_Y + 8; }
static inline int C_W() { return BOX_W - 16; }
static inline int C_H() { return BOX_H - 16; }

static inline int PANEL_X() { return C_X() + 8; }
static inline int PANEL_Y() { return C_Y() + 44; }
static inline int PANEL_W() { return C_W() - 16; }
static inline int PANEL_H() { return 92; }

static void rflPrintAt(int x, int y, uint8_t sz, uint16_t col, const char* s) {
  busForTFT();
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setCursor(x, y);
  tft.print(s);
}

static void rflDrawHeader(const char* title) {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle(title);
}

static void rflDrawBoxFrame() {
  busForTFT();
  tft.drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, COLDARK);
  tft.fillRect(BOX_X+1, BOX_Y+1, BOX_W-2, BOX_H-2, COLBLACK);
}

static void rflDrawPanel() {
  busForTFT();
  tft.drawRoundRect(PANEL_X(), PANEL_Y(), PANEL_W(), PANEL_H(), 10, COLDARK);
  tft.fillRoundRect(PANEL_X()+1, PANEL_Y()+1, PANEL_W()-2, PANEL_H()-2, 10, COLBLACK);
}

static void rflTitleSubInBox(const char* title, const char* subtitle) {
  rflPrintAt(C_X()+4, C_Y()+2, 2, COLNEON, title);
  if (subtitle && subtitle[0]) {
    rflPrintAt(C_X()+4, C_Y()+24, 1, COLDIM, subtitle);
  }
}

// Overload: permite llamar solo con title
static void rflTitleSubInBox(const char* title) {
  rflTitleSubInBox(title, nullptr);
}

// =====================================================
// 2) Peer detection + last RX decoded
// =====================================================


static bool     rflHaveRx = false;
static uint8_t  rflLastFlag = 0; // Previamente llamado rflLastLesson
static uint8_t  rflLastType = 0;
static uint8_t  rflLastFrom = 0;
static uint8_t  rflLastTarget = 0;
static uint8_t  rflLastCtr = 0;
static int16_t  rflLastRssi = -120;
static uint8_t  rflLastLqi  = 0;
static bool rflStaticDrawn = false;
static RFLabPhase2 rflPrev = (RFLabPhase2)0xFF;

static bool l1_txMode = false;

static uint32_t rflLastDrawMs = 0;
static const uint16_t RFL_DRAW_MS = 240;

// Variables de interacción gamificada para la inyección L1
static bool l1_txActive = false;
static uint32_t l1_txActiveMs = 0;

// --- L1 TX mode params (igual al masterService) ---
static const uint8_t  L1_TXREPEATS = 4;
static const uint16_t L1_TXGAPMS   = 110;  
static const uint16_t L1_TXRATE_MS = 600;

static void l1ApplyMode() {
  busForRF();
  if (l1_txMode) {
    // TX-only: salir de RX para separar RX vs TX de verdad
    ccStrobe(CC1101_SIDLE);
  } else {
    // RX/SCAN: mantener RX activo
    ccStrobe(CC1101_SRX);
    rflScanInit();
  }
}

// =====================================================
// 4) Lesson 3 stats + Lesson 4 capture + Lesson 5 anti-replay
// =====================================================

// L3
static uint32_t l3_rxOk = 0;
static uint32_t l3_missWin = 0;
static uint32_t l3_lastRxMs = 0;

// L4 capture
static const uint8_t CAP_MAX = 4;
static RFL_RxFrame capBuf[CAP_MAX];
static uint8_t capLen = 0;
static bool capArmed = false;
static uint32_t capArmMs = 0;
static const uint32_t CAP_WIN_MS = 6000;

// Selection in capture list
static uint8_t capSel = 0;
static bool capFrozen = false;

// L3 verdict display (TX)
static char l3_crcMsg[12] = "----"; // "TX OK" / "TX COMP"

// L5

// L5 access control UI
static uint32_t l5_accessMs = 0;
static char l5_accessLine1[20] = "";
static char l5_accessLine2[20] = ""; // optional FLAG line
static uint8_t l5_target_id = 0;

static void rflResetL5(bool preserveCapture) {
  if (!preserveCapture) {
    capLen = 0;
    capSel = 0;
    capFrozen = false;
  }

  // L5 NO captura en vivo
  capArmed = false;

  // Limpiar UI de acceso para que NO muestre nada al entrar
  l5_accessLine1[0] = 0;
  l5_accessLine2[0] = 0;
  l5_accessMs = 0;


  rflLastType = 0;
  rflLastFrom = 0;
  rflLastTarget = 0;

  delay(100);
}

static void l5AccessEffect(bool granted) {
  for (int i=0; i<3; i++) {
    for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = granted ? CRGB::Green : CRGB::Red;
    ledsMarkDirty(); ledsCommitIfNeeded();
    if (buzzerEnabled) { ledcChangeFrequency(BUZZER_PIN, granted ? 1200 : 400, BUZZER_LEDC_RES); ledcWrite(BUZZER_PIN, _buzzerDuty()); }

    uint32_t tOk = granted ? 60 : 120;
    uint32_t t0 = millis();
    while(millis() - t0 < tOk) {
      screensaverTick();
      powerLedsTick();
      // ledTick();
      delay(1);
    }

    for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = CRGB::Black;
    ledsMarkDirty(); ledsCommitIfNeeded();
    if (buzzerEnabled) ledcWrite(BUZZER_PIN, 0);
    
    t0 = millis();
    while(millis() - t0 < 60) {
      screensaverTick();
      powerLedsTick();
      // ledTick();
      delay(1);
    }
  }
}

static bool rflParse(const RFL_RxFrame &f) {
  // Allow corrupted frames to be observed (learning/capture),
  // but ignore anything that doesn't look like our protocol.
  if (f.payload[0] != RFL_MAGIC) return false;

  rflLastFlag     = f.payload[1];
  rflLastType     = f.payload[2];
  rflLastFrom     = f.payload[3];
  rflLastTarget   = f.payload[4];
  rflLastCtr      = f.payload[5];


  rflLastRssi     = f.rssiDbm;
  rflLastLqi      = f.lqi;
  rflHaveRx       = true;

  // L3 verdict: normal TX vs replay (compromised)
    // L3 verdict: TX OK vs compromised (replay)
  strncpy(l3_crcMsg, (f.payload[2] == (uint8_t)RFL_REPLAY) ? "TX COMP" : "TX OK", sizeof(l3_crcMsg));
  if (capArmed && !capFrozen) {
    if (capLen < CAP_MAX) {
      capBuf[capLen++] = f;
      if (capSel >= capLen) capSel = capLen - 1;
      if (capLen >= CAP_MAX) capFrozen = true;
    }
  }


  // Receiver logic (Victim): detect replay while in L4
  if (rflab == RFL_L4) {
    if (rflLastType == RFL_REPLAY) {
      uint8_t self = rflSelfId();
      bool ok = (rflLastFrom == self); 
      if (ok) {
        strncpy(l5_accessLine1, "ACCESS GRANTED", sizeof(l5_accessLine1));
        l5_accessLine2[0] = 0; 
        l5_accessMs = millis();
        l5AccessEffect(true);

        // Inform the attacker that the replay was successful by broadcasting an ACK burst
        rflTxRequest(4, RFL_REPLAY_ACK, RFL_BCAST, false, 0x00, 4, 80);
      } else {
        strncpy(l5_accessLine1, "ACCESS DENIED", sizeof(l5_accessLine1));
        l5_accessLine2[0] = 0;
        l5_accessMs = millis();
        l5AccessEffect(false); // <--- Agregado el efecto para el badge espectador
      }
    }
  }

  // Attacker logic (Attacker): listen for the victim's reaction in L5
  if (rflab == RFL_L5) {
     if (rflLastType == RFL_REPLAY_ACK && l5_accessLine1[0] == 'W') { 
        // Verificamos que el ACK sea genuinamente de la placa a la que intentamos hacer Replay
        if (rflLastFrom == l5_target_id) {
          strncpy(l5_accessLine1, "ACCESS GRANTED", sizeof(l5_accessLine1));
          strncpy(l5_accessLine2, "CONGRATS!", sizeof(l5_accessLine2));
          l5_accessMs = millis();
          l5AccessEffect(true);
        }
     }
  }

  return true;
}

static void rflRxTick() {
  rflRxService();

  RFL_RxFrame f;
  while (rflRxPop(f)) {
    rflParse(f);
  }

}

// =====================================================
// 3) Intro
// =====================================================

static const char* RFLAB_GUIDE_URL = "https://hackconrd.org/";

static void rflDrawIntroScreen() {
  const int px = PANEL_X() + 10;
  const int py = PANEL_Y() + 10;
  const int pw = PANEL_W() - 20;
  const int ph = PANEL_H() - 20;

  busForTFT();
  tft.fillRect(px, py, pw, ph, COLBLACK);

  const char* msg =
    "This is a guided learning.\n"
    "To complete it, open the\n"
    "following web guide.";

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);

  const int charW = 6;
  const int charH = 8;

  int y = py + 2;
  const char* ptr = msg;
  while (*ptr) {
    char line[64];
    int i = 0;
    while (*ptr && *ptr != '\n' && i < 63) line[i++] = *ptr++;
    line[i] = 0;
    if (*ptr == '\n') ptr++;

    int lineW = (int)strlen(line) * charW;
    int x = PANEL_X() + (PANEL_W() - lineW) / 2;

    tft.setCursor(x, y);
    tft.print(line);
    y += charH + 4;
  }
}

static void rflDrawQrScreen(const char* text) {
  const int px = PANEL_X() + 10;
  const int py = PANEL_Y() + 10;
  const int pw = PANEL_W() - 20;
  const int ph = PANEL_H() - 20;

  busForTFT();
  tft.fillRect(px, py, pw, ph, COLBLACK);

  QRCode qrcode;
  const uint8_t version = 4;
  const uint8_t ecc = 0;

  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  qrcode_initText(&qrcode, qrcodeData, version, ecc, text);

  const int size = qrcode.size;

  int scale = min(pw / size, ph / size);
  if (scale < 3) scale = 3;

  const int qrPixelSize = size * scale;
  const int ox = px + (pw - qrPixelSize) / 2;
  const int oy = py + (ph - qrPixelSize) / 2 - 16;

  for (uint8_t y = 0; y < size; y++) {
    for (uint8_t x = 0; x < size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(ox + x * scale, oy + y * scale, scale, scale, COLNEON);
      }
    }
  }
}

// =====================================================
// 3) Lesson 1: Spectrum scan (live)
// =====================================================
static const uint8_t  SCAN_BINS = 20;
static int16_t  scanRssiDbm[SCAN_BINS];
static uint8_t  scanPos = 0;

static const uint32_t SCAN_START_HZ = 433720000UL;
static const uint32_t SCAN_STEP_HZ  = 20000UL;

// Tu firmware ya tenía esto: ccSetFrequencyMHz100(mhz*100)
// Si se llama distinto, cambia SOLO esta declaración/llamada.
extern void ccSetFrequencyMHz100(uint32_t mhz100);

static inline void rflSetFreqHz(uint32_t hz) {
  uint32_t mhz100 = hz / 10000UL;
  ccSetFrequencyMHz100(mhz100);
}

static int16_t rflClamp16(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
static int rflMapInt(int x, int in_min, int in_max, int out_min, int out_max) {
  long num = (long)(x - in_min) * (long)(out_max - out_min);
  long den = (long)(in_max - in_min);
  if (den == 0) return out_min;
  return (int)(out_min + num / den);
}

static void rflScanInit() {
  for (uint8_t i=0; i<SCAN_BINS; i++) scanRssiDbm[i] = -110;
  scanPos = 0;
}

static void rflScanTick() {
  uint32_t hz = SCAN_START_HZ + (uint32_t)scanPos * SCAN_STEP_HZ;

  busForRF();
  ccStrobe(CC1101_SIDLE);
  rflSetFreqHz(hz);
  ccStrobe(CC1101_SRX);

  delay(3);

  uint8_t raw = ccReadStatus(CC1101_RSSI);
  scanRssiDbm[scanPos] = convertRssi(raw);

  scanPos++;
  if (scanPos >= SCAN_BINS) scanPos = 0;
}

// =====================================================
// 5) Draw screens (no freeze; always live)
// =====================================================
static void rflDrawMenu() {
  rflDrawHeader("Learning");
  rflDrawBoxFrame();

  const int rowH = 34;
  int y = BOX_Y + (BOX_H / 2) - (rowH / 2);
  int textY = y + 3;

  busForTFT();
  tft.fillRoundRect(BOX_X+8, y-4, BOX_W-18, 30, 6, COLDARK);
  tft.drawRoundRect(BOX_X+8, y-4, BOX_W-18, 30, 6, COLNEON);

  tft.setTextSize(2);
  tft.setTextColor(COLNEON);

  const char* label = "Start lesson";
  int textWidth = (int)strlen(label) * 12;
  int textX = BOX_X + (BOX_W - textWidth) / 2;

  tft.setCursor(textX, textY);
  tft.print(label);

  uiFooter("OK start   BACK main");
}


static void rflDrawL1() {
  // RX mode: spectrum scan live
  if (!l1_txMode) {
    rflScanTick();

    const int sx = PANEL_X()+10;
    const int sy = PANEL_Y()+6;
    const int sw = PANEL_W()-20;
    const int sh = PANEL_H()-28;

    busForTFT();
    tft.fillRect(sx, sy, sw, sh, COLBLACK);

    int16_t peakDbm = -200;
    for (uint8_t i=0;i<SCAN_BINS;i++) if (scanRssiDbm[i] > peakDbm) peakDbm = scanRssiDbm[i];

    const int step = sw / SCAN_BINS;
    const int baseY = sy + sh - 2;

    for (uint8_t i=0;i<SCAN_BINS;i++) {
      int bx = sx + i*step + (step/2);
      int16_t dbm = rflClamp16(scanRssiDbm[i], -110, -40);
      int h = rflMapInt((int)dbm, -110, -40, 2, sh-6);
      if (h < 2) h = 2;
      if (h > sh-6) h = sh-6;

      tft.drawLine(bx, baseY, bx, baseY-h, COLNEON);
      tft.drawLine(bx+1, baseY, bx+1, baseY-(h-2), COLNEON);
    }

    const int tx = PANEL_X()+12;
    const int ty = PANEL_Y()+PANEL_H()-16;
    const int tw = PANEL_W()-24;
    const int th = 10;
    busForTFT();
    tft.fillRect(tx, ty, tw, th, COLBLACK);

    char r[40];
    snprintf(r, sizeof(r), "MODE: RX   PEAK: %d dBm", (int)peakDbm);
    rflPrintAt(PANEL_X()+12, PANEL_Y()+PANEL_H()-16, 1, COLDIM, r);
    return;
  }
  // TX mode: gamified single centered message
  busForTFT();
  tft.fillRect(PANEL_X()+10, PANEL_Y()+10, PANEL_W()-20, PANEL_H()-20, COLBLACK);

  int y = PANEL_Y() + (PANEL_H()/2) - 10;
  uiMsgCenter("PRESS UP", y, l1_txActive ? COLNEON : COLDIM, 2);
}


static void rflDrawL2() {
  busForTFT();
  tft.fillRect(PANEL_X()+10, PANEL_Y()+10, PANEL_W()-20, PANEL_H()-20, COLBLACK);

  char ln1[40], ln2[40], ln3[40];
  if (rflHaveRx) {
    snprintf(ln1, sizeof(ln1), "from:%02X  ctr:%u  type:%u", (unsigned)rflLastFrom, (unsigned)rflLastCtr, (unsigned)rflLastType);
    snprintf(ln2, sizeof(ln2), "rssi:%d dBm  lqi:%u", (int)rflLastRssi, (unsigned)rflLastLqi);
    snprintf(ln3, sizeof(ln3), "proto:%02X  (packet field 1)", (unsigned)rflLastFlag);
  } else {
    snprintf(ln1, sizeof(ln1), "RX: (none yet)");
    snprintf(ln2, sizeof(ln2), "Waiting for packet...");
    ln3[0] = 0;
  }

  rflPrintAt(PANEL_X()+12, PANEL_Y()+14, 1, COLDIM, ln1);
  rflPrintAt(PANEL_X()+12, PANEL_Y()+28, 1, COLDIM, ln2);
  rflPrintAt(PANEL_X()+12, PANEL_Y()+42, 1, COLDIM, ln3);

}

static void rflDrawL3() {
  busForTFT();
  tft.fillRect(PANEL_X()+10, PANEL_Y()+10, PANEL_W()-20, PANEL_H()-20, COLBLACK);

  // Single integrity verdict
  int y = PANEL_Y() + (PANEL_H()/2) - 10;
  uiMsgCenter(l3_crcMsg, y, (strcmp(l3_crcMsg, "TX OK") == 0) ? COLNEON : COLRED, 2);
}

static void rflDrawL4() {
  busForTFT();
  tft.fillRect(PANEL_X()+10, PANEL_Y()+10, PANEL_W()-20, PANEL_H()-20, COLBLACK);

  char st[40];
  uint32_t left = 0;
  if (capArmed) {
    uint32_t el = millis() - capArmMs;
    left = (el >= CAP_WIN_MS) ? 0 : (CAP_WIN_MS - el);
  }
  snprintf(st, sizeof(st), "Armed:%s %s %us", capArmed ? "YES" : "NO", capFrozen ? "FROZEN" : "LIVE", (unsigned)(left/1000));
  rflPrintAt(PANEL_X()+12, PANEL_Y()+14, 1, COLDIM, st);

  if (rflHaveRx) {
    char ls[40];
    snprintf(ls, sizeof(ls), "Last: from:%02X ctr:%u", (unsigned)rflLastFrom, (unsigned)rflLastCtr);
    rflPrintAt(PANEL_X()+12, PANEL_Y()+28, 1, COLDIM, ls);
  } else {
    rflPrintAt(PANEL_X()+12, PANEL_Y()+28, 1, COLDIM, "Last: (none)");
  }

    // show up to 5 items, keep selected visible
  uint8_t showMax = 4;
  uint8_t start = 0;
  if (capLen > showMax && capSel >= showMax) start = capSel - (showMax - 1);
  int y = PANEL_Y() + 44;
  for (uint8_t k=0; k<showMax; k++) {
    uint8_t i = start + k;
    if (i >= capLen) break;
    const uint8_t *p = capBuf[i].payload;
    if (p[0] != RFL_MAGIC) continue;
    char li[48];
    snprintf(li, sizeof(li), "%c#%u from:%02X ctr:%u", (i==capSel)?'>':' ', (unsigned)(i+1), (unsigned)p[3], (unsigned)p[5]);
    rflPrintAt(PANEL_X()+12, y, 1, (i==capSel)?COLNEON:COLDIM, li);
    y += 10;
  }
  // Access result (shown when a REPLAY is received as a victim)
  if (l5_accessLine1[0] && millis() - l5_accessMs < 4000) {
    int yy = PANEL_Y() + 55;
    uiMsgCenter(l5_accessLine1, yy, COLNEON, 2);
    if (l5_accessLine2[0]) uiMsgCenter(l5_accessLine2, yy + 18, COLDIM, 1);
  }
}

static void rflDrawL5() {
  busForTFT();
  tft.fillRect(PANEL_X()+10, PANEL_Y()+10, PANEL_W()-20, PANEL_H()-20, COLBLACK);

  // Source selection
  if (capLen > 0) {
    uint8_t idx = (capSel < capLen) ? capSel : 0;
    const uint8_t *p = capBuf[idx].payload;
    char src[48];
    snprintf(src, sizeof(src), "Replay src #%u from:%02X ctr:%u", (unsigned)(idx+1), (unsigned)p[3], (unsigned)p[5]);
    rflPrintAt(PANEL_X()+12, PANEL_Y()+14, 1, COLDIM, src);
  } else {
    rflPrintAt(PANEL_X()+12, PANEL_Y()+14, 1, COLDIM, "Replay src: (none)");
  }

  // Access result (shown when a REPLAY is received)
  if (l5_accessLine1[0]) {
    int y = PANEL_Y() + 45;
    uiMsgCenter(l5_accessLine1, y, COLNEON, 2);
    if (l5_accessLine2[0]) uiMsgCenter(l5_accessLine2, y + 18, COLDIM, 1);
  } else {
    rflPrintAt(PANEL_X()+12, PANEL_Y()+34, 1, COLDIM, "UP replay  DOWN clear");
  }
}

static void rflDrawStaticForCurrent() {
  rflDrawHeader("Learning");

  switch (rflab) {
    case RFL_MENU:
      rflDrawBoxFrame();
      break;

    case RFL_INTRO:
      rflDrawBoxFrame();
      uiFooter("OK next   BACK menu");
      break;

    case RFL_QR:
      rflDrawBoxFrame();
      uiFooter("OK next   BACK prev");
      break;

    case RFL_L1:
      rflDrawBoxFrame();
      if (l1_txMode) { 
        rflTitleSubInBox("PACKET INJECTION", "TX MODE (live transmit)"); 
        }
      else {
        rflTitleSubInBox("SPECTRUM SCAN", "RX MODE (scan + listen)");
      }
      rflDrawPanel();
      uiFooter("HOLD DOWN mode  OK next  BACK menu");
      break;

    case RFL_L2:
      rflDrawBoxFrame();
      rflTitleSubInBox("FRAMES");
      rflDrawPanel();
      uiFooter("DOWN clear  OK next  BACK prev");
      break;

    case RFL_L3:
      rflDrawBoxFrame();
      rflTitleSubInBox("INTEGRITY");
      rflDrawPanel();
      uiFooter("DOWN clear  OK next  BACK prev");
      break;

    case RFL_L4:
      rflDrawBoxFrame();
      rflTitleSubInBox("CAPTURE", "Victim Mode (Accept Replay)");
      rflDrawPanel();
      uiFooter("UP arm  DOWN sel  HOLD DOWN clear");
      break;

    case RFL_L5:
      rflDrawBoxFrame();
      rflTitleSubInBox("REPLAY", "Targeted + anti-replay");
      rflDrawPanel();
      uiFooter("UP replay  OK menu  BACK prev");
      break;
  }

  rflStaticDrawn = true;
}

static inline void rflClearPanelInner() {
  const int x = PANEL_X()+10;
  const int y = PANEL_Y()+10;
  const int w = PANEL_W()-20;
  const int h = PANEL_H()-20;
  busForTFT();
  tft.fillRect(x, y, w, h, COLBLACK);
}


// =====================================================
// 6) Navigation init
// =====================================================
static void rflGo(RFLabPhase2 p);

static void rflGo(RFLabPhase2 p) {
  RFLabPhase2 prev = rflab;

  rflab = p;

  rflStaticDrawn = false;
  rflPrev = (RFLabPhase2)0xFF; 

  rflLastDrawMs = 0; 
  
  beep(10);

  // Keep RX always
  busForRF();
  ccStrobe(CC1101_SRX);

  if (p == RFL_MENU) {
    capLen = 0;
    capArmed = false;
  }

  if (p == RFL_L1) {
    l1_txMode = false;   // default RX
    rflScanInit();
    l1ApplyMode();
  }

  if (p == RFL_L3) {
    l3_rxOk = 0;
    l3_missWin = 0;
    l3_lastRxMs = millis();
  }

  if (p == RFL_L4) {
    capLen = 0;
    capArmed = false;
  }

  if (p == RFL_L5) {
    bool preserve = (prev == RFL_L4 && capLen > 0);
    rflResetL5(preserve);
  }
}

// =====================================================
// 7) Public API: enter/draw/tick
// =====================================================
static void learningEnter() {
  rflab = RFL_MENU;

  rflHaveRx = false;

  l1_txMode = false;


  busForRF();
  ccStrobe(CC1101_SRX);

  rflDrawMenu();
}

static void learningDraw() {;
  uint32_t now = millis();
  if (now - rflLastDrawMs < RFL_DRAW_MS) return;
  rflLastDrawMs = now;
  
  // ---- One-time full redraw when screen changes ----
  if (rflab != rflPrev) {
    rflPrev = rflab;
    rflStaticDrawn = false;
  }

  if (!rflStaticDrawn) {
    rflDrawStaticForCurrent();   // <- este hace rflDrawHeader() y limpia
    rflStaticDrawn = true;
  }

  if (rflab == RFL_L1 && l1_txActive) {
     if (millis() - l1_txActiveMs > 250) {
       l1_txActive = false; // Turn off the neon flash after 250ms
     }
  }

  if (rflab == RFL_L5) {
     if (l5_accessLine1[0] == 'W' && millis() - l5_accessMs > 2500) {
        // Timeout waiting for victim
        strncpy(l5_accessLine1, "ACCESS DENIED", sizeof(l5_accessLine1));
        l5_accessLine2[0] = 0;
        l5AccessEffect(false);
     }
  }

  if (rflab == RFL_L3) {
    uint32_t now = millis();
    if (now - l3_lastRxMs > 1200) {
      l3_missWin++;
      l3_lastRxMs = now;
    }
    // Count RX_OK only for lesson 3 frames (now using type to distinguish, or we can check type)
    if (rflHaveRx && rflLastType != RFL_BEACON) {
      l3_rxOk++;
      l3_lastRxMs = now;
      // prevent double-count on same drawn frame
      rflHaveRx = false;
    }
  }

  if (rflab == RFL_L4 && capArmed) {
    uint32_t now = millis();
    if (now - capArmMs > CAP_WIN_MS) capArmed = false;
  }
  

  switch (rflab) {
    case RFL_MENU:  rflDrawMenu(); break;
    case RFL_INTRO: rflDrawIntroScreen();  break;
    case RFL_QR:    rflDrawQrScreen(RFLAB_GUIDE_URL); break;
    case RFL_L1:    rflDrawL1(); break;
    case RFL_L2:    rflDrawL2(); break;
    case RFL_L3:    rflDrawL3(); break;
    case RFL_L4:    rflDrawL4(); break;
    case RFL_L5:    rflDrawL5(); break;
  }
}

static void learningTick(const ButtonsEdge& b) {
  // Always service RF (RX-dominant + non-blocking TX scheduler)
  rflRxTick();
  rflTxTick();

  // ======================= MENU =======================
  if (rflab == RFL_MENU) {
    if (b.ok)   { rflGo(RFL_INTRO); learningDraw(); return; }
    if (b.back) { rfPowerDown(); goTo(MODE_MENU); return; }
    return;
  }

  // ======================= INTRO =======================
  if (rflab == RFL_INTRO) {
    if (b.ok)   { rflGo(RFL_QR); learningDraw(); return; }
    if (b.back) { rflGo(RFL_MENU); learningDraw(); return; }
    return;
  }

  // ======================= QR =======================
  if (rflab == RFL_QR) {
    if (b.ok)   { rflGo(RFL_L1); learningDraw(); return; }
    if (b.back) { rflGo(RFL_INTRO); learningDraw(); return; }
    return;
  }

  // ======================= LESSON NAV =======================
  if (rflab >= RFL_L1 && rflab <= RFL_L5) {
    // OK / BACK navigation
    if (b.ok) {
      if (rflab == RFL_L4 && capLen == 0) {
        beep(50);
        uiMsgCenter("CAPTURE REQ'D", 150, COLRED, 2);
        delay(600);
        return;
      }
      
      if (rflab == RFL_L5) rflGo(RFL_MENU);
      else rflGo((RFLabPhase2)((uint8_t)rflab + 1));
      learningDraw();
      return;
    }
    if (b.back) {
      if (rflab == RFL_L1) rflGo(RFL_MENU);
      else rflGo((RFLabPhase2)((uint8_t)rflab - 1));
      learningDraw();
      return;
    }

    // ======================= L1 =======================
    if (rflab == RFL_L1) {
      // HOLD DOWN => toggle RX/TX
      if (downHoldDetect(700)) {
        l1_txMode = !l1_txMode;
        l1ApplyMode();
        beep(12);
        rflStaticDrawn = false;
        learningDraw();
        return;
      }

      if (b.up) {
        if (l1_txMode) {
          // Temporizador para el destello visual de la UI
          l1_txActive = true;
          l1_txActiveMs = millis();

          // Non-blocking burst (scheduler). Byte 1 sends Flag 0x26 (HackCon 26) instead of lesson 1
          // Byte 7 sends 0x48 (The ASCII letter 'H' for HackCon)
          rflTxRequest(0x26, RFL_BEACON, rflSelfId(), false, 0x48, L1_TXREPEATS, L1_TXGAPMS);
          beep(10);
        } else {
          beep(6);
        }
      }

      learningDraw();
      return;
    }

    // ======================= L2 =======================
    if (rflab == RFL_L2) {
      if (b.down) {
        capLen = 0;
        capSel = 0;
        capArmed = false;
        l5_accessLine1[0] = 0;
        l5_accessLine2[0] = 0;

        rflHaveRx = false;

        rflLastFlag = 0;
        rflLastType = 0;
        rflLastFrom = 0;
        rflLastTarget = 0;
        rflLastCtr = 0;

        rflLastRssi = -127;
        rflLastLqi = 0;

        rflTxqClear(); // cancel any pending transmissions
        beep(8);
        learningDraw();
        return;
      }


      learningDraw();
      return;
    }

    // ======================= L3 =======================
    if (rflab == RFL_L3) {
      // L3 shows whether the last RX looks normal or replayed (compromised).
      if (b.down) {
        strncpy(l3_crcMsg, "----", sizeof(l3_crcMsg));
        beep(8);
      }
      learningDraw();
      return;
    }

    // ======================= L4 =======================
    if (rflab == RFL_L4) {
      if (downHoldDetect(700)) {
        capLen = 0;
        capSel = 0;
        capFrozen = false;
        capArmed = false;
        rflTxqClear();
        beep(8);
        learningDraw();
        return;
      }

      // DOWN tap => selection manual (cycle)
      if (b.down) {
        if (capLen > 0) {
          capSel = (uint8_t)((capSel + 1) % capLen);
          beep(6);
        } else {
          beep(4);
        }
        learningDraw();
        return;
      }

      // UP tap => arm capture window
      if (b.up) {
        capLen = 0;
        capSel = 0;
        capFrozen = false;
        capArmed = true;
        capArmMs = millis();

        beep(10);
        learningDraw();
        return;
      }

      learningDraw();
      return;
    }

    // ======================= L5 =======================
    if (rflab == RFL_L5) {
      // DOWN tap => clear/reset capture buffer (and access banner)
      if (b.down) {
        capLen = 0;
        capSel = 0;
        capFrozen = false;
        capArmed = false; // L5 never captures live
        l5_accessLine1[0] = 0;
        l5_accessLine2[0] = 0;
        rflTxqClear();
        beep(8);
        learningDraw();
        return;
      }

      // UP replays the selected capture (selected in L4)
      if (b.up) {
        if (capLen == 0) {
          strncpy(l5_accessLine1, "NO CAPTURE", sizeof(l5_accessLine1));
          l5_accessLine2[0] = 0;
          l5_accessMs = millis();
          beep(12);
          learningDraw();
          return;
        }

        uint8_t idx = (capSel < capLen) ? capSel : 0;
        uint8_t p[9];
        memcpy(p, capBuf[idx].payload, 9);
        p[2] = (uint8_t)RFL_REPLAY;

        // Non-blocking replay burst
        rflTxRequestRaw9(p, L1_TXREPEATS, L1_TXGAPMS);

        // Simulamos la lógica para el atacante que evalúa si robó correctamente una identidad (replayedFrom != my_id)
        uint8_t replayedFrom = p[3];
        if (replayedFrom != rflSelfId()) {
          l5_target_id = replayedFrom; // Guardamos registro explícito a quién atacamos
          strncpy(l5_accessLine1, "WAITING...", sizeof(l5_accessLine1));
          l5_accessLine2[0] = 0;
          l5_accessMs = millis();
        } else {
          strncpy(l5_accessLine1, "ACCESS DENIED", sizeof(l5_accessLine1));
          l5_accessLine2[0] = 0;
          l5_accessMs = millis();
          l5AccessEffect(false);
        }

        learningDraw();
        return;
      }

      learningDraw();
      return;
    }

    // Fallback
    learningDraw();
    return;
  }
}

// ======================= THE BOX =======================
static char boxCodeInput[7] = "AAAAAA";
static uint8_t boxCodePos = 0;

static void drawTheBox() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("THE BOX");

  tft.drawRect(10, 78, 220, 90, COLDARK);

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(14, 52);
  tft.print("hold OK submit   HOLD BACK exit");

  tft.setTextSize(4);
  tft.setTextColor(COLNEON);

  const int baseX = 36;
  const int baseY = 106;
  const int stepX = 28;

  for (uint8_t i = 0; i < 6; i++) {
    char c = boxCodeInput[i];
    tft.setCursor(baseX + (i * stepX), baseY);
    tft.print(c);
  }

  // Cursor tipo underline
  int cx = baseX + (boxCodePos * stepX);
  tft.drawFastHLine(cx, 140, 22, COLNEON);

  uiFooter("UP/DOWN digit   OK next   BACK prev");
}

// ======================= THE KEY =======================
static char keyInput[7] = "000000";
static uint8_t keyPos = 0;
static bool    keyVerified = false;
static bool    keyJustEntered = false;

static const uint8_t KEY_XOR = 0x5A;

static const uint8_t KEY_UNLOCK_XOR[6] = { 0x0F, 0x0A, 0x02, 0x0A, 0x18, 0x1B };

static const uint8_t KEY_PAIRS_XOR[6]  = { 0x6F, 0x6C, 0x63, 0x62, 0x6E, 0x69 };

static inline void keyDecodeUnlock(char out[7]) {
  for (uint8_t i = 0; i < 6; i++) out[i] = (char)(KEY_UNLOCK_XOR[i] ^ KEY_XOR);
  out[6] = 0;
}

static inline char keyPairsChar(uint8_t i) {
  return (char)(KEY_PAIRS_XOR[i] ^ KEY_XOR);
}

static uint8_t keyPairOrder[3] = {0,1,2}; 

// ===================== helpers =====================

static void keyClearInput() {
  for (uint8_t i = 0; i < 6; i++) keyInput[i] = '0';
  keyInput[6] = 0;
  keyPos = 0;
  keyJustEntered = false;
}

static void keyShufflePairsOnce() {
  for (int i = 2; i > 0; --i) {
    int j = esp_random() % (i + 1);
    uint8_t tmp = keyPairOrder[i];
    keyPairOrder[i] = keyPairOrder[j];
    keyPairOrder[j] = tmp;
  }
}

static void keyBuildFixedShuffledView(char out[9], bool showMask) {
  for (uint8_t k = 0; k < 3; k++) {
    uint8_t p = keyPairOrder[k];  
    uint8_t src = (uint8_t)(p * 2);
    uint8_t dst = (uint8_t)(k * 2);

    char a = keyPairsChar(src);
    char b = keyPairsChar(src + 1);

    if (showMask) {
      if (dst >= keyPos) a = '_';
      if ((dst + 1) >= keyPos) b = '_';
    }

    out[dst]     = a;
    out[dst + 1] = b;
  }
  out[6] = 0;
}

static uint8_t keyScreenIndexToFixedIndex(uint8_t screenIdx) {
  uint8_t pairOnScreen = screenIdx / 2;     
  uint8_t digitInPair  = screenIdx % 2;    
  uint8_t pairId       = keyPairOrder[pairOnScreen];
  return (uint8_t)(pairId * 2 + digitInPair);
}

static bool keyIsCorrectInCurrentOrder() {
  for (uint8_t screenIdx = 0; screenIdx < 6; screenIdx++) {
    uint8_t fixedIdx = keyScreenIndexToFixedIndex(screenIdx);
    if (keyInput[screenIdx] != keyPairsChar(fixedIdx)) return false;
  }
  return true;
}

// ===================== anim "crypto-ish" =====================

static char randDecryptChar() {
  const char set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%*+-";
  return set[random8(sizeof(set)-1)];
}

// ===== THE KEY layout constants (match drawTheKey) =====
static const int KEY_BOX_X = 10;
static const int KEY_BOX_Y = 78;
static const int KEY_BOX_W = 220;
static const int KEY_BOX_H = 90;

static const int KEY_CODE_Y = KEY_BOX_Y + 36;

static void keyDrawInCodeArea(const char* s, uint16_t col, uint8_t textSize) {
  busForTFT();

  int clearY = KEY_CODE_Y - 6;
  int clearH = (KEY_BOX_Y + KEY_BOX_H - 2) - clearY;
  if (clearH < 0) clearH = 0;

  tft.fillRect(KEY_BOX_X + 1, clearY, KEY_BOX_W - 2, clearH, COLBLACK);
  uiMsgCenter(s, KEY_CODE_Y, col, textSize);
}

static void keyDecryptAnimationThenMessage(const char* finalMsg, uint16_t finalCol) {
  const uint32_t totalMs = 4000;
  const uint32_t stepMs  = 55;
  const uint32_t t0 = millis();

  char line[13];
  memset(line, 0, sizeof(line));

  while ((millis() - t0) < totalMs) {
    for (uint8_t i=0; i<6; i++) line[i] = randDecryptChar();
    line[6] = 0;

    keyDrawInCodeArea(line, COLDIM, 3);

    frfBackgroundTick();
    screensaverTick();
    if (systemSuspended) return;  // si entra el saver, abortar animacion
    powerLedsTick();
    ledTick();
    delay(stepMs);
  }

  keyDrawInCodeArea(finalMsg, finalCol, 3);
  beep(20);
}

// ===================== entry + draw tick =====================
static void keyEnterModule() {
  keyClearInput();

  keyPairOrder[0]=0; keyPairOrder[1]=1; keyPairOrder[2]=2;

  keyShufflePairsOnce();

  char view[9];
  keyBuildFixedShuffledView(view, true);
  keyDrawInCodeArea(view, COLWHITE, 3);
}

static void keyDrawTick() {
  char view[7];
  keyBuildFixedShuffledView(view, true);
  keyDrawInCodeArea(view, COLWHITE, 3);
}

// ======================= MENU / SETTINGS / FOX HUNT / FRIENDS =======================
static bool loadKeyVerified() {
  prefs.begin("hackcon", true);
  bool v = prefs.getBool("k_ok", false);
  prefs.end();
  return v;
}
static void saveKeyVerified(bool v) {
  prefs.begin("hackcon", false);
  prefs.putBool("k_ok", v);
  prefs.end();
}

static uint8_t menuIndex = 0;

static inline bool keyUnlocked() { return questAlienDone; }

static inline uint8_t menuCount() {
  return keyUnlocked() ? 7 : 6;
}

static inline const char* menuItem(uint8_t i) {
  if (i == 0) return "Go Hunting";
  if (i == 1) return "Friends";
  if (i == 2) return "Games";

  if (keyUnlocked()) {
    if (i == 3) return "THE KEY";
    if (i == 4) return "THE BOX";
    if (i == 5) return "Learning";
    if (i == 6) return "Settings";
  } else {
    if (i == 3) return "THE BOX";
    if (i == 4) return "Learning";
    if (i == 5) return "Settings";
  }
  return "???";
}

static const int MENU_LIST_X = 10;
static const int MENU_LIST_W = 220;
static const int MENU_LIST_Y = 42;
static const int MENU_LIST_H = 160;
static const int MENU_ROW_H  = 34;
static const uint8_t MENU_VISIBLE_ROWS = 4;

static void drawScrollbarGeneric(int listX, int listY, int listW, int listH, uint8_t visibleRows, uint8_t total, uint8_t topIndex) {
  int sbX = listX + listW - 8;
  int sbY = listY + 2;
  int sbH = listH - 4;

  busForTFT();
  tft.drawRect(sbX, sbY, 6, sbH, COLDARK);

  if (total <= visibleRows) {
    tft.fillRect(sbX+1, sbY+1, 4, sbH-2, COLDARK);
    return;
  }

  float ratio = (float)visibleRows / (float)total;
  int thumbH = (int)((sbH-2) * ratio);
  if (thumbH < 10) thumbH = 10;

  int maxTop = total - visibleRows;
  int trackH = (sbH-2) - thumbH;
  int thumbY = sbY + 1 + (int)((trackH * (float)topIndex) / (float)maxTop);

  tft.fillRect(sbX+1, sbY+1, 4, sbH-2, COLBLACK);
  tft.fillRect(sbX+1, thumbY, 4, thumbH, COLNEON);
}

static void drawMenu() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("MAIN MENU");
  uiSmallStatusRX();

  tft.drawRect(MENU_LIST_X, MENU_LIST_Y, MENU_LIST_W, MENU_LIST_H, COLDARK);

  uint8_t total = menuCount();
  if (menuIndex >= total) menuIndex = 0;

  uint8_t topIndex = 0;
  if (total > MENU_VISIBLE_ROWS) {
    if (menuIndex < (MENU_VISIBLE_ROWS/2)) topIndex = 0;
    else {
      int desired = (int)menuIndex - (int)(MENU_VISIBLE_ROWS/2);
      int maxTop = (int)total - (int)MENU_VISIBLE_ROWS;
      if (desired > maxTop) desired = maxTop;
      if (desired < 0) desired = 0;
      topIndex = (uint8_t)desired;
    }
  }

  tft.fillRect(MENU_LIST_X+1, MENU_LIST_Y+1, MENU_LIST_W-2, MENU_LIST_H-2, COLBLACK);

  for (uint8_t r=0; r<MENU_VISIBLE_ROWS; r++) {
    uint8_t idx = topIndex + r;
    if (idx >= total) break;

    int y = MENU_LIST_Y + 15 + r*MENU_ROW_H;
    int textY = y + 3;
    bool sel = (idx == menuIndex);

    if (sel) {
      tft.fillRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLDARK);
      tft.drawRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLNEON);
      tft.setTextColor(COLNEON);
    } else {
      tft.setTextColor(COLDIM);
    }

    tft.setTextSize(2);
    tft.setCursor(MENU_LIST_X+16, textY);
    tft.print(menuItem(idx));
  }

  drawScrollbarGeneric(MENU_LIST_X, MENU_LIST_Y, MENU_LIST_W, MENU_LIST_H, MENU_VISIBLE_ROWS, total, topIndex);

  uiFooter("UP/DOWN move   OK select");
}

// ======================= Screen saver =======================
// (BLE removed)

static inline bool rfWantedForMode(ScreenMode m) {
  return (m == MODE_FOX_HUNT || m == MODE_LEARNING || m == MODE_THE_BOX ||
          m == MODE_FRIENDS_MENU || m == MODE_FRIENDS_SCAN || m == MODE_FRIENDS_REQUESTING ||
          m == MODE_FRIENDS_LIST || m == MODE_FRIEND_REQ_PROMPT);
}

static inline bool frfWantedForMode(ScreenMode m) {
  return (m == MODE_FRIENDS_MENU || m == MODE_FRIENDS_SCAN || m == MODE_FRIENDS_REQUESTING ||
          m == MODE_FRIENDS_LIST || m == MODE_FRIEND_REQ_PROMPT);
}

static void rfEnsureForMode(ScreenMode m) {
  if (systemSuspended) {
    rfPowerDown();
    return;
  }
  const bool want = rfWantedForMode(m);
  if (want) {
    rfPowerUp();
  } else {
    rfPowerDown();
  }
}


// ======================= BOOT TO SAVER =======================
static bool bootToSaverDone  = false;     
static bool bootToSaverActive = false;    


static void enterBootToSaver() {
  if (bootToSaverActive) return;

  mode = MODE_MENU;
  bootToSaverActive = true;
  bootToSaverDone   = true;
  screensaverDrawn = false;

  rfPowerDown();
  ledsSuspended = true;
  FastLED.setBrightness(10);
  ledsMarkDirty();
  ledsCommitIfNeeded();

  drawScreensaverStatic();
}

static void exitBootToSaver() {
  if (!bootToSaverActive) return;

  bootToSaverActive = false;
  screensaverDrawn = false;

  ledsSuspended = false;
  applyLedBrightness();
  ledTick();

  lastActivityMs = millis();
  goTo(MODE_MENU); 
}

// ======================= SCREENSAVER (STATIC) =======================

struct SaverBox {
  int x, y, w, h;
};

static const SaverBox SAVER_BOX_NICK = { 94, 44, 84, 18 };
static const SaverBox SAVER_BOX_ROLE = { 94, 64, 130, 24 };

static inline const char* saverNick() {
  return (badgeNick[0] ? badgeNick : "AAAAAAA");
}

static inline const char* saverRole() {
  uint8_t r = (uint8_t)badgeRole;
  if (r > 4) r = 0;

  return BADGE_ROLE_NAMES[r];
}

static void drawAutoBoxText(const char* s,
                            int x, int y,
                            uint16_t textCol,
                            uint8_t textSize,
                            uint8_t pad = 4) {
  if (!s) s = "";

  busForTFT();

  tft.setTextWrap(false);
  tft.setTextSize(textSize);
  tft.setTextColor(textCol);

  int16_t x1, y1; uint16_t tw, th;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);

  int bw = tw + pad * 2;
  int bh = th + pad * 2;

  tft.fillRect(x, y, bw, bh, COLBLACK);

  int tx = x + (bw - tw) / 2;
  int ty = y + (bh - th) / 2;

  tft.setCursor(tx, ty);
  tft.print(s);
}


static void drawScreensaverStatic() {
  if (screensaverDrawn) return;

  busForTFT();
  tft.fillScreen(COLBLACK);

  const int x = (240 - SAVER_W) / 2;
  const int y = (240 - SAVER_H) / 2 - 2;

  const uint8_t* bits = SAVERS[selectedSaver].bits;

  // Una sola transacción SPI para toda la imagen = máxima velocidad
  uint16_t lineBuf[SAVER_W];
  tft.startWrite();
  tft.setAddrWindow(x, y, SAVER_W, SAVER_H);
  if (SAVERS[selectedSaver].isMono) {
    // Bitmap 1-bit: expandir a RGB565 con colores del badge
    for (int i = 0; i < SAVER_H; i++) {
      for (int j = 0; j < SAVER_W; j++) {
        int bitPos = i * SAVER_W + j;
        uint8_t b = pgm_read_byte(&bits[bitPos / 8]);
        lineBuf[j] = (b & (0x80 >> (bitPos % 8))) ? COLNEON : COLBLACK;
      }
      tft.writePixels(lineBuf, SAVER_W);
    }
  } else {
    // Imagen RGB565: corregir endianness línea a línea
    for (int i = 0; i < SAVER_H; i++) {
      for (int j = 0; j < SAVER_W; j++) {
        int idx = (i * SAVER_W + j) * 2;
        uint8_t hi = bits[idx];
        uint8_t lo = bits[idx + 1];
        lineBuf[j] = (hi << 8) | lo;
      }
      tft.writePixels(lineBuf, SAVER_W);
    }
  }
  tft.endWrite();

  if (SAVERS[selectedSaver].showOverlay) {
    char nickBuf[8]; memset(nickBuf, 0, sizeof(nickBuf));
    strlcpy(nickBuf, saverNick(), sizeof(nickBuf));
    drawAutoBoxText(nickBuf, 115, 42, COLNEON, 2);

    drawAutoBoxText(saverRole(), 115, 70, COLNEON, 2);

    char idBuf[12]; memset(idBuf, 0, sizeof(idBuf));
    snprintf(idBuf, sizeof(idBuf), "ID-%04X", selfShortId);
    drawAutoBoxText(idBuf, 115, 98, COLNEON, 2);
  }

  screensaverDrawn = true;
}

static void enterScreensaver() {
  if (systemSuspended) return;

  modeBeforeSaver = mode;
  systemSuspended = true;
  screensaverDrawn = false;

  // Silenciar música de juegos para que no siga sonando en el screensaver
  // (buzzerNoTone es suficiente aquí; el restart se hace en exitScreensaver→goTo)
  buzzerNoTone();

  rfPowerDown();
  drawScreensaverStatic();
}

static void exitScreensaver() {
  if (!systemSuspended) return;

  systemSuspended = false;
  screensaverDrawn = false;

  // Volver al modo previo sin replays de intros; goTo() aplica politicas de RF correctamente
  lastActivityMs = millis();
  goTo(modeBeforeSaver, true);
}


static void screensaverTick() {
  if (bootToSaverActive) return;

  // En juegos usamos timeout más largo para no interrumpir partidas activas
  const bool inGame = (mode == MODE_DINO || mode == MODE_MAZE || mode == MODE_HACKMON);
  // const uint32_t IDLE_SAVER_MS = 30UL * 1000UL;  // 30s (DEV)
  const uint32_t IDLE_SAVER_MS = inGame
    ? 5UL * 60UL * 1000UL   // 5 min mientras se juega
    : 1UL * 60UL * 1000UL;  // 1 min en el resto

  bool idle = (millis() - lastActivityMs) > IDLE_SAVER_MS;

  if (idle && !systemSuspended) {
    enterScreensaver();
  }
}

// ======================= FRIENDS UI =======================
static uint8_t friendsMenuIndex = 0;
static const char* FRIENDS_MENU_ITEMS[] = { "Add Friends", "Friends List" };
static const uint8_t FRIENDS_MENU_COUNT = 2;

static const int FRIENDS_LIST_X = 10;
static const int FRIENDS_LIST_W = 220;
static const int FRIENDS_LIST_Y = 42;
static const int FRIENDS_LIST_H = 160;
static const int FRIENDS_ROW_H  = 28;
static const uint8_t FRIENDS_VISIBLE_ROWS = 5;

static uint8_t scanIndex = 0;
static uint8_t friendsListIndex = 0;

static uint32_t scanEndMs = 0;
bool scanPhaseCollecting = false;
static uint16_t requestTargetId = 0;
static char requestTargetNick[9] = {0};

static bool backHoldDetect(uint16_t holdMs) {
  static uint32_t t0 = 0;
  if (isPressed(PUSH4PIN)) {
    if (t0 == 0) t0 = millis();
    if (millis() - t0 >= holdMs) { t0 = 0; return true; }
  } else {
    t0 = 0;
  }
  return false;
}

static void drawFriendsMenu() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("FRIENDS");

  tft.drawRect(MENU_LIST_X, MENU_LIST_Y, MENU_LIST_W, MENU_LIST_H, COLDARK);

  uint8_t total = FRIENDS_MENU_COUNT;
  if (friendsMenuIndex >= total) friendsMenuIndex = 0;

  uint8_t topIndex = 0;
  tft.fillRect(MENU_LIST_X+1, MENU_LIST_Y+1, MENU_LIST_W-2, MENU_LIST_H-2, COLBLACK);

  for (uint8_t r=0; r<total; r++) {
    int y = MENU_LIST_Y + 15 + r*MENU_ROW_H;
    int textY = y + 3;
    bool sel = (r == friendsMenuIndex);

    if (sel) {
      tft.fillRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLDARK);
      tft.drawRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLNEON);
      tft.setTextColor(COLNEON);
    } else {
      tft.setTextColor(COLDIM);
    }

    tft.setTextSize(2);
    tft.setCursor(MENU_LIST_X+16, textY);
    tft.print(FRIENDS_MENU_ITEMS[r]);
  }

  uiFooter("UP/DOWN sel   OK open   BACK menu");
}

// ======================= GAMES MENU =======================
static uint8_t gamesMenuIndex = 0;
static const char* GAMES_MENU_ITEMS[] = { "Dino Run", "Maze", "HackerMon" };
static const uint8_t GAMES_MENU_COUNT = 3;

static void drawGamesMenu() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("GAMES");

  tft.drawRect(MENU_LIST_X, MENU_LIST_Y, MENU_LIST_W, MENU_LIST_H, COLDARK);

  uint8_t total = GAMES_MENU_COUNT;
  if (gamesMenuIndex >= total) gamesMenuIndex = 0;

  tft.fillRect(MENU_LIST_X+1, MENU_LIST_Y+1, MENU_LIST_W-2, MENU_LIST_H-2, COLBLACK);

  for (uint8_t r = 0; r < total; r++) {
    int y = MENU_LIST_Y + 15 + r * MENU_ROW_H;
    int textY = y + 3;
    bool sel = (r == gamesMenuIndex);

    if (sel) {
      tft.fillRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLDARK);
      tft.drawRoundRect(MENU_LIST_X+8, y-4, MENU_LIST_W-18, 30, 6, COLNEON);
      tft.setTextColor(COLNEON);
    } else {
      tft.setTextColor(COLDIM);
    }

    tft.setTextSize(2);
    tft.setCursor(MENU_LIST_X+16, textY);
    tft.print(GAMES_MENU_ITEMS[r]);
  }

  uiFooter("UP/DOWN sel   OK play   BACK menu");
}

static void drawFriendsScan() {
  busForTFT();
  uiFrame();
  if (scanPhaseCollecting) {
    uint8_t tick = (millis() / 200) % 4;
    char scanBuf[32];
    if (tick == 0)      snprintf(scanBuf, sizeof(scanBuf), "SEARCHING");
    else if (tick == 1) snprintf(scanBuf, sizeof(scanBuf), "SEARCHING.");
    else if (tick == 2) snprintf(scanBuf, sizeof(scanBuf), "SEARCHING..");
    else                snprintf(scanBuf, sizeof(scanBuf), "SEARCHING...");
    uiTitle(scanBuf);
  } else {
    uiTitle("ADD FRIENDS");
  }

  // Draw cyber-border list
  tft.drawRect(FRIENDS_LIST_X, FRIENDS_LIST_Y, FRIENDS_LIST_W, FRIENDS_LIST_H, COLDARK);

  uint8_t total = scanHitsCount;
  if (scanIndex >= total && total > 0) scanIndex = total - 1;
  if (total == 0) scanIndex = 0;

  uint8_t topIndex = 0;
  if (total > FRIENDS_VISIBLE_ROWS) {
    if (scanIndex < (FRIENDS_VISIBLE_ROWS/2)) topIndex = 0;
    else {
      int desired = (int)scanIndex - (int)(FRIENDS_VISIBLE_ROWS/2);
      int maxTop = (int)total - (int)FRIENDS_VISIBLE_ROWS;
      if (desired > maxTop) desired = maxTop;
      if (desired < 0) desired = 0;
      topIndex = (uint8_t)desired;
    }
  }

  // CLEAR BACKGROUND OF THE LIST TO PREVENT STACKING TEXT!
  tft.fillRect(FRIENDS_LIST_X+1, FRIENDS_LIST_Y+1, FRIENDS_LIST_W-2, FRIENDS_LIST_H-2, COLBLACK);

  for (uint8_t r=0; r<FRIENDS_VISIBLE_ROWS; r++) {
    uint8_t idx = topIndex + r;
    if (idx >= total) break;

    int y = FRIENDS_LIST_Y + 16 + r*FRIENDS_ROW_H;
    bool sel = (idx == scanIndex);

    const ScanHit &h = scanHits[idx];
    bool added = (friendsIndexById(h.id) >= 0);
    
    // Cyberpunk dynamic colors based on RSSI
    uint16_t rowColor = COLDIM;
    if (h.rssiDbm >= -50) rowColor = COLNEON;
    else if (h.rssiDbm >= -75) rowColor = COLDIM;
    else rowColor = COLRED; 
    
    // selection graphic
    if (sel) {
      tft.fillRoundRect(FRIENDS_LIST_X+6, y-4, FRIENDS_LIST_W-16, 24, 6, COLDARK);
      tft.drawRoundRect(FRIENDS_LIST_X+6, y-4, FRIENDS_LIST_W-16, 24, 6, COLNEON);
    }

    tft.setTextSize(2);
    tft.setTextColor(sel ? COLNEON : rowColor);
    tft.setCursor(FRIENDS_LIST_X+12, y);
    
    char tmp[16];
    if (h.nick[0]) {
      tft.print(h.nick);
    } else {
      snprintf(tmp, sizeof(tmp), "ID %04X", (unsigned)h.id);
      tft.print(tmp);
    }

    tft.setTextSize(1);
    tft.setTextColor(added ? COLDIM : rowColor);
    int rx = FRIENDS_LIST_X + FRIENDS_LIST_W - 70;
    tft.setCursor(rx, y+5);
    if (added) tft.print("ADDED");
    else {
      tft.print((int)h.rssiDbm);
      tft.print("dB");
    }
  }

  drawScrollbarGeneric(FRIENDS_LIST_X, FRIENDS_LIST_Y, FRIENDS_LIST_W, FRIENDS_LIST_H, FRIENDS_VISIBLE_ROWS, total, topIndex);
}

static void drawFriendsScanFooter() {
  busForTFT();
  if (scanPhaseCollecting) {
    uint32_t left = (scanEndMs > millis()) ? (scanEndMs - millis()) : 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "UP/DOWN sel   BACK cancel   %lus", (unsigned long)(left/1000));
    uiFooter(buf);
  } else {
    uiFooter("UP/DOWN sel  OK add  HOLD BACK scan");
  }
}

static void drawFriendsRequesting() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("REQUEST");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 44);
  tft.print("Waiting for accept...");

  tft.drawRect(10, 78, 220, 80, COLDARK);
  uiMsgCenter(requestTargetNick, 110, COLNEON, 2);

  uiFooter("Please wait...");
}

static void drawFriendReqPrompt() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("FRIEND REQ");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 44);
  tft.print("Incoming request:");

  tft.drawRect(10, 78, 220, 80, COLDARK);
  uiMsgCenter(frfReqNick, 110, COLNEON, 2);

  uiFooter("OK accept   BACK reject");
}

static bool okHoldDetect(uint16_t holdMs) {
  static uint32_t t0 = 0;
  if (isPressed(PUSH3PIN)) {
    if (t0 == 0) t0 = millis();
    if (millis() - t0 >= holdMs) { t0 = 0; return true; }
  } else {
    t0 = 0;
  }
  return false;
}

static void drawTheKey() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("THE KEY");

  const int boxY = 78;
  const int boxH = 90;
  tft.drawRect(10, boxY, 220, boxH, COLDARK);

  // ===== UNLOCKED VIEW =====
  if (keyVerified) {
    tft.setTextSize(1);
    tft.setTextColor(COLDIM);
    tft.setCursor(10, 48);
    tft.print("Unlocked");

    char unlockMsg[7]; keyDecodeUnlock(unlockMsg);
    keyDrawInCodeArea(unlockMsg, COLNEON, 3);

    uiFooter("BACK exit");
    return;
  }

  // ===== LOCKED (ENTER CODE) VIEW =====
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("hold OK submit   HOLD BACK exit");

  uiMsgCenter("ENTER CODE", boxY + 16, COLDIM, 1);

  tft.setTextSize(3);
  tft.setTextColor(COLNEON);

  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(keyInput, 0, 0, &x1, &y1, &w, &h);
  int x = (240 - (int)w) / 2;
  int y = boxY + 36;
  tft.setCursor(x, y);
  tft.print(keyInput);

  const int stepX = 18;
  int ux = x + (int)keyPos * stepX;
  int uy = y + 24;
  tft.drawFastHLine(ux, uy, 14, COLNEON);

  uiFooter("UP/DOWN digit   OK next   BACK prev");
}

static void drawFriendsList() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("FRIENDS LIST");

  tft.drawRect(FRIENDS_LIST_X, FRIENDS_LIST_Y, FRIENDS_LIST_W, FRIENDS_LIST_H, COLDARK);

  uint8_t total = friendsCount;
  if (friendsListIndex >= total && total > 0) friendsListIndex = total - 1;
  if (total == 0) friendsListIndex = 0;

  uint8_t topIndex = 0;
  if (total > FRIENDS_VISIBLE_ROWS) {
    if (friendsListIndex < (FRIENDS_VISIBLE_ROWS/2)) topIndex = 0;
    else {
      int desired = (int)friendsListIndex - (int)(FRIENDS_VISIBLE_ROWS/2);
      int maxTop = (int)total - (int)FRIENDS_VISIBLE_ROWS;
      if (desired > maxTop) desired = maxTop;
      if (desired < 0) desired = 0;
      topIndex = (uint8_t)desired;
    }
  }

  tft.fillRect(FRIENDS_LIST_X+1, FRIENDS_LIST_Y+1, FRIENDS_LIST_W-2, FRIENDS_LIST_H-2, COLBLACK);

  for (uint8_t r=0; r<FRIENDS_VISIBLE_ROWS; r++) {
    uint8_t idx = topIndex + r;
    if (idx >= total) break;

    int y = FRIENDS_LIST_Y + 16 + r*FRIENDS_ROW_H;
    bool sel = (idx == friendsListIndex);

    const FriendEntry &f = friends[idx];

    if (sel) {
      tft.fillRoundRect(FRIENDS_LIST_X+6, y-4, FRIENDS_LIST_W-16, 24, 6, COLDARK);
      tft.drawRoundRect(FRIENDS_LIST_X+6, y-4, FRIENDS_LIST_W-16, 24, 6, COLNEON);
    }

    tft.setTextSize(2);
    tft.setTextColor(sel ? COLNEON : COLDIM);
    tft.setCursor(FRIENDS_LIST_X+12, y);
    tft.print(f.nick[0] ? f.nick : "UNKNOWN");

    tft.setTextSize(1);
    tft.setTextColor(COLDIM);
    int rx = FRIENDS_LIST_X + FRIENDS_LIST_W - 78;
    tft.setCursor(rx, y+5);
    tft.print(BADGE_ROLE_NAMES[min<uint8_t>(f.role, 4)]);
  }

  drawScrollbarGeneric(FRIENDS_LIST_X, FRIENDS_LIST_Y, FRIENDS_LIST_W, FRIENDS_LIST_H, FRIENDS_VISIBLE_ROWS, total, topIndex);

  uiFooter("UP/DOWN sel   BACK del(hold exit)");
}

static void drawFriendsDeleteConfirm(const char* nick8) {
  busForTFT();
  // overlay box
  tft.fillRoundRect(20, 88, 200, 64, 10, COLBLACK);
  tft.drawRoundRect(20, 88, 200, 64, 10, COLNEON);

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(30, 96);
  tft.print("Delete friend?");

  tft.setTextSize(2);
  tft.setTextColor(COLNEON);
  tft.setCursor(30, 112);
  tft.print(nick8);

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(30, 138);
  tft.print("OK=yes   BACK=no");
}

// ======================= Settings editor (extendido) =======================
static uint8_t settingsIndex = 0;

enum SettingsEditMode : uint8_t {
  EDIT_NONE = 0,
  EDIT_FREQ,
  EDIT_LED_BRIGHT,
  EDIT_THEME,
  EDIT_LEDSET,
  EDIT_BUZZER,
  EDIT_BUZZER_VOL,
  EDIT_NICK,
  EDIT_BADGE_ROLE,
  EDIT_FACTORY_RESET,
  EDIT_UI_THEME,
  EDIT_SAVER
};

static SettingsEditMode settingsEdit = EDIT_NONE;

static uint32_t      editFoxFreq100;
static uint8_t       editLedBrightness;
static LedTheme      editLedTheme;
static LedSet        editLedSet;
static bool          editBuzzerEnabled;
static uint8_t       editBuzzerVolume;
static BadgeRole     editBadgeRole;
static uint8_t       editSaverIndex;
static uint8_t       factoryOkCount = 0;
static uint32_t      factoryOkWindowMs = 0;

static const int SETTINGS_LIST_X = 10;
static const int SETTINGS_LIST_W = 220;
static const int SETTINGS_LIST_Y = 42;
static const int SETTINGS_LIST_H = 160;
static const int SETTINGS_ROW_H  = 34;
static const uint8_t SETTINGS_VISIBLE_ROWS = 4;

enum SettingsItem : uint8_t {
  S_NICK = 0,
  S_BADGE_ROLE,
  S_FREQUENCY,
  S_UI_THEME,
  S_LED_BRIGHT,
  S_LED_THEME,
  S_LED_SET,
  S_BUZZER,
  S_BUZZER_VOL,
  S_SAVER,
  S_FACTORY_RESET,
  S_COUNT
};

static const char* SETTINGS_TITLES[S_COUNT] = {
  "Nickname",
  "Badge Role",
  "Frequency",
  "UI Theme",
  "LED Bright",
  "LED Theme",
  "LED Set",
  "Buzzer",
  "Buzzer Volume",
  "Lock Screen",
  "Factory Reset"
};

static void settingsValueStr(uint8_t i, char* out, size_t outLen) {
  out[0] = 0;

  if (i == S_FREQUENCY) {
    uint32_t mhz = foxFreq100 / 100;
    uint32_t dec = foxFreq100 % 100;
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%lu.%02luMHz", (unsigned long)mhz, (unsigned long)dec);
    strlcpy(out, tmp, outLen);
  } else if (i == S_LED_BRIGHT) {
    snprintf(out, outLen, "%u", (unsigned)min<uint8_t>(ledBrightness, LED_BRIGHT_MAX_SAFE));
  } else if (i == S_LED_THEME) {
    snprintf(out, outLen, "%s", THEME_NAMES[(uint8_t)ledTheme]);
  } else if (i == S_LED_SET) {
    snprintf(out, outLen, "%s", LEDSET_NAMES[(uint8_t)ledSetPrincipal]);
  } else if (i == S_BUZZER) {
    snprintf(out, outLen, "%s", buzzerEnabled ? "ON" : "OFF");
  } else if (i == S_BUZZER_VOL) {
    const char* vnames[] = { "Low", "Medium", "High" };
    snprintf(out, outLen, "%s", vnames[buzzerVolume]);
  } else if (i == S_NICK) {
    snprintf(out, outLen, "%s", badgeNick);
  } else if (i == S_BADGE_ROLE) {
    snprintf(out, outLen, "%s", BADGE_ROLE_NAMES[(uint8_t)badgeRole]);
  } else if (i == S_UI_THEME) {
    snprintf(out, outLen, "%s", UI_THEME_NAMES[(uint8_t)uiTheme]);
  } else if (i == S_SAVER) {
    snprintf(out, outLen, "%s", SAVERS[selectedSaver].name);
  } else if (i == S_FACTORY_RESET) {
    snprintf(out, outLen, "%s", "CLEAR ALL");
  }
}

static void drawSettingsSaverEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("LOCK SCREEN");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select lock screen wallpaper");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(SAVERS[editSaverIndex].name);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettings() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("SETTINGS");
  uiSmallStatusRX();

  tft.drawRect(SETTINGS_LIST_X, SETTINGS_LIST_Y, SETTINGS_LIST_W, SETTINGS_LIST_H, COLDARK);

  uint8_t total = (uint8_t)S_COUNT;
  if (settingsIndex >= total) settingsIndex = 0;

  uint8_t topIndex = 0;
  if (total > SETTINGS_VISIBLE_ROWS) {
    if (settingsIndex < (SETTINGS_VISIBLE_ROWS/2)) topIndex = 0;
    else {
      int desired = (int)settingsIndex - (int)(SETTINGS_VISIBLE_ROWS/2);
      int maxTop = (int)total - (int)SETTINGS_VISIBLE_ROWS;
      if (desired > maxTop) desired = maxTop;
      if (desired < 0) desired = 0;
      topIndex = (uint8_t)desired;
    }
  }

  tft.fillRect(SETTINGS_LIST_X+1, SETTINGS_LIST_Y+1, SETTINGS_LIST_W-2, SETTINGS_LIST_H-2, COLBLACK);

  for (uint8_t r=0; r<SETTINGS_VISIBLE_ROWS; r++) {
    uint8_t idx = topIndex + r;
    if (idx >= total) break;

    int y = SETTINGS_LIST_Y + 15 + r*SETTINGS_ROW_H;
    bool sel = (idx == settingsIndex);

    if (sel) {
      tft.fillRoundRect(SETTINGS_LIST_X+8, y-4, SETTINGS_LIST_W-18, 32, 6, COLDARK);
      tft.drawRoundRect(SETTINGS_LIST_X+8, y-4, SETTINGS_LIST_W-18, 32, 6, COLNEON);
    }

    tft.setTextSize(2);
    tft.setTextColor(sel ? COLNEON : COLDIM);
    tft.setCursor(SETTINGS_LIST_X+16, y);
    tft.print(SETTINGS_TITLES[idx]);

    char vbuf[32];
    settingsValueStr(idx, vbuf, sizeof(vbuf));

    if (sel) {
      tft.setTextSize(1);
      tft.setTextColor(COLDIM);
      tft.setCursor(SETTINGS_LIST_X+16, y+18);
      tft.print(vbuf);
    } else {
      tft.setTextSize(1);
      tft.setTextColor(COLDIM);
      int vx = SETTINGS_LIST_X + SETTINGS_LIST_W - 72;
      tft.setCursor(vx, y+16);
      tft.print(vbuf);
    }
  }

  drawScrollbarGeneric(SETTINGS_LIST_X, SETTINGS_LIST_Y, SETTINGS_LIST_W, SETTINGS_LIST_H, SETTINGS_VISIBLE_ROWS, total, topIndex);

  uiFooter("UP/DOWN sel   OK edit   BACK return");
}

static void drawSettingsFoxFreqEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("FREQUENCY");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Step: 0.05 MHz  (UP/DOWN)");

  uint32_t mhz = editFoxFreq100 / 100;
  uint32_t dec = editFoxFreq100 % 100;

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(mhz);
  tft.print(".");
  if (dec < 10) tft.print("0");
  tft.print(dec);
  tft.print("MHz");

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettingsLedEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("LED BRIGHT");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("UP/DOWN set (10)");

  int barX=14, barY=92, barW=212, barH=18;
  tft.drawRect(barX, barY, barW, barH, COLDARK);
  tft.drawRect(barX-1, barY-1, barW+2, barH+2, COLNEON);

  int fillW = (int)((barW-2) * (editLedBrightness / (float)LED_BRIGHT_MAX_SAFE));
  tft.fillRect(barX+1, barY+1, barW-2, barH-2, COLBLACK);
  if (fillW > 0) tft.fillRect(barX+1, barY+1, fillW, barH-2, COLNEON);

  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(14, 130);
  tft.print((int)editLedBrightness);
  if (editLedBrightness > LED_BRIGHT_MAX_SAFE) {
    tft.setTextSize(1);
    tft.setTextColor(COLDIM);
    tft.setCursor(14, 170);
    tft.print("CAPPED (SAFE)");
  }

  uiFooter("UP/DOWN set   OK save   BACK cancel");
}

static void drawSettingsThemeEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("LED THEME");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select your preferred LED theme");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(THEME_NAMES[(uint8_t)editLedTheme]);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettingsUiThemeEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("UI THEME");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select your preferred UI color");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(UI_THEME_NAMES[(uint8_t)editUiTheme]);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettingsLedSetEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("LED SET");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select your preferred LED set");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(LEDSET_NAMES[(uint8_t)editLedSet]);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettingsBuzzerEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("BUZZER");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select your preferred buzzer state");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(editBuzzerEnabled ? "ON" : "OFF");

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawSettingsBuzzerVolEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("BUZZER VOLUME");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Select your preferred buzzer volume");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  const char* vnames[] = { "Low", "Medium", "High" };
  tft.print(vnames[editBuzzerVolume]);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static char nickNextChar(char c, int dir) {
  const char set[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
  const int n = (int)(sizeof(set)-1);

  int idx = 0;
  for (int i=0;i<n;i++) if (set[i]==c) { idx=i; break; }
  idx += dir;
  if (idx < 0) idx = n-1;
  if (idx >= n) idx = 0;
  return set[idx];
}


static void drawNickEditBase(bool firstBoot) {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();

  if (firstBoot) {
    uiTitle("SET YOUR NICK");
    tft.setTextSize(1);
    tft.setTextColor(COLDIM);
    tft.setCursor(10, 48);
    tft.print("hold OK to save (required)");
  } else {
    uiTitle("NICKNAME");
    tft.setTextSize(1);
    tft.setTextColor(COLDIM);
    tft.setCursor(10, 48);
    tft.print("hold OK save   hold BACK cancel");
  }

  tft.drawRect(10, 82, 220, 70, COLDARK);

  tft.setTextSize(3);
  tft.setTextColor(COLNEON);

  const int baseX = 26;
  const int baseY = 104;
  const int stepX = 26;   

  for (uint8_t i = 0; i < 7; i++) {
    char c = editNick[i];
    tft.setCursor(baseX + (i * stepX), baseY);
    tft.print(c);
  }

  int cx = baseX + (editNickPos * stepX);
  tft.drawFastHLine(cx, 136, 16, COLNEON);

  uiFooter("UP/DOWN edit   OK next   BACK prev");
}

static void drawSettingsNickEdit() { drawNickEditBase(false); }

static void drawFirstBootNickEdit() { drawNickEditBase(true); }

static void drawSettingsBadgeRoleEdit() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("BADGE ROLE");

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 48);
  tft.print("Choose your role");

  tft.drawRect(10, 82, 220, 70, COLDARK);
  tft.setTextSize(3);
  tft.setTextColor(COLNEON);
  tft.setCursor(26, 104);
  tft.print(BADGE_ROLE_NAMES[(uint8_t)editBadgeRole]);

  uiFooter("UP/DOWN edit   OK save   BACK cancel");
}

static void drawFactoryReset() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("FACTORY RST");

  tft.setTextSize(1);
  tft.setTextColor(COLRED);
  tft.setCursor(10, 44);
  tft.print("This will erase ALL progress!");

  tft.drawRect(10, 74, 220, 90, COLDARK);
  uiMsgCenter("DOUBLE OK", 103, COLNEON, 2);
  uiMsgCenter("TO CONFIRM", 131, COLDIM, 1);

  uiFooter("OK twice   BACK cancel");
}

static void doFactoryReset() {
  prefs.begin("hackcon", false);
  prefs.clear();
  prefs.end();
  delay(120);
  ESP.restart();
}

// ===== FOX HUNTING =====
static void drawFoxHunt() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("HUNTING");
  uiSmallStatusRX();

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 46);
  tft.print("Locate the transmitter (RSSI)");

  tft.drawRect(10, 70, 220, 96, COLDARK);
  tft.setTextSize(2);
  tft.setTextColor(COLDIM);
  tft.setCursor(16, 78);
  tft.print("RSSI");

  tft.drawRect(16, 110, 208, 16, COLDIM);

  uiFooter("BACK menu");
}

static void updateFoxHuntBar() {
  int r = (int)foxSmoothedRssi;
  int peak = (int)foxMaxRssi;
  
  // Recalibrated scale: -105 to -45
  if (r < -105) r = -105;
  if (r > -45)  r = -45;
  if (peak < -105) peak = -105;
  if (peak > -45) peak = -45;

  float norm = (r + 105) / 60.0f;
  float pNorm = (peak + 105) / 60.0f;

  int barX=16, barY=110, barW=208, barH=16;

  busForTFT();
  tft.drawRect(barX, barY, barW, barH, COLDARK);
  
  int fillW = (int)((barW-2) * norm);
  static const uint16_t FOXGREEN = 0x07E0; // Fixed neon green — never changes with theme
  uint16_t color = FOXGREEN;
  if (r > -65) color = COLRED;       // Near (Hot)
  else if (r > -85) color = FOXGREEN; // Getting close (Warm) — always green
  else color = COLCYAN;              // Far (Cold)

  tft.fillRect(barX+1, barY+1, fillW, barH-2, color);
  tft.fillRect(barX+1+fillW, barY+1, (barW-2)-fillW, barH-2, COLBLACK);

  // Peak line (vertical white line)
  int px = barX+1 + (int)((barW-2)*pNorm);
  if (px > barX+barW-2) px = barX+barW-2;
  tft.drawFastVLine(px, barY+1, barH-2, COLWHITE);

  // Text values (use specific area fill to reduce flicker)
  tft.fillRect(16, 132, 120, 26, COLBLACK);
  tft.setTextSize(2);
  tft.setTextColor(color);
  tft.setCursor(20, 132);
  tft.print(r); tft.print(" dBm");

  tft.fillRect(140, 132, 84, 26, COLBLACK);
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(156, 138);
  tft.print("PEAK: "); tft.print(peak);
}

// ======================= GAMES =======================
#include "games/Dino.h"
#include "games/Maze.h"
#include "games/HackerMon.h"

// ======================= UI control =======================
static void goTo(ScreenMode m, bool skipIntro) {
  const ScreenMode prevMode = mode;
  const bool prevRf = rfWantedForMode(prevMode);

  // Stop game audio when leaving a game mode
  if (prevMode == MODE_DINO)    { dinoMusicStop();    buzzerNoTone(); }
  if (prevMode == MODE_MAZE)    { gameMusicStop();    buzzerNoTone(); }
  if (prevMode == MODE_HACKMON) { pkmMusicStop();     buzzerNoTone(); }

  // Change mode first (UI uses this)
  mode = m;
  currentLedSet = selectLedSetForMode(mode);

  // BUG3/BUG4 FIX: marcar reset de statics al entrar ciertos modos
  if (m == MODE_FRIENDS_SCAN)       frfScanNeedReset = true;
  if (m == MODE_FRIENDS_LIST)       frfListNeedReset = true;
  if (m == MODE_FRIENDS_REQUESTING) frfReqNeedReset  = true;

  const bool isRf = rfWantedForMode(mode);

  // Decide RF profile BEFORE enforcing RF policy
  if (mode == MODE_FOX_HUNT) {
    radioProfile = PROFILE_FOX_OOK_RSSI;
  } else if (mode == MODE_LEARNING) {
    radioProfile = PROFILE_PACKET_GFSK;
  } else if (frfWantedForMode(mode)) {
    radioProfile = PROFILE_FRIENDS_GFSK_32B;
  }
  // Enforce radio policy:
  // - CC1101 active in Friends, Fox Hunting, and Learning

  // Show OFF transitions before shutting down.
  if (!systemSuspended) {
    if (prevRf && !isRf) uiFlashStatus("RADIO", "RX OFF", 800);
  }

  // Apply power state changes (these must be the ONLY places that turn radios on/off)
  rfEnsureForMode(mode);

  // Show ON transitions after enabling.
  if (!systemSuspended) {
    if (!prevRf && isRf) uiFlashStatus("RADIO", "RX ON", 800);
  }

  // Draw / enter modules
  if (mode == MODE_MENU) {
    drawMenu();

  } else if (mode == MODE_FOX_HUNT) {
    lastActivityMs = millis();
    if (!skipIntro) storyFoxIntroShow();
    drawFoxHunt();
    updateFoxHuntBar();

  } else if (mode == MODE_FRIENDS_MENU) {
    drawFriendsMenu();

  } else if (mode == MODE_FRIENDS_SCAN) {
    drawFriendsScan();

  } else if (mode == MODE_FRIENDS_REQUESTING) {
    drawFriendsRequesting();

  } else if (mode == MODE_FRIENDS_LIST) {
    drawFriendsList();

  } else if (mode == MODE_FRIEND_REQ_PROMPT) {
    drawFriendReqPrompt();

  } else if (mode == MODE_SETTINGS) {
    drawSettings();

  } else if (mode == MODE_THE_KEY) {
    lastActivityMs = millis();
    keyVerified = loadKeyVerified();
    if (!skipIntro) {
      questKeyShowIntro();
      if (!keyVerified) keyEnterModule();
    }
    drawTheKey();

  } else if (mode == MODE_THE_BOX) {
    lastActivityMs = millis();
    if (!skipIntro) storyBoxIntroShow();
    drawTheBox();

  } else if (mode == MODE_LEARNING) {
    lastActivityMs = millis();
    // RF is handled by rfEnsureForMode(); do NOT ccWake/ccApplyProfile here.
    learningEnter();

  } else if (mode == MODE_FIRST_BOOT) {
    drawFirstBootNickEdit();

  } else if (mode == MODE_GAMES_MENU) {
    drawGamesMenu();

  } else if (mode == MODE_DINO) {
    lastActivityMs = millis();
    if (skipIntro) {
      // Volver del screensaver: redibujar sin reiniciar la partida
      if      (dinoSt.titleScreen) drawDino();
      else if (dinoSt.gameOver)    dinoDrawGameOver();
      else                         { dinoDrawAll(); dinoMusicStart(); }
    } else {
      drawDino();
    }

  } else if (mode == MODE_MAZE) {
    lastActivityMs = millis();
    if (skipIntro) {
      // Volver del screensaver: redibujar sin reiniciar el laberinto
      if (gamePaused)      gameDrawPauseMenu();
      else if (gameWon)    { gameRenderFrame(); gameDrawHUD(); }
      else                 { gameRenderFrame(); gameDrawHUD(); gameMusicStart(); }
    } else {
      drawGame();
    }

  } else if (mode == MODE_HACKMON) {
    lastActivityMs = millis();
    if (skipIntro) {
      // Volver del screensaver: redibujar la pantalla correcta sin reiniciar
      switch (pkmScreen) {
        case PKM_SCREEN_OVERWORLD: pkmDrawOverworld(); pkmMusicStart(0); break;
        case PKM_SCREEN_BATTLE:    pkmBattleDrawFull(); pkmMusicStart(1); break;
        case PKM_SCREEN_TERMINAL:  pkmDrawTerminal(); break;
        case PKM_SCREEN_RESULT:    pkmDrawResult(pkmResultWon); break;
      }
    } else {
      drawHackerMon();
    }
  }
}

// ======================= SETUP / LOOP =======================
void setup() {
  ledcAttach(BUZZER_PIN, 1000, BUZZER_LEDC_RES);
  ledcWrite(BUZZER_PIN, 0);

  pinMode(PUSH1PIN, INPUT);
  pinMode(PUSH2PIN, INPUT);
  pinMode(PUSH3PIN, INPUT);
  pinMode(PUSH4PIN, INPUT);

  // SPI bus
  pinMode(RF_CS_PIN, OUTPUT);
  digitalWrite(RF_CS_PIN, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(RF_MISO_PIN, INPUT_PULLUP);

  SPI.begin(TFT_SCLK, RF_MISO_PIN, TFT_MOSI);

  busForTFT();
  tft.init(240, 240);
  tft.setRotation(3);
  tft.fillScreen(COLBLACK);

  applyUiTheme(loadUiTheme()); // Apply loaded UI Theme early for boot sequence

  bootSequence();      

  delay(BOOT_STABILIZE_MS);

  // LEDs: init quieto
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.setBrightness(0);
  FastLED.clear(false);
  delay(BOOT_STAGE_GAP_MS);


  // cargar NVS
  ledBrightness   = loadLedBrightness();
  ledTheme        = loadLedTheme();
  ledSetPrincipal = loadLedSetPrincipal();
  buzzerEnabled   = loadBuzzerEnabled();
  buzzerVolume    = loadBuzzerVolume();
  selectedSaver   = loadSaverIndex();

  radioProfile = loadRadioProfile();
  foxFreq100   = loadFoxFreq100();

  selfShortId = calcSelfShortId();
  loadBadgeProfile();
  friendsLoad();
  questAlienLoad();
  keyVerified = loadKeyVerified();
  lastRssiDbm = -120;

  FastLED.clear(false);
  applyLedBrightness(); 

  mode = MODE_MENU;
  modeBeforeSaver = MODE_MENU;

  lastActivityMs = millis();
  if (isFirstBoot) {
    modeBeforeSaver = MODE_FIRST_BOOT;
    goTo(MODE_FIRST_BOOT, true);
  } else {
    enterBootToSaver();
  }
  // goTo(MODE_MENU);
}

void loop() {
  ButtonsEdge b = readButtonsEdge();
  noteActivity(b.any);

  screensaverTick();

  if (bootToSaverActive) {
    if (b.any) {
      exitBootToSaver();
      ledTick();
      delay(1);
      return;
    }

    ledTick();
    delay(10);
    return;
  }

  if (systemSuspended) {
    if (b.any) {
      exitScreensaver();
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    powerLedsTick();
    ledTick();
    delay(10);
    return;
  }

  // BUG5 FIX: bloque dead code eliminado (false && ...) - la logica real esta abajo

  if (frfWantedForMode(mode)) {
    if (mode != MODE_FRIENDS_REQUESTING) frfEnsureHello();
    frfBackgroundTick();

    if (frfSeenReq &&
        mode != MODE_FRIEND_REQ_PROMPT &&
        mode != MODE_FRIENDS_REQUESTING) {

      goTo(MODE_FRIEND_REQ_PROMPT);
      return;
    }
  }


  // ===== FIRST BOOT NICKNAME ======
  if (mode == MODE_FIRST_BOOT) {

    if (b.up) {
      editNick[editNickPos] = nickNextChar(editNick[editNickPos], +1);
      drawFirstBootNickEdit();
    }
    if (b.down) {
      editNick[editNickPos] = nickNextChar(editNick[editNickPos], -1);
      drawFirstBootNickEdit();
    }
    if (b.ok) {
      editNickPos = (editNickPos + 1) % 7;
      drawFirstBootNickEdit();
    }
    if (b.back) {
      editNickPos = (editNickPos == 0) ? 6 : (editNickPos - 1);
      drawFirstBootNickEdit();
    }

    static uint32_t fbOkHoldStart = 0;
    if (isPressed(PUSH3PIN)) { // OK hold
      if (fbOkHoldStart == 0) fbOkHoldStart = millis();
      if (millis() - fbOkHoldStart >= 900) {
        // === SAVE ===
        char tmp[8]; memset(tmp,0,sizeof(tmp));
        memcpy(tmp, editNick, 7);
        for (int i=6;i>=0;i--) {
          if (tmp[i]==' ') tmp[i]=0;
          else break;
        }
        if (tmp[0] == 0) strlcpy(tmp, "ANON", sizeof(tmp));

        memset(badgeNick,0,sizeof(badgeNick));
        strlcpy(badgeNick, tmp, sizeof(badgeNick));
        saveBadgeNick(badgeNick);
        frfEnsureHello();

        beep(15);
        isFirstBoot = false;
        mode = MODE_MENU;
        modeBeforeSaver = MODE_MENU;
        enterBootToSaver();
        fbOkHoldStart = 0;
      }
    } else {
      fbOkHoldStart = 0;
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== MENU =====
  if (mode == MODE_MENU) {
    uint8_t total = menuCount();

    if (b.up)   { menuIndex = (menuIndex == 0) ? (total-1) : (menuIndex-1); drawMenu(); }
    if (b.down) { menuIndex = (menuIndex + 1) % total; drawMenu(); }

    if (b.ok) {
      if (menuIndex == 0) goTo(MODE_FOX_HUNT);
      else if (menuIndex == 1) goTo(MODE_FRIENDS_MENU);
      else if (menuIndex == 2) goTo(MODE_GAMES_MENU);
      else if (keyUnlocked()) {
        if (menuIndex == 3) goTo(MODE_THE_KEY);
        else if (menuIndex == 4) goTo(MODE_THE_BOX);
        else if (menuIndex == 5) goTo(MODE_LEARNING);
        else if (menuIndex == 6) goTo(MODE_SETTINGS);
      } else {
        if (menuIndex == 3) goTo(MODE_THE_BOX);
        else if (menuIndex == 4) goTo(MODE_LEARNING);
        else if (menuIndex == 5) goTo(MODE_SETTINGS);
      }
    }

    // Mantener BACK presionado desde el menú principal = entrar al lock screen inmediatamente
    if (backHoldDetect(800)) {
      enterScreensaver();
      return;
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== Learning =====
  if (mode == MODE_LEARNING) {
    learningTick(b);
    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== THE BOX =====
  if (mode == MODE_THE_BOX) {
    if (backHoldDetect(800)) {
      strlcpy(boxCodeInput, "AAAAAA", sizeof(boxCodeInput));
      boxCodePos = 0;
      goTo(MODE_MENU);
      ledTick();
      delay(1);
      return;
    }

    if (b.back) {
      if (boxCodePos > 0) {
        boxCodePos--;
      } else {
        boxCodePos = 5;
      }
      drawTheBox();
    }

    if (b.up) {
      char &c = boxCodeInput[boxCodePos];
      if (c >= 'A' && c < 'Z') c++;
      else if (c == 'Z') c = 'A';
      else c = 'A';
      drawTheBox();
    }
    if (b.down) {
      char &c = boxCodeInput[boxCodePos];
      if (c > 'A' && c <= 'Z') c--;
      else if (c == 'A') c = 'Z';
      else c = 'A';
      drawTheBox();
    }
    
    if (b.ok) { 
      if (boxCodePos < 5) {
        boxCodePos++;
        drawTheBox();
      } else {
        boxCodePos = 0;
        drawTheBox();
      }
    }

    // HOLD OK TO SEND RF
    if (okHoldDetect(800)) {

        // --- Guard: THE KEY must be solved first ---
        if (!keyVerified) {
          // Orange flash effect
          for (int f=0; f<4; f++) {
            for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = CRGB(255, 80, 0);
            ledsMarkDirty(); ledsCommitIfNeeded();
            if (buzzerEnabled) { ledcChangeFrequency(BUZZER_PIN, 400, BUZZER_LEDC_RES); ledcWrite(BUZZER_PIN, _buzzerDuty()); }

            uint32_t t0 = millis();
            while (millis() - t0 < 80) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); }

            for (uint8_t j=0; j<NUM_LEDS; j++) leds[j] = CRGB::Black;
            ledsMarkDirty(); ledsCommitIfNeeded();
            if (buzzerEnabled) ledcWrite(BUZZER_PIN, 0);
            
            t0 = millis();
            while (millis() - t0 < 80) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); }
          }

          busForTFT();
          tft.fillScreen(COLBLACK);
          uiFrame();
          uiTitle("THE BOX");
          uiMsgCenter("ERROR!", 110, 0xFBE0, 3); // orange
          {
            uint32_t t0 = millis();
            while (millis() - t0 < 800) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); }
          }

          storyFoxPage(
            "ERROR",
            "NICE TRY",
            "",
            "Nobody reads a book",
            "from the last page.",
            "OK close",
            false
          );

          drawTheBox();
          powerLedsTick();
          ledTick();
          delay(1);
          return;
        }
        // --- End guard ---
        busForTFT();
        tft.fillScreen(COLBLACK);
        uiFrame();
        uiTitle("THE BOX");
        uiMsgCenter("TRANSMITTING...", 110, COLNEON, 2);
        
        RadioProfile oldProf = radioProfile;
        ccApplyProfile(PROFILE_FRIENDS_GFSK_32B);

        // Envío directo de la clave en texto plano hacia THE BOX
        uint8_t payload[32];
        memset(payload, 0, 32);
        memcpy(payload, boxCodeInput, 6);
        memcpy(payload + 6, badgeNick, 8);
        payload[14] = (uint8_t)(selfShortId & 0xFF);
        payload[15] = (uint8_t)(selfShortId >> 8);

        // Disparamos 5 veces por fiabilidad
        for (int i=0; i<5; i++) {
          ccSendFixed32(payload);
          delay(50);
        }

        ccApplyProfile(oldProf);

        auto RFL_Hash_Local = [](const uint8_t* data, size_t length) -> uint32_t {
          uint32_t hash = 2166136261u;
          for (size_t i = 0; i < length; i++) {
            hash ^= data[i];
            hash *= 16777619u;
          }
          return hash;
        };
        
        uint32_t currentHash = RFL_Hash_Local((const uint8_t*)boxCodeInput, 6);
        bool isCorrect = (currentHash == 0x0F044E3B);

        if (isCorrect) {
          uiMsgCenter("ACCESS GRANTED", 150, COLNEON, 2);
          l5AccessEffect(true);  // Destellos Verdes
          
          uint32_t t0 = millis();
          while (millis() - t0 < 1000) {
            screensaverTick();
            powerLedsTick();
            ledTick();
            delay(5);
          }
          
          // FLAG 4
          tft.fillScreen(COLBLACK);
          uiFrame();
          uiTitle("FLAG 4 DISCOVERED");
          uiMsgCenter("HCRD{B0X-H4CK3D}", 110, COLNEON, 2);
          uiFooter("OK to continue");
          
          // FLAG 4
          bool f4Aborted = false;
          while (true) {
            screensaverTick();
            if (systemSuspended) {
              ButtonsEdge bw = readButtonsEdge();
              if (bw.any) {
                exitScreensaver();  // goTo(THE_BOX, skipIntro=true): enciende RF, redibuja modulo
                f4Aborted = true;
                break;
              }
              powerLedsTick(); ledTick(); delay(10); continue;
            }
            ButtonsEdge bw = readButtonsEdge();
            noteActivity(bw.any);
            if (bw.ok) break;
            powerLedsTick(); ledTick(); delay(10);
          }
          if (f4Aborted) { powerLedsTick(); ledTick(); delay(1); return; }
        } else {
          uiMsgCenter("ACCESS DENIED", 150, COLRED, 2);
          l5AccessEffect(false); // Destellos Rojos
        }

        boxCodePos = 0; // reset cursor
        drawTheBox();
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== THE KEY =====
  if (mode == MODE_THE_KEY) {
    if (!keyUnlocked()) { goTo(MODE_MENU); powerLedsTick(); ledTick(); delay(1); return; }
    
    if (keyVerified) {
      if (b.back) {
        goTo(MODE_MENU);
        powerLedsTick();
        ledTick();
        delay(1);
        return;
      }
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (backHoldDetect(800)) {
      keyClearInput();         
      goTo(MODE_MENU);
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (b.back) {
      if (keyPos > 0) {
        keyPos--;
      } else {
        keyPos = 5;
      }
      drawTheKey();
    }

    if (b.up) {
      char &c = keyInput[keyPos];
      c = (c == '9') ? '0' : (char)(c + 1);
      drawTheKey();
    }
    if (b.down) {
      char &c = keyInput[keyPos];
      c = (c == '0') ? '9' : (char)(c - 1);
      drawTheKey();
    }

    if (b.ok) {
      keyJustEntered = true;
      keyPos = (keyPos + 1) % 6;
      drawTheKey();
    }

    if (okHoldDetect(900)) {
      bool ok = keyIsCorrectInCurrentOrder();

      if (ok) {
        keyVerified = true;
        saveKeyVerified(true);

        questAlertEffect();

        keyDecryptAnimationThenMessage("UNLOCKED", COLNEON);
        {
          uint32_t t0 = millis();
          while (millis() - t0 < 1200) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); }
        }

        keyDrawInCodeArea("ERROR!", COLRED, 3);
        // Red LED blinking effect
        for (int i = 0; i < 6; i++) {
          for (int j = 0; j < NUM_LEDS; j++) leds[j] = CRGB::Red;
          FastLED.show();
          beep(20);
          { uint32_t t0 = millis(); while (millis() - t0 < 120) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); } }
          for (int j = 0; j < NUM_LEDS; j++) leds[j] = CRGB::Black;
          FastLED.show();
          { uint32_t t0 = millis(); while (millis() - t0 < 120) { screensaverTick(); powerLedsTick(); /* ledTick(); */ delay(1); } }
        }

        storyFoxPage(
          "CHAPTER III",
          "DECODER ERROR!",
          "",
          "An error occurred while",
          "decoding the key completely.",
          "OK show key",
          true
        );

        // FLAG 3
        tft.fillScreen(COLBLACK);
        uiFrame();
        uiTitle("FLAG 3 DISCOVERED");
        keyDrawInCodeArea("HCRD{K3Y-0P3N3D}", COLNEON, 2);
        uiFooter("OK to continue");
        
        // FLAG 3 — espera OK press; permite screensaver (al despertar: rfEnsureForMode + no intro)
        bool f3Aborted = false;
        while (true) {
          screensaverTick();
          if (systemSuspended) {
            ButtonsEdge bw = readButtonsEdge();
            if (bw.any) {
              exitScreensaver();  // goTo(THE_KEY, skipIntro=true): aplica RF, redibuja modulo
              f3Aborted = true;
              break;
            }
            powerLedsTick(); ledTick(); delay(10); continue;
          }
          ButtonsEdge bw = readButtonsEdge();
          noteActivity(bw.any);
          if (bw.ok) break;
          powerLedsTick(); ledTick(); delay(10);
        }
        if (f3Aborted) { powerLedsTick(); ledTick(); delay(1); return; }

        tft.fillScreen(COLBLACK);
        uiFrame();
        uiTitle("THE KEY");
        char unlockMsg2[7]; keyDecodeUnlock(unlockMsg2);
        keyDrawInCodeArea(unlockMsg2, COLNEON, 3);
      } else {
        keyDrawInCodeArea("INVALID", COLDIM, 3);
        beep(10);
        uint32_t t0 = millis();
        while (millis() - t0 < 1200) {
          screensaverTick();
          powerLedsTick();
          ledTick();
          delay(5);
        }
      }

      drawTheKey();
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FRIEND REQ PROMPT =====
  if (mode == MODE_FRIEND_REQ_PROMPT) {
    if (frfReqPromptStartMs != 0 && (uint32_t)(millis() - frfReqPromptStartMs) > 10500) {
      lastIncomingReqFrom = frfReqFrom;
      incomingReqIgnoreUntil = millis() + 4000;

      frfSeenReq = false;
      frfReqFrom = 0;
      memset(frfReqNick, 0, sizeof(frfReqNick));
      frfReqBro = 0;
      frfReqPromptStartMs = 0;

      busForTFT();
      tft.fillRect(10, 170, 220, 30, COLBLACK);
      uiMsgCenter("TIMEOUT", 176, COLDIM, 2);
      delay(180);

      goTo(modeBeforePrompt);
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (b.ok) {
      uint16_t from = frfReqFrom;
      uint8_t bro  = frfReqBro;

      // We received the friend request and showed it to the user. They accepted.
      // We already have their Nickname in frfReqNick from the original REQ packet.
      const char* nickUse = frfReqNick[0] ? frfReqNick : "FRIEND";
      bool wasArmed = questAlienArmed;
      bool added = friendsAdd(from, nickUse, bro);
      
      // Authorize and send our own nickname back to them via an ACK BEFORE doing long blocking UI stuff
      frfSendAckBurst(from, frfReqNonce);

      lastIncomingReqFrom = from;
      incomingReqIgnoreUntil = millis() + 4000;

      frfSeenReq = false;
      frfReqFrom = 0;
      memset(frfReqNick, 0, sizeof(frfReqNick));
      frfReqBro = 0;

      bool questCompleted = false;
      if (added && wasArmed && broIsAlien(bro)) {
        questCompleted = questAlienOnAlienAdded(from);
      }
      questAlienArmIfNeeded();
      
      if (!wasArmed && questAlienArmed) {
        lastActivityMs = millis();
        questAlienShowUnlocked();
      }

      if (questCompleted) {
        return;
      }

      busForTFT();
      tft.fillRect(4, 170, 232, 34, COLBLACK);
      if (added) {
        uiMsgCenter("ADDED", 176, COLNEON, 2);
        beep(12);
        delay(140);
      } else {
        if (friendsCount >= FRIENDS_MAX) uiMsgCenter("BOX FULL", 176, COLDIM, 2);
        else uiMsgCenter("EXISTS", 176, COLDIM, 2);
        delay(140);
      }

      goTo(MODE_FRIENDS_MENU);
      return;
    }
    if (b.back) {
      frfSendNackBurst(frfReqFrom, frfReqNonce);
      uint16_t from = frfReqFrom;

      lastIncomingReqFrom = from;
      incomingReqIgnoreUntil = millis() + 4000;

      frfSeenReq = false;
      frfReqFrom = 0;
      memset(frfReqNick, 0, sizeof(frfReqNick));
      frfReqBro = 0;

      busForTFT();
      tft.fillRect(10, 170, 220, 30, COLBLACK);
      uiMsgCenter("REJECTED", 176, COLDIM, 2);
      delay(180);


      goTo(modeBeforePrompt);
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FRIENDS MENU =====
  if (mode == MODE_FRIENDS_MENU) {
    if (b.up)   { friendsMenuIndex = (friendsMenuIndex==0)?(FRIENDS_MENU_COUNT-1):(friendsMenuIndex-1); drawFriendsMenu(); }
    if (b.down) { friendsMenuIndex = (friendsMenuIndex+1)%FRIENDS_MENU_COUNT; drawFriendsMenu(); }

    if (b.ok) {
      if (friendsMenuIndex == 0) {
        friendsScanClear(6000);          // <-- usa el cache RF real
        scanIndex = 0;                   // UI
        scanPhaseCollecting = true;
        scanEndMs = frfScanEndMs;        // <-- sincroniza el countdown del footer

        goTo(MODE_FRIENDS_SCAN);         // <-- importante: aplica políticas de RF y dibuja
      } else if (friendsMenuIndex == 1) {
        friendsListIndex = 0;
        goTo(MODE_FRIENDS_LIST);
      }
    }

    if (b.back) goTo(MODE_MENU);

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FRIENDS SCAN =====
  if (mode == MODE_FRIENDS_SCAN) {

    static uint8_t  lastHits = 255;
    static uint32_t lastDraw = 0;

    // BUG4 FIX: resetear statics al (re-)entrar al modo
    if (frfScanNeedReset) {
      frfScanNeedReset = false;
      lastHits = 255;
      lastDraw = 0;
    }

    if (scanPhaseCollecting) {
      if (b.back) { scanPhaseCollecting = false; goTo(MODE_FRIENDS_MENU); powerLedsTick(); ledTick(); delay(1); return; }
      // BUG6 FIX: frfEnsureHello() ya es llamado en el bloque global de frfWantedForMode (linea ~4817)
      // no llamarlo de nuevo aqui para evitar doble llamada por ciclo de loop

      const uint32_t now = millis();
      if (scanHitsCount != lastHits) {
        lastHits = scanHitsCount;
        drawFriendsScan();
        drawFriendsScanFooter();
        lastDraw = now;
      } else if ((now - lastDraw) >= 200) {
        lastDraw = now;
        drawFriendsScan();       // Fuerza actualización de animación del título
        drawFriendsScanFooter(); // Fuerza actualización del timer
      }

      if ((int32_t)(now - scanEndMs) >= 0) {
        scanPhaseCollecting = false;
        scanIndex = 0;
        drawFriendsScan();
        drawFriendsScanFooter();
      }
      
      // Permitir navegar por la lista incluso si sigue "buscando" de fondo
      if (b.up && scanHitsCount > 0)   { scanIndex = (scanIndex==0)?(scanHitsCount-1):(scanIndex-1); drawFriendsScan(); }
      if (b.down && scanHitsCount > 0) { scanIndex = (scanIndex+1)%scanHitsCount; drawFriendsScan(); }

      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    static uint32_t backDownMs = 0;
    static bool backHoldFired = false;
    const uint16_t HOLD_MS = 600;

    if (isPressed(PUSH4PIN)) {
      if (backDownMs == 0) {
        backDownMs = millis();
        backHoldFired = false;
      }
      if (!backHoldFired && (millis() - backDownMs) >= HOLD_MS) {
        backHoldFired = true;
        friendsScanClear(6000); // Trigger a 6-second rescan
        scanIndex = 0;
        scanPhaseCollecting = true;
        scanEndMs = frfScanEndMs;
        drawFriendsScan();
        drawFriendsScanFooter();
        // Clear queue slightly to avoid ghosts
        b.back = false;
      }
    } else {
      if (backDownMs != 0) {
        uint32_t held = millis() - backDownMs;
        backDownMs = 0;
        if (!backHoldFired && held < HOLD_MS) {
          scanPhaseCollecting = false; 
          goTo(MODE_FRIENDS_MENU); 
          powerLedsTick();
          ledTick(); 
          delay(1); 
          return;
        }
      }
    }

    if (b.up && scanHitsCount > 0)   { scanIndex = (scanIndex==0)?(scanHitsCount-1):(scanIndex-1); drawFriendsScan(); }
    if (b.down && scanHitsCount > 0) { scanIndex = (scanIndex+1)%scanHitsCount; drawFriendsScan(); }

    if (b.ok && scanHitsCount > 0) {
      ScanHit &h = scanHits[scanIndex];

      if (h.id == selfShortId) {
        beep(10);
      } else if (friendsIndexById(h.id) >= 0) {
        busForTFT();
        uiMsgCenter("ADDED", 186, COLDIM, 2);
        beep(10);
        delay(180);
        drawFriendsScan();
      } else {
        requestTargetId = h.id;
        memset(requestTargetNick, 0, sizeof(requestTargetNick));
        strlcpy(requestTargetNick, h.nick, sizeof(requestTargetNick));

        frfSeenAck = false;
        frfAckFrom = 0;

        frfSendReqTo(requestTargetId, 2200);
        scanPhaseCollecting = false;  // BUG9 FIX: salir de fase collecting antes de ir a REQUESTING
        goTo(MODE_FRIENDS_REQUESTING);
      }
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FRIENDS REQUESTING =====
  if (mode == MODE_FRIENDS_REQUESTING) {
    static const uint32_t REQ_TIMEOUT_MS = 10000;
    static const uint16_t resendIntervals[] = { 650, 900, 1200, 1600 };
    static uint8_t resendStage = 0;
    static uint32_t reqStartMs = 0;
    static uint32_t lastResend = 0;

    // BUG3 FIX: resetear statics al (re-)entrar al modo para evitar timeout inmediato
    if (frfReqNeedReset) {
      frfReqNeedReset = false;
      reqStartMs  = 0;
      lastResend  = 0;
      resendStage = 0;
    }

    if (reqStartMs == 0) {
      reqStartMs = millis();
      lastResend = 0;
      resendStage = 0;
    }

    uint16_t interval = resendIntervals[min<uint8_t>(resendStage, (uint8_t)3)];
    if ((uint32_t)(millis() - lastResend) > interval && requestTargetId != 0) {
      lastResend = millis();
      frfSendReqTo(requestTargetId, 1200);
      if (resendStage < 3) resendStage++;
    }

    // ack?
    if (frfSeenAck && frfAckFrom == requestTargetId) {

      const char* nickUse = requestTargetNick[0] ? requestTargetNick : "FRIEND";
      uint8_t bro = ROLE_SCRPTK1D;

      bool wasArmed = questAlienArmed;

      for (uint8_t i=0; i<scanHitsCount; i++) {
        if (scanHits[i].id == requestTargetId) {
          nickUse = scanHits[i].nick[0] ? scanHits[i].nick : nickUse;
          bro = scanHits[i].bro;
          break;
        }
      }

      bool added = friendsAdd(requestTargetId, nickUse, bro);

      // We have the Nickname directly from the ACK, so no need for Profile bursts or 2-phase commit.
      // Friend is added instantly.

      // clear outgoing req session
      frfOutReqTo = 0;
      frfOutReqNonce = 0;

      frfSeenAck = false;
      frfAckFrom = 0;

      uint16_t addedId = requestTargetId;
      requestTargetId = 0;

      reqStartMs = 0;
      lastResend = 0;

      bool questCompleted = false;
      if (added && wasArmed && broIsAlien(bro)) {
        questCompleted = questAlienOnAlienAdded(addedId);
      }

      questAlienArmIfNeeded();

      if (!wasArmed && questAlienArmed) {
        lastActivityMs = millis();
        questAlienShowUnlocked();
      }

      if (questCompleted) {
        return;
      }

      busForTFT();
      tft.fillRect(4, 170, 232, 34, COLBLACK);
      if (added) {
        uiMsgCenter("ADDED", 176, COLNEON, 2);
        beep(18);
        delay(220);
      } else {
        if (friendsCount >= FRIENDS_MAX) uiMsgCenter("BOX FULL", 176, COLDIM, 2);
        else uiMsgCenter("EXISTS", 176, COLDIM, 2);
        delay(220);
      }

      goTo(MODE_FRIENDS_MENU);
    }

    // nack?
    if (frfSeenNack && frfNackFrom == requestTargetId) {
      frfSeenNack = false;
      frfNackFrom = 0;
      requestTargetId = 0;

      busForTFT();
      tft.fillRect(4, 170, 232, 34, COLBLACK);
      uiMsgCenter("REJECTED", 176, COLDIM, 2);
      beep(10);
      delay(220);

      reqStartMs = 0;
      lastResend = 0;

      frfEnsureHello();

      goTo(MODE_FRIENDS_MENU);
    }

    // Se deshabilitó b.back para esperar forzosamente ACK, NACK o TIMEOUT

    if (reqStartMs != 0 && (uint32_t)(millis() - reqStartMs) > REQ_TIMEOUT_MS) {
      // TIMEOUT
      busForTFT();
      tft.fillRect(4, 170, 232, 34, COLBLACK);
      uiMsgCenter("TIMEOUT", 176, COLDIM, 2);
      delay(220);

      frfEnsureHello();

      requestTargetId = 0;
      reqStartMs = 0;
      lastResend = 0;
      resendStage = 0;

      goTo(MODE_FRIENDS_MENU);
    }

    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FRIENDS LIST =====
  if (mode == MODE_FRIENDS_LIST) {
    static uint32_t backDownMs = 0;
    static bool backHoldFired = false;

    // BUG4 FIX: resetear statics al (re-)entrar al modo
    if (frfListNeedReset) {
      frfListNeedReset = false;
      backDownMs    = 0;
      backHoldFired = false;
    }

    if (friendsCount == 0) {
      if (b.back) goTo(MODE_FRIENDS_MENU);
      if (backHoldDetect(900)) goTo(MODE_FRIENDS_MENU);
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (b.up)   { friendsListIndex = (friendsListIndex==0)?(friendsCount-1):(friendsListIndex-1); drawFriendsList(); }
    if (b.down) { friendsListIndex = (friendsListIndex+1)%friendsCount; drawFriendsList(); }
    
    // ---- BACK handling (short release = delete, hold = exit) ----
    const uint16_t HOLD_MS = 900;

    if (isPressed(PUSH4PIN)) {
      if (backDownMs == 0) {
        backDownMs = millis();
        backHoldFired = false;
      }

      if (!backHoldFired && (millis() - backDownMs) >= HOLD_MS) {
        backHoldFired = true;
        backDownMs = 0;            
        goTo(MODE_FRIENDS_MENU);
        powerLedsTick();
    ledTick();
        delay(1);
        return;
      }
    } else {
      if (backDownMs != 0) {
        uint32_t held = millis() - backDownMs;
        backDownMs = 0;

        if (held < HOLD_MS && friendsListIndex < friendsCount) {
          char nbuf[9]; memset(nbuf, 0, sizeof(nbuf));
          strlcpy(nbuf, friends[friendsListIndex].nick, sizeof(nbuf));
          drawFriendsDeleteConfirm(nbuf);

          while (true) {
            frfBackgroundTick();
            frfEnsureHello();
            screensaverTick();
            if (systemSuspended) {
              ButtonsEdge bw = readButtonsEdge();
              if (bw.any) {
                exitScreensaver();
                // Al salir del saver en este contexto, cerrar dialogo
                drawFriendsList();
              }
              powerLedsTick();
              ledTick();
              delay(10);
              continue;
            }
            ButtonsEdge bb = readButtonsEdge();
            noteActivity(bb.any);
            if (bb.ok) {
              friendsDeleteAt(friendsListIndex);
              if (friendsListIndex >= friendsCount && friendsCount > 0)
                friendsListIndex = friendsCount - 1;
              drawFriendsList();
              beep(12);
              break;
            }
            if (bb.back) {  
              drawFriendsList();
              break;
            }
            powerLedsTick();
    ledTick();
            delay(1);
          }
        }
      }
    }


    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== FOX HUNTING =====
  if (mode == MODE_FOX_HUNT) {
    static uint32_t lastUi=0;
    if (millis() - lastUi > 60) {
      lastUi = millis();
      foxHuntPollLiveRssi();
      updateFoxHuntBar();
    }

    if (b.back) {
      rfPowerDown();
      goTo(MODE_MENU);
    }

    powerLedsTick();
    ledTick(); 
    delay(1);
    return;
  }


  // ===== SETTINGS =====
  if (mode == MODE_SETTINGS) {
    if (settingsEdit == EDIT_NONE) {
      uint8_t total = (uint8_t)S_COUNT;

      if (b.up)   { settingsIndex = (settingsIndex==0)?(total-1):(settingsIndex-1); drawSettings(); }
      if (b.down) { settingsIndex = (settingsIndex+1)%total; drawSettings(); }

      if (b.ok) {
        if (settingsIndex == S_FREQUENCY) {
          settingsEdit = EDIT_FREQ;
          editFoxFreq100 = foxFreq100;
          drawSettingsFoxFreqEdit();
        } else if (settingsIndex == S_LED_BRIGHT) {
          settingsEdit = EDIT_LED_BRIGHT;
          editLedBrightness = min<uint8_t>(ledBrightness, LED_BRIGHT_MAX_SAFE);
          drawSettingsLedEdit();
        } else if (settingsIndex == S_LED_THEME) {
          settingsEdit = EDIT_THEME;
          editLedTheme = ledTheme;
          drawSettingsThemeEdit();
        } else if (settingsIndex == S_LED_SET) {
          settingsEdit = EDIT_LEDSET;
          editLedSet = ledSetPrincipal;
          drawSettingsLedSetEdit();
        } else if (settingsIndex == S_BUZZER) {
          settingsEdit = EDIT_BUZZER;
          editBuzzerEnabled = buzzerEnabled;
          drawSettingsBuzzerEdit();
        } else if (settingsIndex == S_BUZZER_VOL) {
          settingsEdit = EDIT_BUZZER_VOL;
          editBuzzerVolume = buzzerVolume;
          drawSettingsBuzzerVolEdit();
        } else if (settingsIndex == S_NICK) {
          settingsEdit = EDIT_NICK;
          memset(editNick, 0, sizeof(editNick));
          strlcpy(editNick, badgeNick, sizeof(editNick));
          for (uint8_t i=0;i<7;i++) if (editNick[i] == 0) editNick[i] = ' ';
          editNick[7] = 0;
          editNickPos = 0;
          drawSettingsNickEdit();
        } else if (settingsIndex == S_BADGE_ROLE) {
          settingsEdit = EDIT_BADGE_ROLE;
          editBadgeRole = badgeRole;
          drawSettingsBadgeRoleEdit();
        } else if (settingsIndex == S_UI_THEME) {
          settingsEdit = EDIT_UI_THEME;
          editUiTheme = uiTheme;
          drawSettingsUiThemeEdit();
        } else if (settingsIndex == S_SAVER) {
          settingsEdit = EDIT_SAVER;
          editSaverIndex = selectedSaver;
          drawSettingsSaverEdit();
        } else if (settingsIndex == S_FACTORY_RESET) {
          settingsEdit = EDIT_FACTORY_RESET;
          factoryOkCount = 0;
          factoryOkWindowMs = 0;
          drawFactoryReset();
        }
      }

      if (b.back) goTo(MODE_MENU);

      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_FREQ) {
      if (b.up) {
        editFoxFreq100 += 5;
        if (editFoxFreq100 > 44000) editFoxFreq100 = 43000;
        drawSettingsFoxFreqEdit();
      }
      if (b.down) {
        if (editFoxFreq100 <= 43000) editFoxFreq100 = 44000;
        else editFoxFreq100 -= 5;
        drawSettingsFoxFreqEdit();
      }
      if (b.ok) {
        foxFreq100 = editFoxFreq100;
        saveFoxFreq100(foxFreq100);
        if (radioProfile == PROFILE_FOX_OOK_RSSI) {
          ccSetFrequencyMHz100(foxFreq100);
          ccStrobe(CC1101_SRX);
        }
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_LED_BRIGHT) {
      if (b.up) {
        editLedBrightness = (editLedBrightness >= (LED_BRIGHT_MAX_SAFE - 10)) ? LED_BRIGHT_MAX_SAFE : (editLedBrightness + 10);
        FastLED.setBrightness(min<uint8_t>(editLedBrightness, LED_BRIGHT_MAX_SAFE));
        ledsMarkDirty();
        ledsCommitIfNeeded();
        drawSettingsLedEdit();
      }
      if (b.down) {
        editLedBrightness = (editLedBrightness <= 10) ? 0 : (editLedBrightness - 10);
        FastLED.setBrightness(min<uint8_t>(editLedBrightness, LED_BRIGHT_MAX_SAFE));
        ledsMarkDirty();
        ledsCommitIfNeeded();
        drawSettingsLedEdit();
      }
      if (b.ok) {
        ledBrightness = min<uint8_t>(editLedBrightness, LED_BRIGHT_MAX_SAFE);
        saveLedBrightness(ledBrightness);
        applyLedBrightness();
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { applyLedBrightness(); settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_THEME) {
      if (b.up) {
        editLedTheme = (LedTheme)(((uint8_t)editLedTheme + 1) % (uint8_t)THEME_COUNT);
        drawSettingsThemeEdit();
      }
      if (b.down) {
        uint8_t v = (uint8_t)editLedTheme;
        v = (v == 0) ? ((uint8_t)THEME_COUNT - 1) : (v - 1);
        editLedTheme = (LedTheme)v;
        drawSettingsThemeEdit();
      }
      if (b.ok) {
        ledTheme = editLedTheme;
        saveLedTheme(ledTheme);
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_UI_THEME) {
      if (b.up) {
        editUiTheme = (UiTheme)(((uint8_t)editUiTheme + 1) % (uint8_t)UI_THEME_COUNT);
        drawSettingsUiThemeEdit();
      }
      if (b.down) {
        uint8_t v = (uint8_t)editUiTheme;
        v = (v == 0) ? ((uint8_t)UI_THEME_COUNT - 1) : (v - 1);
        editUiTheme = (UiTheme)v;
        drawSettingsUiThemeEdit();
      }
      if (b.ok) {
        uiTheme = editUiTheme;
        saveUiTheme(uiTheme);
        applyUiTheme(uiTheme);
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_LEDSET) {
      if (b.up) {
        editLedSet = (LedSet)(((uint8_t)editLedSet + 1) % (uint8_t)LEDSET_COUNT);
        drawSettingsLedSetEdit();
      }
      if (b.down) {
        uint8_t v = (uint8_t)editLedSet;
        v = (v == 0) ? ((uint8_t)LEDSET_COUNT - 1) : (v - 1);
        editLedSet = (LedSet)v;
        drawSettingsLedSetEdit();
      }
      if (b.ok) {
        ledSetPrincipal = editLedSet;
        saveLedSetPrincipal(ledSetPrincipal);
        currentLedSet = selectLedSetForMode(mode);
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_BUZZER) {
      if (b.up || b.down) {
        editBuzzerEnabled = !editBuzzerEnabled;
        drawSettingsBuzzerEdit();
      }
      if (b.ok) {
        buzzerEnabled = editBuzzerEnabled;
        saveBuzzerEnabled(buzzerEnabled);
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_BUZZER_VOL) {
      if (b.up)   { editBuzzerVolume = (editBuzzerVolume == 0) ? 2 : editBuzzerVolume - 1; drawSettingsBuzzerVolEdit(); }
      if (b.down) { editBuzzerVolume = (editBuzzerVolume + 1) % 3; drawSettingsBuzzerVolEdit(); }
      if (b.ok) {
        buzzerVolume = editBuzzerVolume;
        saveBuzzerVolume(buzzerVolume);
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_NICK) {
      if (b.up) {
        editNick[editNickPos] = nickNextChar(editNick[editNickPos], +1);
        drawSettingsNickEdit();
      }
      if (b.down) {
        editNick[editNickPos] = nickNextChar(editNick[editNickPos], -1);
        drawSettingsNickEdit();
      }
      if (b.ok) {
        editNickPos = (editNickPos + 1) % 7;
        drawSettingsNickEdit();
      }

      if (b.back) {
        editNickPos = (editNickPos == 0) ? 6 : (editNickPos - 1);
        drawSettingsNickEdit();
      }

      static uint32_t okHoldStart = 0;
      if (isPressed(PUSH3PIN)) {
        if (okHoldStart == 0) okHoldStart = millis();
        if (millis() - okHoldStart >= 900) {
          // === SAVE ===
          char tmp[8]; memset(tmp,0,sizeof(tmp));
          memcpy(tmp, editNick, 7);
          for (int i=6;i>=0;i--) {
            if (tmp[i]==' ') tmp[i]=0;
            else break;
          }
          if (tmp[0] == 0) strlcpy(tmp, "ANON", sizeof(tmp));

          memset(badgeNick,0,sizeof(badgeNick));
          strlcpy(badgeNick, tmp, sizeof(badgeNick));
          saveBadgeNick(badgeNick);
          frfEnsureHello();

          beep(15);
          settingsEdit = EDIT_NONE;
          drawSettings();
          okHoldStart = 0;
        }
      } else {
        okHoldStart = 0;
      }

      if (backHoldDetect(900)) {
        settingsEdit = EDIT_NONE;
        drawSettings();
      }

      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_BADGE_ROLE) {
      if (b.up) {
        editBadgeRole = (BadgeRole)(((uint8_t)editBadgeRole + 1) % 5);
        drawSettingsBadgeRoleEdit();
      }
      if (b.down) {
        uint8_t v = (uint8_t)editBadgeRole;
        v = (v == 0) ? 4 : (v - 1);
        editBadgeRole = (BadgeRole)v;
        drawSettingsBadgeRoleEdit();
      }
      if (b.ok) {
        badgeRole = editBadgeRole;
        saveBadgeRole(badgeRole);
        frfEnsureHello();
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
    ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_SAVER) {
      if (b.up) {
        editSaverIndex = (editSaverIndex + 1) % SAVER_COUNT;
        drawSettingsSaverEdit();
      }
      if (b.down) {
        uint8_t v = editSaverIndex;
        v = (v == 0) ? (SAVER_COUNT - 1) : (v - 1);
        editSaverIndex = v;
        drawSettingsSaverEdit();
      }
      if (b.ok) {
        selectedSaver = editSaverIndex;
        saveSaverIndex(selectedSaver);
        screensaverDrawn = false;  // forzar redibujado con el nuevo saver
        beep(15);
        settingsEdit = EDIT_NONE;
        drawSettings();
      }
      if (b.back) { settingsEdit = EDIT_NONE; drawSettings(); }
      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }

    if (settingsEdit == EDIT_FACTORY_RESET) {
      if (b.ok) {
        uint32_t now = millis();
        if (factoryOkCount == 0) {
          factoryOkCount = 1;
          factoryOkWindowMs = now + 1800;
          beep(12);
        } else {
          if ((int32_t)(now - factoryOkWindowMs) < 0) {
            doFactoryReset();
          } else {
            factoryOkCount = 1;
            factoryOkWindowMs = now + 1800;
            beep(12);
          }
        }
      }

      if (b.back) {
        settingsEdit = EDIT_NONE;
        drawSettings();
      }

      if (factoryOkCount == 1 && (int32_t)(millis() - factoryOkWindowMs) >= 0) {
        factoryOkCount = 0;
        factoryOkWindowMs = 0;
      }

      powerLedsTick();
      ledTick();
      delay(1);
      return;
    }
  }
  // goTo(MODE_MENU);

  // ===== Games Menu =====
  if (mode == MODE_GAMES_MENU) {
    if (b.up)   { gamesMenuIndex = (gamesMenuIndex == 0) ? (GAMES_MENU_COUNT-1) : (gamesMenuIndex-1); drawGamesMenu(); }
    if (b.down) { gamesMenuIndex = (gamesMenuIndex + 1) % GAMES_MENU_COUNT; drawGamesMenu(); }
    if (b.ok) {
      if (gamesMenuIndex == 0) goTo(MODE_DINO);
      else if (gamesMenuIndex == 1) goTo(MODE_MAZE);
      else if (gamesMenuIndex == 2) goTo(MODE_HACKMON);
    }
    if (b.back) goTo(MODE_MENU);
    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== Dino Run =====
  if (mode == MODE_DINO) {
    dinoMusicTick();
    dinoTick(b);
    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== Maze =====
  if (mode == MODE_MAZE) {
    gameMusicTick();
    gameTick(b);
    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }

  // ===== HackerMon =====
  if (mode == MODE_HACKMON) {
    pkmMusicTick();
    hackerMonTick(b);
    powerLedsTick();
    ledTick();
    delay(1);
    return;
  }
}
