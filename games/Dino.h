// ======================= DINO RUN (Corredor sin fin) =======================
// Side-scroller estilo dinosaurio de Chrome para el badge HackConRD 2026.
//
// Externos requeridos desde badge.ino:
//   tft, busForTFT(), buzzerTone(), buzzerNoTone(), buzzerEnabled,
//   isPressed(), PUSH1PIN..PUSH4PIN,
//   uiFrame(), COLDIM, COLDARK, COLBLACK, COLNEON,
//   goTo(), MODE_MENU, MODE_GAMES_MENU, ButtonsEdge, prefs

// ==================== MÚSICA ====================
struct DinoNote { uint16_t freq; uint16_t dur; };

#define _Q   375
#define _QD  563
#define _8   188
#define _8D  281
#define _H   750

static const DinoNote dinoMelody[] PROGMEM = {
  // Frase A
  {659,_Q},{784,_Q},{880,_Q},{784,_Q},
  {659,_Q},{523,_Q},{440,_H},
  {523,_Q},{587,_Q},{659,_Q},{784,_Q},
  {880,_QD},{784,_8},{659,_H},

  // Frase B
  {784,_Q},{880,_Q},{988,_Q},{880,_Q},
  {784,_Q},{659,_Q},{523,_H},
  {587,_Q},{659,_Q},{784,_Q},{880,_Q},
  {988,_QD},{880,_8},{784,_H},

  // Puente
  {1047,_Q},{880,_8},{784,_8},{659,_Q},{523,_Q},
  {440,_Q},{494,_Q},{523,_Q},{587,_Q},
  {659,_QD},{587,_8},{523,_Q},{440,_Q},
  {494,_Q},{440,_8},{0,_8},{0,_H},

  // Frase A (repetición)
  {659,_Q},{784,_Q},{880,_Q},{784,_Q},
  {659,_Q},{523,_Q},{440,_H},
  {523,_Q},{587,_Q},{659,_Q},{784,_Q},
  {880,_QD},{784,_8},{659,_H},
  {0,_8},
};
#undef _Q
#undef _QD
#undef _8
#undef _8D
#undef _H
static const int dinoMelodyLen = sizeof(dinoMelody) / sizeof(dinoMelody[0]);

static int      dinoMusicIdx     = 0;
static uint32_t dinoMusicNextMs  = 0;
static bool     dinoMusicPlaying = false;

