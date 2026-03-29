// ======================= STAR INVADERS =======================
// Space Invaders clásico para el badge HackConRD 2026.
//
// Externos requeridos desde badge.ino:
//   tft, busForTFT(), buzzerTone(), buzzerNoTone(), buzzerEnabled,
//   isPressed(), PUSH1PIN, PUSH2PIN,
//   uiFrame(), COLDIM, COLDARK, COLBLACK, COLNEON, COLWHITE, COLRED, COLCYAN,
//   goTo(), MODE_GAMES_MENU, ButtonsEdge, prefs

// ==================== MÚSICA: Tema Space Attack ====================
struct SINote { uint16_t freq; uint16_t dur; };

#define _SQ   400
#define _SDQ  600
#define _S8   200
#define _SD8  300
#define _SH   800
#define _S16  100

static const SINote siMelody[] PROGMEM = {
  // --- Fanfarria de intro: ascendente luego descendente ---
  {440,_S16},{523,_S16},{659,_S16},{880,_S8},{0,_S16},
  {784,_S16},{659,_S16},{523,_S16},{440,_S8},{0,_S16},
  {440,_S16},{523,_S16},{659,_S16},{784,_S16},{659,_SQ},{0,_S8},

  // --- Sección A: tema principal impulsado (La menor, corredizas) ---
  {330,_S8},{392,_S8},{440,_S8},{523,_S8},{494,_S8},{440,_S8},{392,_S8},{330,_S8},
  {349,_S8},{440,_S8},{523,_S8},{659,_S8},{587,_S8},{523,_S8},{440,_S8},{349,_S8},
  {392,_S8},{440,_S8},{494,_S8},{523,_S8},{587,_S8},{659,_S8},{587,_S8},{523,_S8},
  {494,_SQ},{440,_SQ},{0,_SQ},

  // --- Sección B: explosión de energía en registro alto ---
  {880,_S8},{784,_S8},{698,_S8},{659,_S8},{587,_S8},{523,_S8},{494,_S8},{440,_S8},
  {392,_S8},{440,_S8},{494,_S8},{523,_S8},{587,_S8},{659,_SDQ},{0,_S8},
  {659,_S8},{587,_S8},{523,_S8},{494,_S8},{440,_S8},{392,_S8},{349,_S8},{330,_S8},
  {440,_SQ},{659,_SQ},{880,_SQ},{0,_SQ},
};
#undef _SQ
#undef _SDQ
#undef _S8
#undef _SD8
#undef _SH
#undef _S16

static const int siMelodyLen = sizeof(siMelody) / sizeof(siMelody[0]);
static int      siMusicIdx    = 0;
static uint32_t siMusicNextMs = 0;
static bool     siMusicPlaying = false;

static void siMusicStart() { siMusicIdx = 0; siMusicNextMs = millis(); siMusicPlaying = true; }
static void siMusicStop()  { siMusicPlaying = false; if (buzzerEnabled) buzzerNoTone(); }
static void siMusicTick() {
  if (!siMusicPlaying || !buzzerEnabled) return;
  uint32_t now = millis();
  if (now < siMusicNextMs) return;
  const SINote n = {
    pgm_read_word(&siMelody[siMusicIdx].freq),
    pgm_read_word(&siMelody[siMusicIdx].dur)
  };
  if (n.freq == 0) buzzerNoTone(); else buzzerTone(n.freq);
  siMusicNextMs = now + n.dur;
  if (++siMusicIdx >= siMelodyLen) siMusicIdx = 0;
}

// ==================== EFECTOS DE SONIDO ====================
static const uint16_t SI_SFX_NOTES[7] PROGMEM = { 494, 440, 392, 349, 294, 262, 220 };
static uint8_t  siSfxIdx    = 0;
static uint32_t siSfxNextMs = 0;
static bool     siSfxActive = false;

static void siSfxGameOverStart() {
  siSfxIdx = 0; siSfxNextMs = millis(); siSfxActive = true;
}
static void siSfxTick() {
  if (!siSfxActive || !buzzerEnabled) return;
  uint32_t now = millis();
  if (now < siSfxNextMs) return;
  if (siSfxIdx >= 7) { siSfxActive = false; buzzerNoTone(); return; }
  buzzerTone(pgm_read_word(&SI_SFX_NOTES[siSfxIdx++]));
  siSfxNextMs = now + 120;
}

