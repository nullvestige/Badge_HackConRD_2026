// ======================= DINO RUN (Endless Runner) =======================
// Chrome dinosaur-style side-scroller for HackConRD 2026 badge.
//
// Externals required from badge.ino:
//   tft, busForTFT(), buzzerTone(), buzzerNoTone(), buzzerEnabled,
//   isPressed(), PUSH1PIN..PUSH4PIN,
//   uiFrame(), COLDIM, COLDARK, COLBLACK, COLNEON,
//   goTo(), MODE_MENU, MODE_GAMES_MENU, ButtonsEdge, prefs

// ==================== MUSIC ====================
struct DinoNote { uint16_t freq; uint16_t dur; };

// Dino Runner Theme — 8-bit prehistoric adventure, tempo 160 BPM
// wholenote = (60000/160)*4 = 1500ms
// quarter=375, dotted-quarter=563, eighth=188, dotted-eighth=281, half=750
// Freqs: A4=440 B4=494 C5=523 D5=587 E5=659 G5=784 A5=880 B5=988 C6=1047
#define _Q   375  // quarter
#define _QD  563  // dotted quarter
#define _8   188  // eighth
#define _8D  281  // dotted eighth
#define _H   750  // half

static const DinoNote dinoMelody[] PROGMEM = {
  // Phrase A — ascent and descent
  {659,_Q},{784,_Q},{880,_Q},{784,_Q},   // E5 G5 A5 G5
  {659,_Q},{523,_Q},{440,_H},            // E5 C5 A4
  {523,_Q},{587,_Q},{659,_Q},{784,_Q},   // C5 D5 E5 G5
  {880,_QD},{784,_8},{659,_H},           // A5. G5 E5

  // Phrase B — a step higher
  {784,_Q},{880,_Q},{988,_Q},{880,_Q},   // G5 A5 B5 A5
  {784,_Q},{659,_Q},{523,_H},            // G5 E5 C5
  {587,_Q},{659,_Q},{784,_Q},{880,_Q},   // D5 E5 G5 A5
  {988,_QD},{880,_8},{784,_H},           // B5. A5 G5

  // Bridge — peak and fall
  {1047,_Q},{880,_8},{784,_8},{659,_Q},{523,_Q},  // C6 A5 G5 E5 C5
  {440,_Q},{494,_Q},{523,_Q},{587,_Q},            // A4 B4 C5 D5
  {659,_QD},{587,_8},{523,_Q},{440,_Q},           // E5. D5 C5 A4
  {494,_Q},{440,_8},{0,_8},{0,_H},                // B4 A4 REST

  // Phrase A repeat (leads back to loop start)
  {659,_Q},{784,_Q},{880,_Q},{784,_Q},
  {659,_Q},{523,_Q},{440,_H},
  {523,_Q},{587,_Q},{659,_Q},{784,_Q},
  {880,_QD},{784,_8},{659,_H},
  {0,_8},   // rest before loop
};
#undef _Q
#undef _QD
#undef _8
#undef _8D
#undef _H
static const int dinoMelodyLen = sizeof(dinoMelody) / sizeof(dinoMelody[0]);

static int      dinoMusicIdx    = 0;
static uint32_t dinoMusicNextMs = 0;

static void dinoMusicStart() {
  dinoMusicIdx    = 0;
  dinoMusicNextMs = millis();
}
static void dinoMusicStop() {
  if (buzzerEnabled) buzzerNoTone();
}
static void dinoMusicTick() {
  if (!buzzerEnabled) return;
  uint32_t now = millis();
  if (now < dinoMusicNextMs) return;
  const DinoNote n = {
    pgm_read_word(&dinoMelody[dinoMusicIdx].freq),
    pgm_read_word(&dinoMelody[dinoMusicIdx].dur)
  };
  if (n.freq == 0) buzzerNoTone(); else buzzerTone(n.freq);
  dinoMusicNextMs = now + n.dur;
  if (++dinoMusicIdx >= dinoMelodyLen) dinoMusicIdx = 0;
}