static void dinoMusicStart() {
  dinoMusicIdx    = 0;
  dinoMusicNextMs = millis();
  dinoMusicPlaying = true;
}
static void dinoMusicStop() {
  dinoMusicPlaying = false;
  if (buzzerEnabled) buzzerNoTone();
}
static void dinoMusicTick() {
  if (!dinoMusicPlaying || !buzzerEnabled) return;
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

// ==================== EFECTOS DE SONIDO ====================
static uint32_t dinoSfxUntil = 0;
static uint8_t  dinoSfxPhase = 0;

static const uint16_t DINO_SFX_GO_NOTES[8] PROGMEM = { 880, 784, 698, 659, 587, 523, 494, 392 };
static uint8_t  dinoGoSfxIdx    = 0;
static uint32_t dinoGoSfxNextMs = 0;
static bool     dinoGoSfxActive = false;

static void dinoSfxGameOverStart() {
  dinoGoSfxIdx    = 0;
  dinoGoSfxNextMs = millis();
  dinoGoSfxActive = true;
}
static void dinoGoSfxTick() {
  if (!dinoGoSfxActive || !buzzerEnabled) return;
  uint32_t now = millis();
  if (now < dinoGoSfxNextMs) return;
  if (dinoGoSfxIdx >= 8) { dinoGoSfxActive = false; buzzerNoTone(); return; }
  buzzerTone(pgm_read_word(&DINO_SFX_GO_NOTES[dinoGoSfxIdx++]));
  dinoGoSfxNextMs = now + 65;
}

static void dinoSfxJump() {
  if (!buzzerEnabled) return;
  buzzerTone(880);
  dinoSfxUntil = millis() + 40;
  dinoSfxPhase = 1;
}

static void dinoSfxCrash() {
  if (!buzzerEnabled) return;
  buzzerTone(180);
  dinoSfxUntil = millis() + 80;
  dinoSfxPhase = 10;
}

static void dinoSfxTick() {
  if (!buzzerEnabled || dinoSfxUntil == 0) return;
  uint32_t now = millis();
  if (now < dinoSfxUntil) return;
  dinoSfxUntil = 0;
  if (dinoSfxPhase == 1) {
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

// ==================== CONSTANTES ====================
#define DINO_W            240
#define DINO_H            240
#define DINO_GROUND_Y     180   // parte superior de la línea del suelo (deja 60px de espacio de juego arriba)
#define DINO_HUD_Y        195   // parte superior del HUD (margen de 15px desde el suelo — evita superposición)
#define DINO_DINO_X        28   // borde izquierdo horizontal fijo del dino
#define DINO_DINO_W        18   // ancho del sprite del dino (px)
#define DINO_DINO_H_RUN    28   // altura del dino al correr/saltar
#define DINO_DINO_H_DUCK   14   // altura del dino al agacharse
#define DINO_JUMP_VY      -14   // velocidad vertical inicial al saltar (negativo = arriba)
#define DINO_GRAVITY        1   // gravedad sumada a vy por tick
#define DINO_MAX_SPEED     12   // velocidad máxima de desplazamiento (px/tick)
#define DINO_OBS_MAX        3   // máximo de obstáculos simultáneos

// Tipos de obstáculos
#define OBS_CACTUS_S  0   // cactus pequeño, 20px de alto
#define OBS_CACTUS_L  1   // cactus alto, 36px de alto
#define OBS_BIRD_HI   2   // pájaro a altura de cabeza — hay que agacharse (y = GROUND_Y-60)
#define OBS_BIRD_LO   3   // pájaro a altura de torso — hay que saltar (y = GROUND_Y-35)
#define OBS_ROCK      4   // roca plana ancha en el suelo — hay que saltar, no se puede agachar
#define OBS_PAIR_SS   5   // pequeño + pequeño  (velocidad 5+)
#define OBS_PAIR_SL   6   // pequeño + alto     (velocidad 5+)
#define OBS_PAIR_LS   7   // alto  + pequeño    (velocidad 5+)
#define OBS_PAIR_LL   8   // alto  + alto       (velocidad 7+)
#define OBS_TRIO_SSS  9   // P+P+P              (velocidad 9+)
#define OBS_TRIO_SLS  10  // P+A+P              (velocidad 9+)
#define OBS_TRIO_LSL  11  // A+P+A              (velocidad 10+)

// Colores (constantes RGB565, neutras de tema)
#define DINO_COL_SKY    0x0000   // cielo negro
#define DINO_COL_GROUND 0x2945   // franja de suelo gris oscuro
#define DINO_COL_BIRD   0x077F   // pájaro cian
#define DINO_COL_CLOUD  0x1863   // nube verde azulado oscuro (visible sobre el cielo negro)

#define DINO_CLOUD_COUNT 3

// ==================== ESTADO ====================
struct DinoObs {
  int16_t x;      // borde izquierdo en pantalla; -1 = inactivo
  uint8_t kind;   // constante OBS_*
};

static struct {
  int16_t  dinoY;          // parte superior del sprite del dino
  int8_t   dinoVY;         // velocidad vertical
  bool     ducking;
  uint8_t  legPhase;       // 0 o 1
  uint8_t  legTimer;       // ticks hasta el siguiente cambio de pierna
  DinoObs  obs[DINO_OBS_MAX];
  uint32_t score;
  uint32_t hiScore;
  uint8_t  speed;          // velocidad de desplazamiento actual 3..DINO_MAX_SPEED
  uint32_t lastSpeedUp;    // puntuación en el último incremento de velocidad
  uint8_t  lives;
  uint16_t obsSpawnTimer;  // ticks hasta el siguiente spawn
  uint32_t frameCount;
  int16_t  cloudX[DINO_CLOUD_COUNT];  // posiciones X de las nubes (se desplazan a mitad de velocidad)
  uint8_t  cloudY[DINO_CLOUD_COUNT];  // posiciones Y de las nubes (fijas por partida)
  uint8_t  cloudW[DINO_CLOUD_COUNT];  // anchos de las nubes (varían para interés visual)
  uint32_t hitFlashUntil;  // plazo millis() para la pausa de parpadeo por golpe (no bloqueante)
  uint32_t speedBeepUntil; // plazo millis() para el pitido de subida de velocidad (no bloqueante)
  bool     gameOver;
  bool     titleScreen;
} dinoSt;

// ==================== SPRITES ====================
// El llamador DEBE invocar busForTFT() antes de cualquier pasada de renderizado.

static void dinoDrawRunning(int16_t y, uint8_t leg) {
  uint16_t c = COLNEON, cd = COLDIM;
  int x = DINO_DINO_X;
  tft.fillRect(x,      y,      14, 16, c);   // cuerpo
  tft.fillRect(x + 4,  y - 8,  12, 10, c);  // cabeza
  tft.fillRect(x + 13, y - 6,   2,  2, COLBLACK); // ojo
  tft.fillRect(x - 4,  y + 4,   6,  4, cd); // cola
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

// Nube: nube suave de píxeles en 3 capas
static void dinoDrawCloud(int16_t x, int16_t y, uint8_t w) {
  uint16_t c = DINO_COL_CLOUD;
  tft.fillRect(x,         y,      w,     4, c);  // capa base
  tft.fillRect(x + 4,     y - 4,  w - 6, 4, c);  // protuberancia media
  tft.fillRect(x + 8,     y - 8,  w - 14, 4, c); // pico superior
}

// Roca: roca plana y ancha en el suelo — hay que saltar, agacharse no ayuda
static void dinoDrawRock(int16_t x) {
  int by = DINO_GROUND_Y;
  uint16_t c = COLDIM;
  tft.fillRect(x,      by - 12, 28, 12, c);  // cuerpo principal
  tft.fillRect(x + 4,  by - 16,  8,  4, c);  // protuberancia superior
  tft.fillRect(x + 18, by - 14,  6,  2, c);  // protuberancia secundaria
}

// Combos de pares/tríos — 14px de separación entre cada cactus
static void dinoDrawPairSS(int16_t x)  { dinoDrawCactusS(x); dinoDrawCactusS(x+14); }
static void dinoDrawPairSL(int16_t x)  { dinoDrawCactusS(x); dinoDrawCactusL(x+14); }
static void dinoDrawPairLS(int16_t x)  { dinoDrawCactusL(x); dinoDrawCactusS(x+14); }
static void dinoDrawPairLL(int16_t x)  { dinoDrawCactusL(x); dinoDrawCactusL(x+14); }
static void dinoDrawTrioSSS(int16_t x) { dinoDrawCactusS(x); dinoDrawCactusS(x+14); dinoDrawCactusS(x+28); }
static void dinoDrawTrioSLS(int16_t x) { dinoDrawCactusS(x); dinoDrawCactusL(x+14); dinoDrawCactusS(x+28); }
static void dinoDrawTrioLSL(int16_t x) { dinoDrawCactusL(x); dinoDrawCactusS(x+14); dinoDrawCactusL(x+28); }

static void dinoEraseDino(int16_t y, bool wasDucking) {
  int h = wasDucking ? DINO_DINO_H_DUCK : DINO_DINO_H_RUN;
  tft.fillRect(DINO_DINO_X - 5, y - 10, DINO_DINO_W + 10, h + 18, DINO_COL_SKY);
  tft.drawFastHLine(DINO_DINO_X - 5, DINO_GROUND_Y, DINO_DINO_W + 10, DINO_COL_GROUND);
}

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

// ==================== DIBUJO DE FRAME COMPLETO ====================
static void dinoDrawAll() {
  busForTFT();

  tft.fillRect(0, 0, DINO_W, DINO_GROUND_Y, DINO_COL_SKY);
  tft.fillRect(0, DINO_GROUND_Y, DINO_W, 2, DINO_COL_GROUND);

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

  if (dinoSt.ducking) dinoDrawDucking(dinoSt.dinoY);
  else                dinoDrawRunning(dinoSt.dinoY, dinoSt.legPhase);

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

// ==================== PANTALLA DE GAME OVER ====================
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

// ==================== INICIALIZACIÓN ====================
static void dinoInitGame() {
  dinoSt.dinoY        = DINO_GROUND_Y - DINO_DINO_H_RUN;
  dinoSt.dinoVY       = 0;
  dinoSt.ducking      = false;
  dinoSt.legPhase     = 0;
  dinoSt.legTimer     = 8;
  dinoSt.speed        = 3;
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
  static const uint8_t cloudYs[DINO_CLOUD_COUNT] = { 30, 55, 18 };
  static const uint8_t cloudWs[DINO_CLOUD_COUNT] = { 32, 24, 28 };
  for (uint8_t i = 0; i < DINO_CLOUD_COUNT; i++) {
    dinoSt.cloudX[i] = (int16_t)(i * (DINO_W / DINO_CLOUD_COUNT) + 20);
    dinoSt.cloudY[i] = cloudYs[i];
    dinoSt.cloudW[i] = cloudWs[i];
  }
}

// ==================== GENERACIÓN DE OBSTÁCULOS ====================
static void dinoSpawnObs() {
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x >= 0) continue;
    dinoSt.obs[i].x = DINO_W + 4;
    // XOR frame + puntuación + nibble alto para mejor distribución
    uint8_t r = (uint8_t)(dinoSt.frameCount & 0xFF)
              ^ (uint8_t)(dinoSt.score & 0xFF)
              ^ (uint8_t)(dinoSt.frameCount >> 4);
    uint8_t sp = dinoSt.speed;

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

// ==================== COLISIÓN ====================
static bool dinoCheckHit(const DinoObs& o) {
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

// ==================== DIBUJO DE ENTRADA ====================
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

// ==================== TICK DEL JUEGO ====================
static void dinoTick(ButtonsEdge b) {
  dinoGoSfxTick();

  // ---- Pantalla de título ----
  if (dinoSt.titleScreen) {
    if (b.ok)   { dinoInitGame(); dinoMusicStart(); dinoDrawAll(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  // ---- Pantalla de Game Over ----
  if (dinoSt.gameOver) {
    if (b.ok)   { dinoInitGame(); dinoMusicStart(); dinoDrawAll(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  uint32_t now = millis();

  // ---- Pausa de parpadeo por golpe (no bloqueante) ----
  if (now < dinoSt.hitFlashUntil) { dinoSfxTick(); return; }

  // ---- Juego activo ----
  dinoSfxTick();
  dinoSt.frameCount++;
  bool onGround = (dinoSt.dinoY >= DINO_GROUND_Y - DINO_DINO_H_RUN);

  if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
    dinoMusicStop();
    while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
    goTo(MODE_GAMES_MENU);
    return;
  }

  if (b.up && onGround && !dinoSt.ducking) {
    dinoSt.dinoVY = DINO_JUMP_VY;
  }
  dinoSt.ducking = isPressed(PUSH2PIN) && onGround;

  dinoSt.dinoVY += DINO_GRAVITY;
  dinoSt.dinoY  += dinoSt.dinoVY;
  int groundTop = DINO_GROUND_Y - (dinoSt.ducking ? DINO_DINO_H_DUCK : DINO_DINO_H_RUN);
  if (dinoSt.dinoY >= groundTop) {
    dinoSt.dinoY  = groundTop;
    dinoSt.dinoVY = 0;
  }

  if (onGround && !dinoSt.ducking) {
    if (--dinoSt.legTimer == 0) { dinoSt.legPhase ^= 1; dinoSt.legTimer = 8; }
  }

  uint8_t scrollSpeed = dinoSt.speed;
  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x < 0) continue;
    dinoSt.obs[i].x -= scrollSpeed;
    if (dinoSt.obs[i].x < -30) dinoSt.obs[i].x = -1;
  }

  // Generación
  if (--dinoSt.obsSpawnTimer == 0) {
    dinoSpawnObs();
    int gap = 100 - dinoSt.speed * 6;
    int minGap = (dinoSt.score > 500) ? 20 : 30;
    if (gap < minGap) gap = minGap;
    dinoSt.obsSpawnTimer = (uint16_t)(gap + (dinoSt.frameCount % 20));
  }

  for (uint8_t i = 0; i < DINO_OBS_MAX; i++) {
    if (dinoSt.obs[i].x < 0) continue;
    if (!dinoCheckHit(dinoSt.obs[i])) continue;
    dinoSfxCrash();
    dinoSt.obs[i].x = -1;
    dinoSt.lives--;
    if (dinoSt.lives == 0) {
      dinoSt.gameOver = true;
      dinoMusicStop();
      dinoSfxUntil = 0; dinoSfxPhase = 0;
      dinoSfxGameOverStart();
      if (dinoSt.score > dinoSt.hiScore) {
        dinoSt.hiScore = dinoSt.score;
        prefs.begin("hackcon", false);
        prefs.putUInt("dinohi", dinoSt.hiScore);
        prefs.end();
      }
      dinoDrawGameOver();
      return;
    }
    dinoSt.hitFlashUntil = now + 150;
    break;
  }

  dinoSt.score += 1;
  if (dinoSt.score - dinoSt.lastSpeedUp >= 100 && dinoSt.speed < DINO_MAX_SPEED) {
    dinoSt.speed++;
    dinoSt.lastSpeedUp = dinoSt.score;
  }

  dinoDrawAll();
}