// ==================== CONSTANTES DEL JUEGO ====================
#define SI_COLS       10
#define SI_ROWS        5
#define SI_CELL_W     20    // ancho de celda (alien + espacio)
#define SI_CELL_H     15    // alto de celda  (alien + espacio)
#define SI_ALIEN_W    16    // ancho del cuerpo del alien
#define SI_ALIEN_H    11    // alto del cuerpo del alien
#define SI_GRID_X     20    // X superior-izquierdo de la cuadrícula: (240 - 10*20) / 2 = 20
#define SI_GRID_Y     40    // Y superior-izquierdo de la cuadrícula (límite de rebote superior)
#define SI_GRID_Y_MAX 130   // límite de rebote inferior (Y superior-izquierdo, borde inferior = 130+75 = 205)
#define SI_WAVE_PAUSE_MS 1800
#define SI_PLAYER_Y  218    // posición Y del jugador
#define SI_PLAYER_W   20    // ancho del jugador
#define SI_PLAYER_H    9    // alto del jugador
#define SI_PLAYER_SPD      2    // píxeles por movimiento
#define SI_PLAYER_MOVE_MS 18    // ms entre cada paso de movimiento
#define SI_PBUL_MS        18    // ms entre pasos de bala del jugador
#define SI_ABULLET_MS     25    // ms entre pasos de bala alien
#define SI_SHOOT_CD_MS   180    // ms mínimos entre disparos del jugador
#define SI_BULLET_W    3
#define SI_BULLET_H    7
#define SI_MAX_PBUL    3    // máximo de balas simultáneas del jugador
#define SI_MAX_ABUL    4    // máximo de balas simultáneas alien
#define SI_COLYELLOW 0xFFE0

// ==================== ESTADO ====================
static bool     siAlive[SI_ROWS][SI_COLS];
static int16_t  siGridX;
static int16_t  siGridY;
static int8_t   siDirX;
static int8_t   siDirY;   // +1 = descendiendo, -1 = ascendiendo
static int16_t  siPlayerX;
static int16_t  siPBulX[SI_MAX_PBUL], siPBulY[SI_MAX_PBUL]; // balas del jugador
static int16_t  siABulX[SI_MAX_ABUL], siABulY[SI_MAX_ABUL]; // balas alien
static uint32_t siScore;
static uint32_t siHiScore;
static uint8_t  siLives;
static bool     siGameOver;
static bool     siTitleScreen;
static uint8_t  siWave;
static uint32_t siWaveClearUntil;
static uint32_t siMoveNextMs;
static uint32_t siShootNextMs;
static uint8_t  siAnimFrame;
static uint8_t  siAliensAlive;
static uint32_t siPlayerFlashUntil;
static uint32_t siPlayerFlashToggle;
static bool     siPlayerFlashOn;
static uint32_t siPlayerMoveNextMs;
static uint32_t siPBulNextMs;
static uint32_t siABulNextMs;
static uint32_t siShootCdMs;

// ==================== FUNCIONES AUXILIARES ====================
static int16_t siRightEdge() {
  for (int8_t c = SI_COLS-1; c >= 0; c--)
    for (int8_t r = 0; r < SI_ROWS; r++)
      if (siAlive[r][c]) return siGridX + c*SI_CELL_W + SI_ALIEN_W;
  return siGridX;
}
static int16_t siLeftEdge() {
  for (int8_t c = 0; c < SI_COLS; c++)
    for (int8_t r = 0; r < SI_ROWS; r++)
      if (siAlive[r][c]) return siGridX + c*SI_CELL_W;
  return siGridX;
}
static int16_t siBottomEdge() {
  for (int8_t r = SI_ROWS-1; r >= 0; r--)
    for (int8_t c = 0; c < SI_COLS; c++)
      if (siAlive[r][c]) return siGridY + r*SI_CELL_H + SI_ALIEN_H;
  return siGridY;
}
static uint32_t siMoveInterval() {
  if (siAliensAlive <= 1) return 30;   // último alien: velocidad turbo
  uint32_t base = 550, fast = 80;
  return fast + (base - fast) * siAliensAlive / (SI_ROWS * SI_COLS);
}

// ==================== DIBUJO ====================
static uint16_t siAlienColor(uint8_t row) {
  switch (row) {
    case 0:  return COLCYAN;
    case 1:  return COLNEON;
    case 2:  return SI_COLYELLOW;
    default: return COLRED;
  }
}