// ==================== SFX ====================
// Jump: quick rising chirp (two-tone)
static uint32_t dinoSfxUntil = 0;
static uint8_t  dinoSfxPhase = 0;

static void dinoSfxJump() {
  if (!buzzerEnabled) return;
  buzzerTone(880);
  dinoSfxUntil = millis() + 40;
  dinoSfxPhase = 1;  // will step to 1047 Hz after 40ms
}

static void dinoSfxCrash() {
  if (!buzzerEnabled) return;
  buzzerTone(180);
  dinoSfxUntil = millis() + 80;
  dinoSfxPhase = 10;  // descending buzz: 180→120→80
}

// Call every tick to advance multi-step SFX (runs alongside music)
static void dinoSfxTick() {
  if (!buzzerEnabled || dinoSfxUntil == 0) return;
  uint32_t now = millis();
  if (now < dinoSfxUntil) return;
  dinoSfxUntil = 0;
  if (dinoSfxPhase == 1) {
    // Jump step 2: brief higher chirp
    buzzerTone(1047); dinoSfxUntil = now + 30; dinoSfxPhase = 2;
  } else if (dinoSfxPhase == 2) {
    buzzerNoTone(); dinoSfxPhase = 0;
  } else if (dinoSfxPhase == 10) {
    buzzerTone(120); dinoSfxUntil = now + 80; dinoSfxPhase = 11;
  } else if (dinoSfxPhase == 11) {
    buzzerTone(80); dinoSfxUntil = now + 100; dinoSfxPhase = 12;
  } else if (dinoSfxPhase == 12) {
    buzzerNoTone(); dinoSfxPhase = 0;
  }
}

// ==================== CONSTANTS ====================
#define DINO_W            240
#define DINO_H            240
#define DINO_GROUND_Y     180   // top of ground line (leaves 60px play space above)
#define DINO_HUD_Y        195   // top of HUD strip (15px gap from ground — avoids overlap)
#define DINO_DINO_X        28   // fixed horizontal left edge of dino
#define DINO_DINO_W        18   // dino sprite width (px)
#define DINO_DINO_H_RUN    28   // dino height when running/jumping
#define DINO_DINO_H_DUCK   14   // dino height when ducking
#define DINO_JUMP_VY      -14   // initial upward velocity (negative = up)
#define DINO_GRAVITY        1   // gravity added to vy per tick
#define DINO_MAX_SPEED     12   // max scroll speed (px/tick)
#define DINO_OBS_MAX        3   // max simultaneous obstacles

// Obstacle kinds
#define OBS_CACTUS_S  0   // small cactus, 20px tall
#define OBS_CACTUS_L  1   // tall cactus, 36px tall
#define OBS_BIRD_HI   2   // bird at head height — must duck (y = GROUND_Y-60)
#define OBS_BIRD_LO   3   // bird at torso height — must jump (y = GROUND_Y-35)
#define OBS_ROCK      4   // wide flat rock on ground — must jump, cannot duck
#define OBS_PAIR_SS   5   // small + small  (speed 5+)
#define OBS_PAIR_SL   6   // small + tall   (speed 5+)
#define OBS_PAIR_LS   7   // tall  + small  (speed 5+)
#define OBS_PAIR_LL   8   // tall  + tall   (speed 7+)
#define OBS_TRIO_SSS  9   // S+S+S          (speed 9+)
#define OBS_TRIO_SLS  10  // S+L+S          (speed 9+)
#define OBS_TRIO_LSL  11  // L+S+L          (speed 10+)

// Colors (RGB565 constants, theme-neutral)
#define DINO_COL_SKY    0x0000   // black sky
#define DINO_COL_GROUND 0x2945   // dark grey ground strip
#define DINO_COL_BIRD   0x077F   // cyan bird
#define DINO_COL_CLOUD  0x1863   // dark teal cloud (visible on black sky)