// Sprite alien 16×11 dibujado con rectángulos
static void siDrawAlien(int16_t px, int16_t py, uint8_t row, uint8_t anim) {
  uint16_t c = siAlienColor(row);
  tft.fillRect(px+2,  py+3, 12, 6, c);  // cuerpo principal
  tft.fillRect(px+4,  py+1,  8, 2, c);  // cabeza
  tft.fillRect(px,    py+4,  2, 4, c);  // brazo izquierdo
  tft.fillRect(px+14, py+4,  2, 4, c);  // brazo derecho
  if (row <= 1) {                        // antenas
    tft.fillRect(px+3, py,   1, 2, c);
    tft.fillRect(px+12,py,   1, 2, c);
  }
  if (anim == 0) {                       // patas frame A
    tft.fillRect(px+2,  py+9, 3, 2, c);
    tft.fillRect(px+11, py+9, 3, 2, c);
  } else {                               // patas frame B
    tft.fillRect(px+5,  py+9, 3, 2, c);
    tft.fillRect(px+8,  py+9, 3, 2, c);
  }
}

static void siEraseAlien(int16_t px, int16_t py) {
  tft.fillRect(px, py, SI_ALIEN_W, SI_ALIEN_H, COLBLACK);
}

static void siDrawPlayer(int16_t x, uint16_t col) {
  tft.fillRect(x,    SI_PLAYER_Y+5, SI_PLAYER_W, 4, col);
  tft.fillRect(x+3,  SI_PLAYER_Y+2, 14,          3, col);
  tft.fillRect(x+9,  SI_PLAYER_Y,    2,           2, col);
}

static void siErasePlayer(int16_t x) {
  tft.fillRect(x, SI_PLAYER_Y, SI_PLAYER_W, SI_PLAYER_H, COLBLACK);
}

static void siDrawHUD() {
  tft.fillRect(5, 5, 230, 14, COLBLACK);
  tft.setTextSize(1);
  tft.setTextColor(COLNEON, COLBLACK);
  tft.setCursor(8, 8);
  tft.print("SC:");
  if (siScore > 99999) tft.print("99999+"); else tft.print(siScore);
  tft.setCursor(90, 8);
  tft.print("W:");
  tft.print(siWave);
  tft.setCursor(120, 8);
  tft.print("HI:");
  tft.print(siHiScore);
  for (uint8_t i = 0; i < siLives; i++) {
    tft.fillRect(210 + i*8, 9, 5, 5, COLNEON);
  }
}

static void siRedrawAll() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  siDrawHUD();
  tft.drawFastHLine(4, 22, 232, COLDARK);

  for (uint8_t r = 0; r < SI_ROWS; r++)
    for (uint8_t c = 0; c < SI_COLS; c++)
      if (siAlive[r][c])
        siDrawAlien(siGridX + c*SI_CELL_W, siGridY + r*SI_CELL_H, r, siAnimFrame);
  siDrawPlayer(siPlayerX, COLNEON);
}

// ==================== INICIALIZACIÓN ====================
static void siNextWave() {
  siWave++;
  if (siLives < 3) siLives++;
  for (uint8_t i = 0; i < SI_MAX_PBUL; i++) { siPBulX[i] = -1; siPBulY[i] = -1; }
  for (uint8_t i = 0; i < SI_MAX_ABUL; i++) { siABulX[i] = -1; siABulY[i] = -1; }
  memset(siAlive, 1, sizeof(siAlive));
  siAliensAlive = SI_ROWS * SI_COLS;
  siGridX = SI_GRID_X;
  siGridY = SI_GRID_Y;
  siDirX  = 1;
  siDirY  = 1;
  siPlayerX = (240 - SI_PLAYER_W) / 2;
  siPlayerFlashUntil  = 0;
  siPlayerFlashToggle = 0;
  siPlayerFlashOn     = false;
  siAnimFrame = 0;
  siMoveNextMs  = millis() + SI_WAVE_PAUSE_MS + 500;
  siShootNextMs = millis() + SI_WAVE_PAUSE_MS + 2000;
  siShootCdMs   = millis() + SI_WAVE_PAUSE_MS + 500;
  busForTFT();
  siRedrawAll();
  tft.fillRect(25, 90, 190, 52, COLBLACK);
  tft.drawRect(25, 90, 190, 52, COLNEON);
  tft.setTextColor(COLNEON, COLBLACK);
  tft.setTextSize(2);
  tft.setCursor(84, 100);
  tft.print("WAVE ");
  tft.print(siWave - 1);
  tft.setCursor(84, 122);
  tft.print("CLEAR!");
  siWaveClearUntil = millis() + SI_WAVE_PAUSE_MS;
}