#define DINO_CLOUD_COUNT 3

// ==================== STATE ====================
struct DinoObs {
  int16_t x;      // left edge screen x; -1 = inactive
  uint8_t kind;   // OBS_* constant
};

static struct {
  int16_t  dinoY;          // top of dino sprite
  int8_t   dinoVY;         // vertical velocity
  bool     ducking;
  uint8_t  legPhase;       // 0 or 1
  uint8_t  legTimer;       // ticks until next leg flip
  DinoObs  obs[DINO_OBS_MAX];
  uint32_t score;
  uint32_t hiScore;
  uint8_t  speed;          // current scroll speed 3..DINO_MAX_SPEED
  uint32_t lastSpeedUp;    // score at last speed bump
  uint8_t  lives;
  uint16_t obsSpawnTimer;  // ticks until next spawn
  uint32_t frameCount;
  int16_t  cloudX[DINO_CLOUD_COUNT];  // cloud x positions (scroll at half speed)
  uint8_t  cloudY[DINO_CLOUD_COUNT];  // cloud y positions (fixed per game)
  uint8_t  cloudW[DINO_CLOUD_COUNT];  // cloud widths (vary for visual interest)
  uint32_t hitFlashUntil;  // millis() deadline for hit-flash pause (non-blocking)
  uint32_t speedBeepUntil; // millis() deadline for speed-up beep (non-blocking)
  bool     gameOver;
  bool     titleScreen;
} dinoSt;

// ==================== SPRITES ====================
// Callers MUST call busForTFT() before any render pass.
// These helpers do NOT call busForTFT() themselves.

static void dinoDrawRunning(int16_t y, uint8_t leg) {
  uint16_t c = COLNEON, cd = COLDIM;
  int x = DINO_DINO_X;
  tft.fillRect(x,      y,      14, 16, c);   // body
  tft.fillRect(x + 4,  y - 8,  12, 10, c);  // head
  tft.fillRect(x + 13, y - 6,   2,  2, COLBLACK); // eye
  tft.fillRect(x - 4,  y + 4,   6,  4, cd); // tail
  if (leg == 0) {
    tft.fillRect(x + 2,  y + 16, 4, 8, cd);
    tft.fillRect(x + 8,  y + 20, 4, 4, cd);
  } else {
    tft.fillRect(x + 2,  y + 20, 4, 4, cd);
    tft.fillRect(x + 8,  y + 16, 4, 8, cd);
  }
}

static void dinoDrawDucking(int16_t y) {
  uint16_t c = COLNEON, cd = COLDIM;
  int x = DINO_DINO_X;
  tft.fillRect(x,      y,      18, 10, c);
  tft.fillRect(x + 6,  y - 6,  10,  8, c);
  tft.fillRect(x + 15, y - 4,   2,  2, COLBLACK);
  tft.fillRect(x + 2,  y + 10,  4,  4, cd);
  tft.fillRect(x + 10, y + 10,  4,  4, cd);
}

static void dinoDrawCactusS(int16_t x) {
  int by = DINO_GROUND_Y;
  uint16_t c = COLNEON;
  tft.fillRect(x + 3, by - 20, 6, 20, c);
  tft.fillRect(x,     by - 14, 5,  4, c);
  tft.fillRect(x + 9, by - 10, 5,  4, c);
  tft.fillRect(x,     by - 18, 3, 12, c);
  tft.fillRect(x + 9, by - 14, 3, 10, c);
}

static void dinoDrawCactusL(int16_t x) {
  int by = DINO_GROUND_Y;
  uint16_t c = COLNEON;
  tft.fillRect(x + 3, by - 36, 6, 36, c);
  tft.fillRect(x,     by - 24, 5,  4, c);
  tft.fillRect(x + 9, by - 20, 5,  4, c);
  tft.fillRect(x,     by - 30, 3, 14, c);
  tft.fillRect(x + 9, by - 26, 3, 12, c);
}

static void dinoDrawBird(int16_t x, int16_t y, bool wingUp) {
  uint16_t c = DINO_COL_BIRD;
  tft.fillRect(x,      y,      18, 6, c);
  tft.fillRect(x + 14, y - 2,   4, 4, c);
  if (wingUp) tft.fillRect(x + 2, y - 6, 10, 4, c);
  else        tft.fillRect(x + 2, y + 6, 10, 4, c);
}

// Cloud: soft 3-layer pixel cloud
static void dinoDrawCloud(int16_t x, int16_t y, uint8_t w) {
  uint16_t c = DINO_COL_CLOUD;
  tft.fillRect(x,         y,      w,     4, c);  // base layer
  tft.fillRect(x + 4,     y - 4,  w - 6, 4, c);  // middle bump
  tft.fillRect(x + 8,     y - 8,  w - 14, 4, c); // top peak
}

// Rock: wide flat boulder on the ground — must jump, ducking doesn't help
static void dinoDrawRock(int16_t x) {
  int by = DINO_GROUND_Y;
  uint16_t c = COLDIM;
  tft.fillRect(x,      by - 12, 28, 12, c);  // main body
  tft.fillRect(x + 4,  by - 16,  8,  4, c);  // top bump
  tft.fillRect(x + 18, by - 14,  6,  2, c);  // secondary bump
}

// Pair/trio combos — 14px spacing between each cactus
static void dinoDrawPairSS(int16_t x)  { dinoDrawCactusS(x); dinoDrawCactusS(x+14); }
static void dinoDrawPairSL(int16_t x)  { dinoDrawCactusS(x); dinoDrawCactusL(x+14); }
static void dinoDrawPairLS(int16_t x)  { dinoDrawCactusL(x); dinoDrawCactusS(x+14); }
static void dinoDrawPairLL(int16_t x)  { dinoDrawCactusL(x); dinoDrawCactusL(x+14); }
static void dinoDrawTrioSSS(int16_t x) { dinoDrawCactusS(x); dinoDrawCactusS(x+14); dinoDrawCactusS(x+28); }
static void dinoDrawTrioSLS(int16_t x) { dinoDrawCactusS(x); dinoDrawCactusL(x+14); dinoDrawCactusS(x+28); }
static void dinoDrawTrioLSL(int16_t x) { dinoDrawCactusL(x); dinoDrawCactusS(x+14); dinoDrawCactusL(x+28); }

// Erase dino area (fill with sky, restore ground strip below)
static void dinoEraseDino(int16_t y, bool wasDucking) {
  int h = wasDucking ? DINO_DINO_H_DUCK : DINO_DINO_H_RUN;
  tft.fillRect(DINO_DINO_X - 5, y - 10, DINO_DINO_W + 10, h + 18, DINO_COL_SKY);
  tft.drawFastHLine(DINO_DINO_X - 5, DINO_GROUND_Y, DINO_DINO_W + 10, DINO_COL_GROUND);
}

// Erase single obstacle area
static void dinoEraseObs(const DinoObs& o) {
  if (o.kind == OBS_CACTUS_S) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 21, 16, 22, DINO_COL_SKY);
  } else if (o.kind == OBS_CACTUS_L) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 37, 16, 38, DINO_COL_SKY);
  } else if (o.kind == OBS_ROCK) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 17, 32, 18, DINO_COL_SKY);
  } else if (o.kind == OBS_PAIR_SS) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 21, 28, 22, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 30, DINO_COL_GROUND);
  } else if (o.kind == OBS_PAIR_SL || o.kind == OBS_PAIR_LS) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 37, 28, 38, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 30, DINO_COL_GROUND);
  } else if (o.kind == OBS_PAIR_LL) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 37, 28, 38, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 30, DINO_COL_GROUND);
  } else if (o.kind == OBS_TRIO_SSS) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 21, 42, 22, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 44, DINO_COL_GROUND);
  } else if (o.kind == OBS_TRIO_SLS || o.kind == OBS_TRIO_LSL) {
    tft.fillRect(o.x - 1, DINO_GROUND_Y - 37, 42, 38, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 44, DINO_COL_GROUND);
  } else {
    int16_t by = (o.kind == OBS_BIRD_HI) ? DINO_GROUND_Y - 60 : DINO_GROUND_Y - 35;
    tft.fillRect(o.x - 1, by - 8, 24, 20, DINO_COL_SKY);
    tft.drawFastHLine(o.x - 1, DINO_GROUND_Y, 26, DINO_COL_GROUND);
  }
}