static void siInitGame() {
  memset(siAlive, 1, sizeof(siAlive));
  siAliensAlive = SI_ROWS * SI_COLS;
  siGridX = SI_GRID_X;
  siGridY = SI_GRID_Y;
  siDirX  = 1;
  siDirY  = 1;
  siPlayerX = (240 - SI_PLAYER_W) / 2;
  for (uint8_t i = 0; i < SI_MAX_PBUL; i++) { siPBulX[i] = -1; siPBulY[i] = -1; }
  for (uint8_t i = 0; i < SI_MAX_ABUL; i++) { siABulX[i] = -1; siABulY[i] = -1; }
  siGameOver       = false;
  siWave           = 1;
  siWaveClearUntil = 0;
  siAnimFrame      = 0;
  siPlayerFlashUntil  = 0;
  siPlayerFlashToggle = 0;
  siPlayerFlashOn     = false;
  siPlayerMoveNextMs  = 0;
  siPBulNextMs        = 0;
  siABulNextMs        = 0;
  siShootCdMs         = 0;
  siMoveNextMs  = millis() + 500;
  siShootNextMs = millis() + 2000;
  siRedrawAll();
  siMusicStart();
}

static void drawSpaceInvaders() {
  siTitleScreen = true;
  siGameOver    = false;
  siLives       = 3;
  siScore       = 0;
  prefs.begin("hackcon", true);
  siHiScore = prefs.getUInt("si_hi", 0);
  prefs.end();

  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  tft.setTextColor(COLNEON, COLBLACK);
  tft.setTextSize(2);
  tft.setCursor(96, 50);
  tft.print("STAR");
  tft.setCursor(72, 72);
  tft.print("INVADERS");
  tft.setTextSize(1);
  tft.setTextColor(COLCYAN, COLBLACK);
  tft.setCursor(75, 130);
  tft.print("HI-SCORE: ");
  tft.print(siHiScore);
  tft.setTextColor(COLDIM, COLBLACK);
  tft.setCursor(81, 160);
  tft.print("[OK] to start");
  tft.setCursor(78, 175);
  tft.print("[BACK] to menu");

  int16_t px = 112, py = 105;
  tft.fillRect(px+2,  py+3, 12, 6, COLNEON);
  tft.fillRect(px+4,  py+1,  8, 2, COLNEON);
  tft.fillRect(px,    py+4,  2, 4, COLNEON);
  tft.fillRect(px+14, py+4,  2, 4, COLNEON);
  tft.fillRect(px+3,  py,    1, 2, COLNEON);
  tft.fillRect(px+12, py,    1, 2, COLNEON);
  tft.fillRect(px+2,  py+9,  3, 2, COLNEON);
  tft.fillRect(px+11, py+9,  3, 2, COLNEON);
}

static void siDrawResult() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  tft.setTextSize(2);
  tft.setTextColor(COLRED, COLBLACK);
  tft.setCursor(66, 68);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(COLNEON, COLBLACK);
  tft.setCursor(96, 100);
  tft.print("WAVE: ");
  tft.print(siWave);
  tft.setCursor(84, 116);
  tft.print("SCORE: ");
  tft.print(siScore);
  tft.setCursor(75, 132);
  tft.print("HI-SCORE: ");
  tft.print(siHiScore);
  tft.setTextColor(COLDIM, COLBLACK);
  tft.setCursor(81, 165);
  tft.print("[OK] to retry");
  tft.setCursor(78, 180);
  tft.print("[BACK] to menu");
}

static void siSaveHiScore() {
  if (siScore > siHiScore) {
    siHiScore = siScore;
    prefs.begin("hackcon", false);
    prefs.putUInt("si_hi", siHiScore);
    prefs.end();
  }
}