// ==================== FULL FRAME DRAW ====================
static void dinoDrawAll() {
  busForTFT();

  // Sky + ground
  tft.fillRect(0, 0, DINO_W, DINO_GROUND_Y, DINO_COL_SKY);
  tft.fillRect(0, DINO_GROUND_Y, DINO_W, 2, DINO_COL_GROUND);

  // Clouds (scroll at half ground speed, wrap around)
  uint8_t halfSpeed = (dinoSt.speed >> 1) + 1;
  for (uint8_t i = 0; i < DINO_CLOUD_COUNT; i++) {
    if ((dinoSt.frameCount & 1) == 0) {
      dinoSt.cloudX[i] -= halfSpeed;
      if (dinoSt.cloudX[i] < -(int16_t)dinoSt.cloudW[i] - 8)
        dinoSt.cloudX[i] = DINO_W + 4;
    }
    if (dinoSt.cloudX[i] < DINO_W && dinoSt.cloudX[i] > -(int16_t)dinoSt.cloudW[i])
      dinoDrawCloud(dinoSt.cloudX[i], dinoSt.cloudY[i], dinoSt.cloudW[i]);
  }

  // HUD strip
  tft.fillRect(0, DINO_HUD_Y, DINO_W, DINO_H - DINO_HUD_Y, COLBLACK);
  tft.setTextSize(1);
  tft.setTextColor(COLNEON);
  tft.setCursor(4, DINO_HUD_Y + 4);
  tft.print("SC:"); tft.print(dinoSt.score);
  tft.setTextColor(COLDIM);
  tft.setCursor(4, DINO_HUD_Y + 14);
  tft.print("HI:"); tft.print(dinoSt.hiScore);
  tft.fillRect(4, DINO_HUD_Y + 28, 120, 4, COLDARK);
  int barW = ((int)dinoSt.speed * 120) / DINO_MAX_SPEED;
  tft.fillRect(4, DINO_HUD_Y + 28, barW, 4, COLNEON);
  tft.setTextColor(COLDIM);
  tft.setCursor(128, DINO_HUD_Y + 26);
  tft.print("SPD");

  // Dino
  if (dinoSt.ducking) dinoDrawDucking(dinoSt.dinoY);
  else                dinoDrawRunning(dinoSt.dinoY, dinoSt.legPhase);

  // Obstacles
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x < 0) continue;
    const DinoObs& o = dinoSt.obs[i];
    if      (o.kind == OBS_CACTUS_S) dinoDrawCactusS(o.x);
    else if (o.kind == OBS_CACTUS_L) dinoDrawCactusL(o.x);
    else if (o.kind == OBS_ROCK)      dinoDrawRock(o.x);
    else if (o.kind == OBS_PAIR_SS)   dinoDrawPairSS(o.x);
    else if (o.kind == OBS_PAIR_SL)   dinoDrawPairSL(o.x);
    else if (o.kind == OBS_PAIR_LS)   dinoDrawPairLS(o.x);
    else if (o.kind == OBS_PAIR_LL)   dinoDrawPairLL(o.x);
    else if (o.kind == OBS_TRIO_SSS)  dinoDrawTrioSSS(o.x);
    else if (o.kind == OBS_TRIO_SLS)  dinoDrawTrioSLS(o.x);
    else if (o.kind == OBS_TRIO_LSL)  dinoDrawTrioLSL(o.x);
    else {
      int16_t by = (o.kind == OBS_BIRD_HI) ? DINO_GROUND_Y - 60 : DINO_GROUND_Y - 35;
      dinoDrawBird(o.x, by, (dinoSt.frameCount >> 3) & 1);
    }
  }
}

// ==================== GAME OVER SCREEN ====================
static void dinoDrawGameOver() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  tft.setTextColor(COLRED);
  tft.setTextSize(2);
  tft.setCursor(66, 70);   // "GAME OVER" 9*12=108 → 120-54=66
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(90, 100);  // "OK = RETRY" 10*6=60 → 120-30=90
  tft.print("OK = RETRY");
  tft.setCursor(87, 116);  // "BACK = MENU" 11*6=66 → 120-33=87
  tft.print("BACK = MENU");
  { char scBuf[16]; snprintf(scBuf, sizeof(scBuf), "SC:%d", dinoSt.score);
    tft.setCursor(120 - (int)(strlen(scBuf)*6)/2, 140); tft.print(scBuf); }
  { char hiBuf[16]; snprintf(hiBuf, sizeof(hiBuf), "HI:%d", dinoSt.hiScore);
    tft.setCursor(120 - (int)(strlen(hiBuf)*6)/2, 154); tft.print(hiBuf); }
}

// ==================== INIT ====================
static void dinoInitGame() {
  dinoSt.dinoY        = DINO_GROUND_Y - DINO_DINO_H_RUN;
  dinoSt.dinoVY       = 0;
  dinoSt.ducking      = false;
  dinoSt.legPhase     = 0;
  dinoSt.legTimer     = 8;
  dinoSt.speed        = 3;   // start slow, increases with score
  dinoSt.score        = 0;
  dinoSt.lastSpeedUp  = 0;
  dinoSt.lives        = 1;
  dinoSt.obsSpawnTimer= 60;
  dinoSt.frameCount   = 0;
  dinoSt.hitFlashUntil = 0;
  dinoSt.speedBeepUntil= 0;
  dinoSt.gameOver     = false;
  dinoSt.titleScreen  = false;
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) dinoSt.obs[i].x = -1;
  // Spread clouds evenly across the screen with varied heights/widths
  static const uint8_t cloudYs[DINO_CLOUD_COUNT] = { 30, 55, 18 };
  static const uint8_t cloudWs[DINO_CLOUD_COUNT] = { 32, 24, 28 };
  for (uint8_t i = 0; i < DINO_CLOUD_COUNT; i++) {
    dinoSt.cloudX[i] = (int16_t)(i * (DINO_W / DINO_CLOUD_COUNT) + 20);
    dinoSt.cloudY[i] = cloudYs[i];
    dinoSt.cloudW[i] = cloudWs[i];
  }
}

// ==================== SPAWN ====================
static void dinoSpawnObs() {
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x >= 0) continue;
    dinoSt.obs[i].x = DINO_W + 4;
    // XOR frame + score + upper nibble for better distribution
    uint8_t r = (uint8_t)(dinoSt.frameCount & 0xFF)
              ^ (uint8_t)(dinoSt.score & 0xFF)
              ^ (uint8_t)(dinoSt.frameCount >> 4);
    uint8_t sp = dinoSt.speed;

    // Build pool of available obstacles — grows with speed
    uint8_t pool[14];
    uint8_t n = 0;
    pool[n++] = OBS_CACTUS_S;
    pool[n++] = OBS_CACTUS_L;
    if (sp >= 5) { pool[n++] = OBS_PAIR_SS; pool[n++] = OBS_PAIR_SL;
                   pool[n++] = OBS_PAIR_LS; }
    if (sp >= 6) { pool[n++] = OBS_ROCK; }
    if (sp >= 7) { pool[n++] = OBS_PAIR_LL;
                   pool[n++] = OBS_BIRD_LO; pool[n++] = OBS_BIRD_HI; }
    if (sp >= 9) { pool[n++] = OBS_TRIO_SSS;
                   pool[n++] = OBS_TRIO_SLS; }
    if (sp >= 10){ pool[n++] = OBS_TRIO_LSL; }
    dinoSt.obs[i].kind = pool[r % n];
    return;
  }
}