// ==================== TICK ====================
static void spaceInvadersTick(ButtonsEdge b) {
  siSfxTick();

  // ---- Pantalla de título ----
  if (siTitleScreen) {
    if (b.ok)   { siTitleScreen = false; siInitGame(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  // ---- Game Over ----
  if (siGameOver) {
    if (b.ok)   { siLives = 3; siScore = 0; siInitGame(); }
    if (b.back) { goTo(MODE_GAMES_MENU); }
    return;
  }

  // ---- Pausa de oleada completada ----
  if (siWaveClearUntil > 0) {
    if (millis() >= siWaveClearUntil) {
      siWaveClearUntil = 0;
      siRedrawAll();
    }
    return;
  }

  busForTFT();
  uint32_t now = millis();

  // ---- Parpadeo por impacto del jugador (no bloqueante) ----
  if (siPlayerFlashUntil > 0) {
    if (now >= siPlayerFlashUntil) {
      siPlayerFlashUntil = 0;
      siErasePlayer(siPlayerX);
      siDrawPlayer(siPlayerX, COLNEON);
      if (siLives == 0) {
        siGameOver = true;
        siMusicStop();
        siSfxGameOverStart();
        siSaveHiScore();
        siDrawResult();
      }
    } else {
      if (now >= siPlayerFlashToggle) {
        siPlayerFlashToggle = now + 100;
        siPlayerFlashOn = !siPlayerFlashOn;
        siErasePlayer(siPlayerX);
        if (siPlayerFlashOn) siDrawPlayer(siPlayerX, COLRED);
      }
    }
    return;
  }

  // ---- ABAJO + BACK mantenidos juntos = salir al menú de juegos ----
  if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
    siMusicStop();
    siSaveHiScore();
    while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
    goTo(MODE_GAMES_MENU);
    return;
  }

  // ---- Movimiento del jugador (limitado) ----
  if (now >= siPlayerMoveNextMs) {
    int16_t oldPX = siPlayerX;
    if (isPressed(PUSH1PIN) && siPlayerX > 5)
      siPlayerX -= SI_PLAYER_SPD;
    if (isPressed(PUSH2PIN) && siPlayerX < 240 - SI_PLAYER_W - 5)
      siPlayerX += SI_PLAYER_SPD;
    if (siPlayerX != oldPX) {
      siErasePlayer(oldPX);
      siDrawPlayer(siPlayerX, COLNEON);
      siPlayerMoveNextMs = now + SI_PLAYER_MOVE_MS;
    }
  }

  // ---- Disparo del jugador ----
  if (b.ok && now >= siShootCdMs) {
    for (uint8_t i = 0; i < SI_MAX_PBUL; i++) {
      if (siPBulX[i] == -1) {
        siPBulX[i] = siPlayerX + SI_PLAYER_W/2 - SI_BULLET_W/2;
        siPBulY[i] = SI_PLAYER_Y - SI_BULLET_H;
        siShootCdMs = now + SI_SHOOT_CD_MS;
        break;
      }
    }
  }

  // ---- Mover balas del jugador ----
  if (now >= siPBulNextMs) {
    siPBulNextMs = now + SI_PBUL_MS;
    for (uint8_t i = 0; i < SI_MAX_PBUL; i++) {
      if (siPBulX[i] == -1) continue;
      tft.fillRect(siPBulX[i], siPBulY[i], SI_BULLET_W, SI_BULLET_H, COLBLACK);
      siPBulY[i] -= 3;
      if (siPBulY[i] < 23) { siPBulX[i] = -1; continue; }
      bool hit = false;
      for (uint8_t r = 0; r < SI_ROWS && !hit; r++) {
        for (uint8_t c = 0; c < SI_COLS && !hit; c++) {
          if (!siAlive[r][c]) continue;
          int16_t ax = siGridX + c*SI_CELL_W;
          int16_t ay = siGridY + r*SI_CELL_H;
          if (siPBulX[i] + SI_BULLET_W > ax && siPBulX[i] < ax + SI_ALIEN_W &&
              siPBulY[i] + SI_BULLET_H > ay && siPBulY[i] < ay + SI_ALIEN_H) {
            siAlive[r][c] = false;
            siAliensAlive--;
            if (ax >= 0 && ax < 240) siEraseAlien(ax, ay);
            siScore += (uint32_t)(SI_ROWS - r) * 10;
            siPBulX[i] = -1;
            hit = true;
            siDrawHUD();
            if (siAliensAlive == 0) {
              siSaveHiScore();
              siNextWave();
              return;
            }
          }
        }
      }
      if (!hit) tft.fillRect(siPBulX[i], siPBulY[i], SI_BULLET_W, SI_BULLET_H, COLWHITE);
    }
  }

  // ---- Mover cuadrícula de aliens ----
  if (now >= siMoveNextMs) {
    siMoveNextMs = now + siMoveInterval();
    siAnimFrame ^= 1;

    bool descend = false;
    if (siDirX > 0 && siRightEdge() >= 234) descend = true;
    if (siDirX < 0 && siLeftEdge()  <=   6) descend = true;

    int16_t eraseX = max((int16_t)0, siGridX);
    int16_t eraseW = min((int16_t)(siGridX + SI_COLS*SI_CELL_W), (int16_t)240) - eraseX;
    if (eraseW > 0)
      tft.fillRect(eraseX, siGridY, eraseW, SI_ROWS*SI_CELL_H, COLBLACK);

    if (descend) {
      siDirX  = -siDirX;
      siGridY += siDirY * 6;
      if (siDirY > 0 && siGridY >= SI_GRID_Y_MAX) {
        siGridY = SI_GRID_Y_MAX;
        siDirY  = -1;
      }
      if (siDirY < 0 && siGridY <= SI_GRID_Y) {
        siGridY = SI_GRID_Y;
        siDirY  = 1;
      }
      if (siBottomEdge() >= SI_PLAYER_Y - 2) {
        siLives = 0;
        siGameOver = true;
        siMusicStop();
        siSfxGameOverStart();
        siSaveHiScore();
        siDrawResult();
        return;
      }
    } else {
      siGridX += siDirX * 3;
    }

    for (uint8_t r = 0; r < SI_ROWS; r++)
      for (uint8_t c = 0; c < SI_COLS; c++)
        if (siAlive[r][c])
          siDrawAlien(siGridX + c*SI_CELL_W, siGridY + r*SI_CELL_H, r, siAnimFrame);

    for (uint8_t i = 0; i < SI_MAX_PBUL; i++)
      if (siPBulX[i] != -1 && siPBulY[i] >= siGridY && siPBulY[i] < siGridY + SI_ROWS*SI_CELL_H)
        tft.fillRect(siPBulX[i], siPBulY[i], SI_BULLET_W, SI_BULLET_H, COLWHITE);

    tft.drawRect(0, 0, 240, 240, COLNEON);
    tft.drawRect(2, 2, 236, 236, COLDARK);
  }

  // ---- Disparo alien ----
  if (now >= siShootNextMs && siAliensAlive > 0) {
    siShootNextMs = now + 300 + (uint32_t)random(500);
    for (uint8_t i = 0; i < SI_MAX_ABUL; i++) {
      if (siABulX[i] != -1) continue;
      uint8_t col = random(SI_COLS);
      for (uint8_t attempt = 0; attempt < SI_COLS; attempt++) {
        uint8_t c = (col + attempt) % SI_COLS;
        for (int8_t r = SI_ROWS-1; r >= 0; r--) {
          if (siAlive[r][c]) {
            int16_t bx = siGridX + c*SI_CELL_W + SI_ALIEN_W/2 - SI_BULLET_W/2;
            if (bx < 4 || bx > 233) break; // alien fuera de pantalla, ignorar
            siABulX[i] = bx;
            siABulY[i] = siGridY + r*SI_CELL_H + SI_ALIEN_H;
            goto si_found_shooter;
          }
        }
      }
      si_found_shooter:
      break;
    }
  }

  // ---- Mover balas alien ----
  const bool moveBullets = (now >= siABulNextMs);
  if (moveBullets) siABulNextMs = now + SI_ABULLET_MS;
  for (uint8_t i = 0; i < SI_MAX_ABUL; i++) {
    if (siABulX[i] == -1) continue;
    if (siABulX[i] < 4 || siABulX[i] > 233) { siABulX[i] = -1; continue; }
    if (!moveBullets) {
      tft.fillRect(siABulX[i], siABulY[i], SI_BULLET_W, SI_BULLET_H, COLRED);
      continue;
    }
    tft.fillRect(siABulX[i], siABulY[i], SI_BULLET_W, SI_BULLET_H, COLBLACK);
    siABulY[i] += 2;
    if (siABulY[i] > 232) { siABulX[i] = -1; continue; }
    // Colisión con el jugador
    if (siABulY[i] + SI_BULLET_H >= SI_PLAYER_Y &&
        siABulY[i] <= SI_PLAYER_Y + SI_PLAYER_H &&
        siABulX[i] + SI_BULLET_W >= siPlayerX   &&
        siABulX[i] <= siPlayerX + SI_PLAYER_W) {
      siABulX[i] = -1;
      siLives--;
      siDrawHUD();
      siPlayerFlashUntil  = now + 600;
      siPlayerFlashToggle = now;
      siPlayerFlashOn     = false;
      continue;
    }
    tft.fillRect(siABulX[i], siABulY[i], SI_BULLET_W, SI_BULLET_H, COLRED);
  }

}