// ==================== COLLISION ====================
static bool dinoCheckHit(const DinoObs& o) {
  // Dino hitbox (shrunk 3px for fairness)
  int dx1 = DINO_DINO_X + 3;
  int dy1 = dinoSt.dinoY + 3;
  int dx2 = DINO_DINO_X + DINO_DINO_W - 3;
  int dy2 = dinoSt.ducking
    ? dinoSt.dinoY + DINO_DINO_H_DUCK - 3
    : dinoSt.dinoY + DINO_DINO_H_RUN  - 3;

  int ox1, oy1, ox2, oy2;
  if (o.kind == OBS_CACTUS_S) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 20;
    ox2 = o.x + 11; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_CACTUS_L) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 36;
    ox2 = o.x + 11; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_ROCK) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 16;
    ox2 = o.x + 27; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_PAIR_SS) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 20; ox2 = o.x + 26; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_PAIR_SL || o.kind == OBS_PAIR_LS || o.kind == OBS_PAIR_LL) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 36; ox2 = o.x + 26; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_TRIO_SSS) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 20; ox2 = o.x + 40; oy2 = DINO_GROUND_Y;
  } else if (o.kind == OBS_TRIO_SLS || o.kind == OBS_TRIO_LSL) {
    ox1 = o.x + 1; oy1 = DINO_GROUND_Y - 36; ox2 = o.x + 40; oy2 = DINO_GROUND_Y;
  } else {
    int16_t by = (o.kind == OBS_BIRD_HI) ? DINO_GROUND_Y - 60 : DINO_GROUND_Y - 35;
    ox1 = o.x + 2; oy1 = by; ox2 = o.x + 16; oy2 = by + 10;
  }
  return (dx1 < ox2 && dx2 > ox1 && dy1 < oy2 && dy2 > oy1);
}

// ==================== ENTRY DRAW ====================
// Called once by goTo(MODE_DINO). Draws title screen.
static void drawDino() {
  dinoSt.titleScreen = true;
  dinoSt.hiScore = 0;
  prefs.begin("hackcon", true);
  dinoSt.hiScore = prefs.getUInt("dinohi", 0);
  prefs.end();

  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();

  tft.setTextColor(COLNEON);
  tft.setTextSize(2);
  tft.setCursor(72, 70);   // "DINO RUN" 8*12=96 → 120-48=72
  tft.print("DINO RUN");
  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  // tft.setCursor(78, 96);   // "HackConRD 2026" 14*6=84 → 120-42=78
  // tft.print("HackConRD 2026");

  // Idle dino centered on ground line
  dinoDrawRunning(DINO_GROUND_Y - DINO_DINO_H_RUN, 0);

  tft.setTextColor(COLNEON);
  tft.setCursor(90, 122);  // "OK = START"
  tft.print("OK = START");
  tft.setTextColor(COLDIM);
  tft.setCursor(87, 136);  // "BACK = MENU"
  tft.print("BACK = MENU");
  { char hiBuf[12]; snprintf(hiBuf, sizeof(hiBuf), "HI:%d", dinoSt.hiScore);
    tft.setCursor(120 - (int)(strlen(hiBuf)*6)/2, 150);
    tft.print(hiBuf); }

  tft.fillRect(0, DINO_GROUND_Y, DINO_W, 2, DINO_COL_GROUND);
}

// ==================== GAME TICK ====================
static void dinoTick(ButtonsEdge b) {
  // ---- Title screen ----
  if (dinoSt.titleScreen) {
    if (b.ok)   { dinoInitGame(); dinoMusicStart(); dinoDrawAll(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  // ---- Game Over screen ----
  if (dinoSt.gameOver) {
    if (b.ok)   { dinoInitGame(); dinoMusicStart(); dinoDrawAll(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  uint32_t now = millis();

  // ---- Hit-flash pause (non-blocking) ----
  // While hitFlashUntil > now, skip game logic but keep music/LEDs running via caller.
  if (now < dinoSt.hitFlashUntil) { dinoSfxTick(); return; }

  // ---- Active game ----
  dinoSfxTick();
  dinoSt.frameCount++;
  bool onGround = (dinoSt.dinoY >= DINO_GROUND_Y - DINO_DINO_H_RUN);

  // DOWN + BACK held together = exit to games menu (same as HackerMon)
  if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
    dinoMusicStop();
    while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
    goTo(MODE_GAMES_MENU);
    return;
  }

  // Input
  if (b.up && onGround && !dinoSt.ducking) {
    dinoSt.dinoVY = DINO_JUMP_VY;
  }
  dinoSt.ducking = isPressed(PUSH2PIN) && onGround;

  // Physics
  dinoSt.dinoVY += DINO_GRAVITY;
  dinoSt.dinoY  += dinoSt.dinoVY;
  int groundTop = DINO_GROUND_Y - (dinoSt.ducking ? DINO_DINO_H_DUCK : DINO_DINO_H_RUN);
  if (dinoSt.dinoY >= groundTop) {
    dinoSt.dinoY  = groundTop;
    dinoSt.dinoVY = 0;
  }

  // Leg animation (only on ground)
  if (onGround && !dinoSt.ducking) {
    if (--dinoSt.legTimer == 0) { dinoSt.legPhase ^= 1; dinoSt.legTimer = 8; }
  }

  // Scroll obstacles (boost adds +3, capped at DINO_MAX_SPEED)
  uint8_t scrollSpeed = dinoSt.speed;
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x < 0) continue;
    dinoSt.obs[i].x -= scrollSpeed;
    if (dinoSt.obs[i].x < -30) dinoSt.obs[i].x = -1;
  }

  // Spawn
  if (--dinoSt.obsSpawnTimer == 0) {
    dinoSpawnObs();
    // Gap shrinks with both speed and score; floor drops from 40→20 after score 500
    int gap = 100 - dinoSt.speed * 6;
    int minGap = (dinoSt.score > 500) ? 20 : 30;
    if (gap < minGap) gap = minGap;
    dinoSt.obsSpawnTimer = (uint16_t)(gap + (dinoSt.frameCount % 20));
  }

  // Collision
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x < 0) continue;
    if (!dinoCheckHit(dinoSt.obs[i])) continue;
    dinoSfxCrash();
    dinoSt.obs[i].x = -1;
    dinoSt.lives--;
    if (dinoSt.lives == 0) {
      dinoSt.gameOver = true;
      dinoMusicStop();
      if (dinoSt.score > dinoSt.hiScore) {
        dinoSt.hiScore = dinoSt.score;
        prefs.begin("hackcon", false);
        prefs.putUInt("dinohi", dinoSt.hiScore);
        prefs.end();
      }
      dinoDrawGameOver();
      return;
    }
    // Non-fatal hit: pause game logic for 150ms (music/LEDs keep running)
    dinoSt.hitFlashUntil = now + 150;
    break;
  }

  // Score + automatic speed up every 100 points
  dinoSt.score += 1;
  if (dinoSt.score - dinoSt.lastSpeedUp >= 100 && dinoSt.speed < DINO_MAX_SPEED) {
    dinoSt.speed++;
    dinoSt.lastSpeedUp = dinoSt.score;
  }

  dinoDrawAll();
}
