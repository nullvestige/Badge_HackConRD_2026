// ===================== HACKER MON (RPG estilo Pokemon) =====================
// Temática hacker: explora una ciudad cyberpunk, enfrenta enemigos de software
// de seguridad en un sistema de batalla clásico por turnos.
//
// Externos requeridos desde badge.ino:
//   tft, busForTFT(), buzzerTone(), buzzerNoTone(), buzzerEnabled,
//   isPressed(), PUSH1PIN..PUSH4PIN, ButtonsEdge,
//   COLNEON, COLDIM, COLDARK, COLBLACK, COLWHITE,
//   goTo(), MODE_MENU

// ==================== MAPA ====================
// Tipos de tile (compartidos en todos los mapas):
//   0 = suelo abierto (transitable, zona de encuentros aleatorios)
//   1 = pared/obstáculo (sólido, impasable)
//   3 = punto de curación (transitable, restaura HP+PP)
//   4 = portal al siguiente mapa

#define PKM_MAP_W   20
#define PKM_MAP_H   17
#define PKM_TILE    12   // tamaño de tile del mundo (colisión)
#define PKM_TDRAW   12   // tamaño de tile dibujado
#define PKM_VP_W   240
#define PKM_VP_H   208   // 208px superiores = vista del mundo, 32px inferiores = HUD
#define PKM_CX     120
#define PKM_CY     104

#define PKM_NUM_MAPS  4

// MAPA 0 — BOSQUE
// Portal en (18,10) → sale por el borde este
static const uint8_t pkmMapForest[PKM_MAP_H][PKM_MAP_W] PROGMEM = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1},
  {1,0,1,1,0,1,0,1,0,1,1,0,1,0,1,0,0,0,1,1},
  {1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,1,0,0,1},
  {1,1,0,1,1,1,1,0,1,1,0,1,1,1,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,1,0,0,0,1,1,0,1,1,1,0,1,1},
  {1,0,0,0,1,0,0,1,0,1,0,0,1,0,0,0,1,0,0,1},
  {1,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,0,1,1},
  {1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,1,1,0,1,0,0,0,0,0,0,0,0,1,0,1,0,4,1},
  {1,0,0,0,0,0,1,0,1,0,3,0,1,0,0,0,0,0,0,1},
  {1,1,0,1,1,0,1,0,1,0,0,0,1,0,1,1,0,1,1,1},
  {1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1},
  {1,0,1,0,0,0,1,1,0,1,0,1,1,0,0,0,0,1,0,1},
  {1,0,1,1,0,0,0,1,0,0,0,0,1,0,0,1,1,1,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// MAPA 1 — DESIERTO
static const uint8_t pkmMapDesert[PKM_MAP_H][PKM_MAP_W] PROGMEM = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1},
  {1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,1},
  {1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
  {1,0,0,1,1,1,0,0,1,1,1,0,0,1,1,1,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1},
  {1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,0,0,0,0,1,0,0,3,0,0,0,1,0,0,0,0,0,1},
  {1,0,1,0,0,1,0,0,0,0,0,0,1,0,0,1,0,0,0,1},
  {4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,0,0,1,0,0,0,3,0,0,0,1,0,0,1,0,1},
  {1,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1},
  {1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,0,1},
  {1,0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,0,0,1},
  {1,1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// MAPA 2 — NIEVE
static const uint8_t pkmMapSnow[PKM_MAP_H][PKM_MAP_W] PROGMEM = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,0,0,1,0,1},
  {1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,1},
  {1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1},
  {1,0,0,0,1,0,0,0,3,0,0,0,1,0,0,0,1,0,0,1},
  {1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1,1},
  {1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1},
  {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1},
  {1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,4,1},
  {1,0,0,1,0,0,0,0,1,0,3,0,1,0,0,0,0,0,0,1},
  {1,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,1},
  {1,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,1},
  {1,0,1,0,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// MAPA 3 — ISLA
static const uint8_t pkmMapIsland[PKM_MAP_H][PKM_MAP_W] PROGMEM = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1},
  {1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1},
  {1,0,0,0,1,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1},
  {1,0,0,1,0,0,0,0,0,1,3,0,0,0,1,0,0,0,0,1},
  {1,0,1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,1},
  {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,0,1,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,1},
  {1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,1},
  {1,0,0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,0,1},
  {4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,0,0,0,1,0,0,3,0,0,1,0,0,0,1,0,1},
  {1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,1},
  {1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
  {1,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,1,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// Destinos de portal: {mapaDestino, destX, destY}
struct PkmPortal { uint8_t mapIdx; int8_t px; int8_t py; };
static const PkmPortal pkmPortals[PKM_NUM_MAPS] PROGMEM = {
  {1, 1, 10},  // portal Bosque  → entrada Desierto (1,10)
  {2, 1, 10},  // portal Desierto → entrada Nieve   (1,10)
  {3, 1, 10},  // portal Nieve   → entrada Isla     (1,10)
  {0, 1, 10},  // portal Isla    → entrada Bosque   (1,10)
};

// Fondos de mapa (color de relleno cuando no hay tile dibujado)
static const uint16_t pkmMapBgColor[PKM_NUM_MAPS] PROGMEM = {
  0x0240,  // Bosque  — verde oscuro
  0x8C00,  // Desierto — arena/marrón oscuro
  0x4A5F,  // Nieve   — azul-gris frío
  0x0013,  // Isla    — azul oceánico profundo
};

// ==================== MÚSICA ====================
struct PkmNote { uint16_t freq; uint16_t dur; };

#define _P8(f)  {(f), 100},{0,25}
#define _P4(f)  {(f), 210},{0,50}
#define _P2(f)  {(f), 430},{0,80}
#define _P16(f) {(f),  45},{0,12}

static const PkmNote pkmOverworldMelody[] PROGMEM = {
  _P8(523),_P8(587),_P8(659),_P4(784),
  _P8(659),_P8(587),_P8(523),_P4(440),
  _P8(494),_P8(523),_P8(587),_P4(659),
  _P8(587),_P8(523),_P8(494),_P2(440),
  _P8(659),_P8(698),_P8(784),_P4(880),
  _P8(784),_P8(698),_P8(659),_P4(587),
  _P8(523),_P8(587),_P8(659),_P8(587),_P8(523),_P8(494),_P2(523),
};
static const int pkmOverworldLen = sizeof(pkmOverworldMelody)/sizeof(pkmOverworldMelody[0]);

static const PkmNote pkmBattleMelody[] PROGMEM = {
  _P16(880),_P16(988),_P8(1047),_P16(880),_P16(988),_P8(784),
  _P16(880),_P16(988),_P8(1047),_P8(1175),_P4(1047),
  _P16(659),_P16(698),_P8(784),_P16(659),_P16(698),_P8(587),
  _P16(523),_P16(587),_P8(659),_P8(784),_P4(659),
  _P8(880),_P8(784),_P8(698),_P8(659),_P8(587),_P8(523),_P2(440),
};
static const int pkmBattleLen = sizeof(pkmBattleMelody)/sizeof(pkmBattleMelody[0]);

static const PkmNote pkmVictoryMelody[] PROGMEM = {
  _P8(784),_P8(784),_P8(784),_P8(784),
  _P8(659),_P8(698),_P8(784),{0,80},
  _P8(698),_P8(784),
};
static const int pkmVictoryLen = sizeof(pkmVictoryMelody)/sizeof(pkmVictoryMelody[0]);

#undef _P8
#undef _P4
#undef _P2
#undef _P16

static int      pkmMusicIdx     = 0;
static uint32_t pkmMusicNextMs  = 0;
static bool     pkmMusicPlaying = false;
static uint8_t  pkmMusicTrack   = 0;

static void pkmMusicStart(uint8_t track) {
  pkmMusicTrack   = track;
  pkmMusicIdx     = 0;
  pkmMusicNextMs  = 0;
  pkmMusicPlaying = true;
}
static void pkmMusicStop() { pkmMusicPlaying = false; if (buzzerEnabled) buzzerNoTone(); }

// ==================== EFECTOS DE SONIDO: GAME OVER ====================
static const uint16_t PKM_SFX_GO_NOTES[6] PROGMEM = { 523, 415, 330, 262, 208, 165 };
static uint8_t  pkmGoSfxIdx    = 0;
static uint32_t pkmGoSfxNextMs = 0;
static bool     pkmGoSfxActive = false;

static void pkmSfxGameOverStart() {
  pkmGoSfxIdx    = 0;
  pkmGoSfxNextMs = millis();
  pkmGoSfxActive = true;
}
static void pkmGoSfxTick() {
  if (!pkmGoSfxActive || !buzzerEnabled) return;
  uint32_t now = millis();
  if (now < pkmGoSfxNextMs) return;
  if (pkmGoSfxIdx >= 6) { pkmGoSfxActive = false; buzzerNoTone(); return; }
  buzzerTone(pgm_read_word(&PKM_SFX_GO_NOTES[pkmGoSfxIdx++]));
  pkmGoSfxNextMs = now + 200;
}

static void pkmMusicTick() {
  if (!pkmMusicPlaying || !buzzerEnabled) { if (pkmMusicPlaying) buzzerNoTone(); return; }
  uint32_t now = millis();
  if (now < pkmMusicNextMs) return;

  const PkmNote* mel  = nullptr;
  int            len  = 0;
  bool           loop = true;

  if (pkmMusicTrack == 0) { mel = pkmOverworldMelody; len = pkmOverworldLen; }
  else if (pkmMusicTrack == 1) { mel = pkmBattleMelody; len = pkmBattleLen; }
  else { mel = pkmVictoryMelody; len = pkmVictoryLen; loop = false; }

  if (pkmMusicIdx >= len) {
    if (loop) pkmMusicIdx = 0;
    else { pkmMusicStop(); return; }
  }

  uint16_t freq = pgm_read_word(&mel[pkmMusicIdx].freq);
  uint16_t dur  = pgm_read_word(&mel[pkmMusicIdx].dur);
  buzzerTone(freq);
  pkmMusicNextMs = now + dur;
  pkmMusicIdx++;
}

// ==================== ENEMIGOS ====================
#define PKM_MAX_ENEMIES 10

struct PkmEnemy {
  const char* name;
  uint8_t     maxHp;
  uint8_t     attack;   // rango de daño base [attack, attack+4]
  uint8_t     defense;  // reducción de daño
  uint16_t    color;    // color del sprite RGB565
  uint16_t    xpGiven;
  uint8_t     minLevel; // nivel mínimo del jugador para encontrar este enemigo
};

static const PkmEnemy pkmEnemies[PKM_MAX_ENEMIES] = {
  {"AVAST",        30,  7,  7, 0x07FF, 14, 1},
  {"Win Defender", 36,  8,  8, 0x001F, 18, 1},
  {"Malwarebytes", 32,  8,  7, 0x03E0, 16, 1},
  {"Kaspersky",    52, 10, 11, 0xF800, 28, 2},
  {"Fortinet",     65, 12, 13, 0xF800, 34, 3},
  {"CrowdStrike",  68, 13, 13, 0xFC00, 38, 4},
  {"SentinelOne",  66, 13, 12, 0xF81F, 36, 4},
  {"Palo Alto",    72, 14, 15, 0xFD20, 42, 5},
  {"Check Point",  75, 15, 15, 0xFFC0, 48, 6},
  {"Cisco ASA",   100, 20, 19, 0x07E0, 85, 8},
};

// Movimientos del Hacker (ataques del jugador)
#define PKM_MOVES 4
struct PkmMove {
  const char* name;
  uint8_t     power;   // daño base
  uint8_t     pp;      // usos (máximo)
  uint16_t    color;
};

static const PkmMove pkmMoves[PKM_MOVES] = {
  {"SQL Inject",  12,  15, 0xFFE0},
  {"0-day",       25,   4, 0xF800},
  {"Exploit Kit", 18,   8, 0xF81F},
  {"Port Scan",    8,  20, 0x07FF},
};

// ==================== ESTADO DEL JUEGO ====================
#define PKM_SCREEN_TITLE      4  // pantalla de intro/título (mostrada al entrar por primera vez)
#define PKM_SCREEN_OVERWORLD  0
#define PKM_SCREEN_BATTLE     1
#define PKM_SCREEN_RESULT     2
#define PKM_SCREEN_TERMINAL   3  // diálogo de terminal de curación

static uint8_t  pkmScreen;
static bool     pkmResultWon = false;  // para redibujar result al volver del screensaver

// Estado del overworld
static uint8_t  pkmMapIdx;           // mapa actual (0=Bosque 1=Desierto 2=Nieve 3=Isla)
static int16_t  pkmPX, pkmPY;        // posición del jugador en tiles
static int8_t   pkmFacing;           // 0=abajo 1=arriba 2=izquierda 3=derecha
static uint8_t  pkmStepCount;        // pasos desde la última verificación de encuentro
static uint16_t pkmXP;
static uint8_t  pkmLevel;
static uint8_t  pkmHP, pkmMaxHP;
static uint8_t  pkmMovePP[PKM_MOVES];
static bool     pkmWalkAnim;
static uint32_t pkmWalkAnimMs;
static uint32_t pkmLastMoveMs;
static bool     pkmGameOver;
static bool     pkmGameWon;

// Tiempo de supervivencia + rango
static uint32_t pkmStartMs;        // millis() al inicio de la partida (entrada al overworld)
static uint32_t pkmSurvivedSecs;   // calculado al finalizar la partida
static uint8_t  pkmBestRank;       // 0-5, persistido en flash

// Cámara
static int16_t  pkmCamX, pkmCamY;

// Estado de batalla
static uint8_t  pkmBattleEnemyIdx;   // índice en pkmEnemies[]
static uint8_t  pkmEnemyHP;
static uint8_t  pkmBattlePhase;
static uint8_t  pkmBattleCursor;     // movimiento seleccionado (0-3)
static char     pkmBattleMsg[48];    // buffer de mensaje
static bool     pkmBattleMsg2;       // segunda línea de mensaje mostrada
static uint32_t pkmBattleDelayMs;    // espera antes de avanzar de fase
static bool     pkmBattlePlayerHurt; // efecto de parpadeo
static bool     pkmBattleEnemyHurt;
static bool     pkmBattleOver;
static bool     pkmBattleWon;
static uint8_t  pkmBattleAnimFrame;
static uint32_t pkmBattleAnimMs;

// Seguimiento de enemigos derrotados (para condición de victoria)
static uint16_t pkmDefeatedMask;   // máscara de bits de índices de enemigos derrotados al menos una vez
#define PKM_WIN_MASK  ((1<<PKM_MAX_ENEMIES)-1)   // derrotar a los 10 al menos una vez

// ==================== TABLA DE NIVELES ====================
static const uint16_t pkmXPTable[10] PROGMEM = {
  0, 20, 45, 80, 130, 200, 290, 400, 540, 720
};

static uint8_t pkmXPtoLevel(uint16_t xp) {
  for (int8_t lv=9; lv>=1; lv--)
    if (xp >= pgm_read_word(&pkmXPTable[lv])) return (uint8_t)(lv+1);
  return 1;
}

// ==================== SISTEMA DE RANGOS ====================
// Rango basado en el tiempo de supervivencia (segundos)
static uint8_t pkmXPToRank(uint16_t xp) {
  if (xp <  50) return 0;
  if (xp < 150) return 1;
  if (xp < 300) return 2;
  if (xp < 500) return 3;
  if (xp < 720) return 4;
  return 5;
}
static const char* pkmRankName(uint8_t rank) {
  switch(rank) {
    case 0: return "SCRIPT KIDDIE";
    case 1: return "NOOB HACKER";
    case 2: return "GREY HAT";
    case 3: return "BLACK HAT";
    case 4: return "ELITE HACKER";
    default: return "LEGENDARY";
  }
}
static uint16_t pkmRankColor(uint8_t rank) {
  switch(rank) {
    case 0: return 0x8410;
    case 1: return 0x07FF;
    case 2: return 0xFFE0;
    case 3: return 0xF800;
    case 4: return 0xFD20;
    default: return 0xF81F;
  }
}
static void pkmLoadBestRank() {
  prefs.begin("hackcon", true);
  pkmBestRank = (uint8_t)prefs.getUInt("pkm_rank", 0);
  prefs.end();
}
static void pkmSaveBestRank(uint8_t rank) {
  if (rank > pkmBestRank) {
    pkmBestRank = rank;
    prefs.begin("hackcon", false);
    prefs.putUInt("pkm_rank", pkmBestRank);
    prefs.end();
  }
}

static uint8_t pkmLevelToMaxHP(uint8_t lv) {
  return 20 + lv * 5;
}

static uint8_t pkmLevelToAtk(uint8_t lv) {
  return 3 + lv * 2;
}

// ==================== UTILIDADES DE MAPA ====================
static uint8_t pkmMapAt(int tx, int ty) {
  if (tx<0||tx>=PKM_MAP_W||ty<0||ty>=PKM_MAP_H) return 1;
  switch(pkmMapIdx) {
    case 0: return pgm_read_byte(&pkmMapForest[ty][tx]);
    case 1: return pgm_read_byte(&pkmMapDesert[ty][tx]);
    case 2: return pgm_read_byte(&pkmMapSnow[ty][tx]);
    case 3: return pgm_read_byte(&pkmMapIsland[ty][tx]);
    default: return 1;
  }
}
static bool pkmPassable(int tx, int ty) {
  uint8_t t = pkmMapAt(tx, ty);
  return t==0 || t==3 || t==4;  // el tile de portal es transitable
}
static bool pkmIsEncounterTile(int tx, int ty) {
  return pkmMapAt(tx, ty) == 0;
}
static bool pkmIsTerminal(int tx, int ty) {
  return pkmMapAt(tx, ty) == 3;
}
static bool pkmIsPortal(int tx, int ty) {
  return pkmMapAt(tx, ty) == 4;
}

// ==================== CÁMARA ====================
static void pkmUpdateCamera() {
  pkmCamX = 0;
  pkmCamY = 0;
}

// ==================== RENDERIZADOR DE TILES ====================
__attribute__((optimize("O3")))
static void pkmDrawTile(int tx, int ty) {
  int sx = tx * PKM_TDRAW - pkmCamX;
  int sy = ty * PKM_TDRAW - pkmCamY;
  if (sx+PKM_TDRAW<=0||sx>=PKM_VP_W||sy+PKM_TDRAW<=0||sy>=PKM_VP_H) return;
  if (tx<0||tx>=PKM_MAP_W||ty<0||ty>=PKM_MAP_H) return;
  uint8_t t = pkmMapAt(tx, ty);
  busForTFT();
  int TS = PKM_TDRAW;
  uint8_t h = (uint8_t)(tx*5+ty*7);

  if (t == 0) {
    switch(pkmMapIdx) {
      default:
      case 0: {
        uint16_t g1=0x0500, g2=0x0580, g3=0x06A0;
        tft.fillRect(sx, sy, TS, TS, g1);
        for (int i=0;i<TS;i+=3) {
          tft.drawPixel(sx+i, sy+(i&(TS-1)), g2);
          tft.drawPixel(sx+(i+1)%TS, sy+((i+2)&(TS-1)), g3);
        }
        if ((tx+ty)&1) { tft.fillRect(sx+1,sy+1,TS/2-1,TS/2-1,g2); tft.fillRect(sx+TS/2,sy+TS/2,TS/2,TS/2,g2); }
        else           { tft.fillRect(sx+TS/2,sy+1,TS/2,TS/2-1,g2); tft.fillRect(sx+1,sy+TS/2,TS/2-1,TS/2,g2); }
        uint8_t bx=(h&3)+1, bx2=((h>>2)&3)+6;
        tft.drawFastVLine(sx+bx,sy+2,4,g3);
        tft.drawFastVLine(sx+bx2,sy+6,3,0x07E0);
        if ((h&7)==3) tft.drawPixel(sx+5,sy+5,0xFFE0);
        if ((h&7)==6) tft.drawPixel(sx+3,sy+8,0xF81F);
        break;
      }
      case 1: {
        uint16_t s1=0xC600, s2=0xD640, s3=0xE6A0;
        tft.fillRect(sx, sy, TS, TS, s1);
        if ((tx+ty)&1) { tft.fillRect(sx+2,sy+2,TS-4,TS-4,s2); }
        else           { tft.fillRect(sx+1,sy+1,TS-2,4,s3); }
        if ((h&3)==0) tft.drawFastHLine(sx+1,sy+5,TS-2,s3);
        if ((h&3)==1) tft.drawFastHLine(sx+2,sy+8,TS-4,s2);
        if ((h&7)==4) tft.drawPixel(sx+4,sy+6,0x8400);
        break;
      }
      case 2: {
        uint16_t w1=0xEF7D, w2=0xF79E, w3=0xFFFF;
        tft.fillRect(sx, sy, TS, TS, w1);
        if ((tx+ty)&1) tft.fillRect(sx+2,sy+2,TS-4,TS-4,w2);
        if ((h&7)==2) tft.drawPixel(sx+3,sy+3,w3);
        if ((h&7)==5) tft.drawPixel(sx+8,sy+7,w3);
        if ((h&3)==0) tft.drawFastHLine(sx,sy+6,TS,0xC71C);
        break;
      }
      case 3: {
        uint16_t b1=0xD6C0, b2=0xE740, b3=0xFFC0;
        tft.fillRect(sx, sy, TS, TS, b1);
        if ((tx+ty)&1) tft.fillRect(sx+2,sy+2,TS-4,TS-4,b2);
        if ((h&7)==1) { tft.fillRect(sx+4,sy+5,3,2,b3); tft.drawRect(sx+4,sy+5,3,2,0xC600); }
        if ((h&7)==4) tft.drawPixel(sx+7,sy+3,0xFFFF);
        break;
      }
    }
    return;
  }

  if (t == 1) {
    uint8_t hh = (uint8_t)(tx*7+ty*11);
    switch(pkmMapIdx) {
      default:
      case 0: {
        uint16_t shadow=0x0200, dark=(hh&1)?0x0340:0x0280, mid=(hh&2)?0x03A0:0x0360, lite=(hh&4)?0x04A0:0x0440;
        tft.fillRect(sx,sy,TS,TS,shadow);
        tft.fillRect(sx+1,sy,TS-2,TS,dark); tft.fillRect(sx,sy+1,TS,TS-2,dark);
        tft.fillRect(sx+2,sy+1,TS-4,TS-3,mid);
        tft.fillRect(sx+3,sy+2,TS-6,TS-5,lite);
        tft.fillRect(sx+TS/2-2,sy+1,4,3,0x07E0);
        tft.fillRect(sx+TS/2-1,sy+TS-4,3,4,0x5140);
        tft.fillRect(sx+TS/2-2,sy+TS-2,5,2,0x5140);
        tft.drawRect(sx,sy,TS,TS,0x0180);
        break;
      }
      case 1: { // Desierto
        uint16_t sand=0xC600, cactus=0x0480, cactusL=0x0600;
        tft.fillRect(sx,sy,TS,TS,sand);
        tft.fillRect(sx+TS/2-2,sy+2,4,TS-3,cactus);
        tft.fillRect(sx+TS/2-1,sy+3,2,TS-4,cactusL);
        tft.fillRect(sx+2,sy+4,TS/2-2,3,cactus);
        tft.fillRect(sx+2,sy+2,3,4,cactus);
        tft.fillRect(sx+TS/2+2,sy+6,TS/2-3,3,cactus);
        tft.fillRect(sx+TS-4,sy+4,3,4,cactus);
        tft.drawPixel(sx+2,sy+2,(uint16_t)0xFFFF);
        tft.drawPixel(sx+TS-4,sy+4,(uint16_t)0xFFFF);
        break;
      }
      case 2: { // Nieve
        uint16_t snow=0xFFFF, ice=0x8C9F, shadow2=0x6B5A;
        tft.fillRect(sx,sy,TS,TS,ice);
        tft.fillRect(sx+1,sy+3,TS-2,TS-4,snow);
        tft.fillRect(sx+2,sy+1,TS-4,4,snow);
        tft.fillRect(sx+4,sy,TS-8,2,snow);
        tft.drawFastHLine(sx,sy+TS-1,TS,shadow2);
        tft.drawFastVLine(sx+TS-1,sy+1,TS-1,shadow2);
        tft.drawPixel(sx+3,sy+2,(uint16_t)0x07FF);
        tft.drawPixel(sx+TS-4,sy+4,(uint16_t)0x07FF);
        break;
      }
      case 3: { // Isla
        uint16_t trunk=0x8220, frond=0x0480, frondL=0x06A0, sand=0xD6C0;
        tft.fillRect(sx,sy,TS,TS,sand);
        tft.fillRect(sx+TS/2-1,sy+5,2,TS-5,trunk);
        tft.fillRect(sx+TS/2,  sy+3,2,4,trunk);
        tft.fillRect(sx+1,sy,TS-2,4,frond);
        tft.fillRect(sx,sy+1,TS,3,frond);
        tft.fillRect(sx+2,sy+1,TS-4,4,frondL);
        tft.fillRect(sx+TS/2-2,sy+4,2,2,(uint16_t)0x8220);
        tft.fillRect(sx+TS/2+1,sy+5,2,2,(uint16_t)0x8220);
        tft.drawRect(sx,sy,TS,TS,0x4100);
        break;
      }
    }
    return;
  }

  if (t == 3) {
    switch(pkmMapIdx) {
      default:
      case 0: {
        tft.fillRect(sx,sy,TS,TS,0x0480);
        tft.fillRect(sx+1,sy+1,TS-2,TS-2,0x0013);
        tft.fillRect(sx+2,sy+2,TS-4,TS-4,0x0237);
        tft.fillRect(sx+3,sy+3,TS-6,TS-6,0x044F);
        tft.drawRect(sx+1,sy+1,TS-2,TS-2,0x03DF);
        tft.drawRect(sx+2,sy+2,TS-4,TS-4,0x05FF);
        tft.drawFastHLine(sx+3,sy+TS/2,TS-6,0x07FF);
        tft.drawFastVLine(sx+TS/2,sy+3,TS-6,0x07FF);
        tft.fillRect(sx+TS/2-1,sy+TS/2-1,3,3,0xFFFF);
        break;
      }
      case 1: {
        tft.fillRect(sx,sy,TS,TS,0x0540);
        tft.fillRect(sx+1,sy+1,TS-2,TS-2,0x0013);
        tft.fillRect(sx+2,sy+2,TS-4,TS-4,0x024F);
        tft.fillRect(sx+3,sy+3,TS-6,TS-6,0x05BF);
        tft.drawRect(sx+1,sy+1,TS-2,TS-2,0xFFE0);
        tft.drawFastHLine(sx+3,sy+TS/2,TS-6,0x07FF);
        tft.drawFastVLine(sx+TS/2,sy+3,TS-6,0x07FF);
        tft.fillRect(sx+TS/2-1,sy+TS/2-1,3,3,0xFFFF);
        tft.drawFastVLine(sx+TS/2,sy,3,0x0480);
        break;
      }
      case 2: {
        tft.fillRect(sx,sy,TS,TS,0x4A5F);
        tft.fillRect(sx+1,sy+1,TS-2,TS-2,0x8010);
        tft.fillRect(sx+2,sy+2,TS-4,TS-4,0xF810);
        tft.fillRect(sx+3,sy+3,TS-6,TS-6,0xFCB0);
        tft.drawRect(sx+1,sy+1,TS-2,TS-2,0xF800);
        tft.drawFastHLine(sx+3,sy+TS/2,TS-6,0xFFFF);
        tft.drawFastVLine(sx+TS/2,sy+3,TS-6,0xFFFF);
        tft.fillRect(sx+TS/2-1,sy+TS/2-1,3,3,0xFFFF);
        tft.drawPixel(sx+3,sy,0xC638); tft.drawPixel(sx+8,sy+1,0xC638);
        break;
      }
      case 3: {
        tft.fillRect(sx,sy,TS,TS,0xD6C0);
        tft.fillRect(sx+2,sy+2,TS-4,TS-4,0x0013);
        tft.fillRect(sx+3,sy+3,TS-6,TS-6,0x024F);
        tft.drawRect(sx+2,sy+2,TS-4,TS-4,0x07FF);
        tft.fillRect(sx+TS/2-1,sy+TS/2-1,3,3,0xFFFF);
        tft.fillRect(sx+1,sy,2,2,0x8220);
        tft.fillRect(sx+TS-3,sy,2,2,0x8220);
        break;
      }
    }
    return;
  }

  if (t == 4) {
    uint32_t ms = millis();
    uint8_t pulse = (uint8_t)((ms >> 4) & 0x1F); // 0..31
    uint16_t inner = (pulse < 16) ? 0xF81F : 0xFFFF;
    tft.fillRect(sx,sy,TS,TS,0x0000);
    tft.drawRect(sx,sy,TS,TS,0xF81F);
    tft.drawRect(sx+1,sy+1,TS-2,TS-2,0x8010);
    tft.fillRect(sx+2,sy+2,TS-4,TS-4,inner);
    uint16_t arr = 0x0000;
    if (tx <= 1) { // portal oeste — flecha apunta a la izquierda
      tft.fillRect(sx+2,sy+TS/2-1,TS-4,2,arr);
      tft.fillRect(sx+2,sy+TS/2-2,3,4,arr);
    } else {       // portal este — flecha apunta a la derecha
      tft.fillRect(sx+2,sy+TS/2-1,TS-4,2,arr);
      tft.fillRect(sx+TS-5,sy+TS/2-2,3,4,arr);
    }
    return;
  }

  // Fallback (tile desconocido)
  uint16_t bgCol = pgm_read_word(&pkmMapBgColor[pkmMapIdx < PKM_NUM_MAPS ? pkmMapIdx : 0]);
  tft.fillRect(sx, sy, TS, TS, bgCol);
}

// ==================== SPRITE DEL JUGADOR ====================
__attribute__((optimize("O3")))
static void pkmDrawPlayer(int sx, int sy, bool walking) {
  busForTFT();
  uint16_t skin  = 0xFD20;  // piel cálida
  uint16_t hair  = 0x1082;  // cabello muy oscuro
  uint16_t hood  = 0x2A3F;  // sudadera verde azulado oscuro
  uint16_t hoodH = 0x3A5F;  // brillo de la sudadera
  uint16_t pack  = 0x6B4D;  // mochila gris
  uint16_t packH = 0x8C71;  // brillo de la mochila
  uint16_t legs  = 0x2104;  // jeans oscuros
  uint16_t shoe  = 0x18C3;  // zapatos
  uint16_t scrn  = 0x07FF;  // brillo cian de laptop/dispositivo
  uint16_t outl  = 0x0000;

  int L = sx-5, T = sy-7; // 10 de ancho x 14 de alto, centrado

  tft.fillRect(L+2, T,   6, 1, hair);
  tft.fillRect(L+2, T+1, 6, 1, hair);
  tft.fillRect(L+1, T+2, 8, 3, skin);
  tft.fillRect(L+2, T+1, 2, 2, hair);
  tft.fillRect(L+6, T+1, 2, 2, hair);
  if (pkmFacing == 0) {
    tft.drawPixel(L+3, T+3, 0x0000);
    tft.drawPixel(L+6, T+3, 0x0000);
    tft.drawPixel(L+3, T+4, skin);  // indicio de boca
    tft.drawPixel(L+4, T+4, 0x9800);
    tft.drawPixel(L+5, T+4, skin);
  } else if (pkmFacing == 1) {
    tft.fillRect(L+2, T,   6, 5, hair);
  } else {
    tft.drawPixel(pkmFacing==3 ? L+6 : L+3, T+3, 0x0000);
  }

  tft.fillRect(L,   T+5, 10, 5, hood);
  tft.fillRect(L+1, T+5,  8, 2, hoodH);
  tft.drawRect(L,   T+5, 10, 5, outl);
  tft.drawPixel(L+4, T+6, 0xFFFF);
  tft.drawPixel(L+5, T+6, 0xFFFF);

  if (pkmFacing != 1) {
    tft.fillRect(L+7, T+5, 3, 5, pack);
    tft.fillRect(L+8, T+6, 1, 3, packH);
    tft.drawRect(L+7, T+5, 3, 5, outl);
  }

  if (pkmFacing == 0) {
    tft.fillRect(L+2, T+8, 5, 3, 0x1082);
    tft.fillRect(L+3, T+8, 3, 2, scrn);
    tft.drawRect(L+2, T+8, 5, 3, outl);
  }

  if (walking) {
    tft.fillRect(L+2, T+10, 2, 3, legs);
    tft.fillRect(L+6, T+10, 2, 3, legs);
    tft.fillRect(L+2, T+12, 2, 2, shoe);
    tft.fillRect(L+5, T+11, 2, 2, shoe);
  } else {
    tft.fillRect(L+2, T+10, 2, 3, legs);
    tft.fillRect(L+5, T+10, 2, 3, legs);
    tft.fillRect(L+2, T+12, 2, 1, shoe);
    tft.fillRect(L+5, T+12, 2, 1, shoe);
  }

  tft.drawFastHLine(L+2, T+14, 6, 0x0200);
}

static void pkmErasePlayer() {
  int tx0=(int)(pkmPX*PKM_TDRAW-4-pkmCamX)/PKM_TDRAW - 1;
  int ty0=(int)(pkmPY*PKM_TDRAW-5-pkmCamY)/PKM_TDRAW - 1;
  int tx1=tx0+3, ty1=ty0+3;
  if(tx0<0)tx0=0; if(ty0<0)ty0=0;
  if(tx1>=PKM_MAP_W)tx1=PKM_MAP_W-1;
  if(ty1>=PKM_MAP_H)ty1=PKM_MAP_H-1;
  for(int ty=ty0;ty<=ty1;ty++)
    for(int tx=tx0;tx<=tx1;tx++)
      pkmDrawTile(tx,ty);
}

// ==================== HUD (Interfaz) ====================
static void pkmDrawHUD() {
  busForTFT();
  tft.fillRect(0, PKM_VP_H, 240, 240-PKM_VP_H, 0x0000);
  tft.drawFastHLine(0, PKM_VP_H, 240, COLDARK);

  tft.setTextSize(1);
  tft.setTextColor(0xBDF7);
  tft.setCursor(4, PKM_VP_H+4);
  tft.print("HP:");
  int barW = 60;
  int filled = (pkmHP * barW) / (pkmMaxHP > 0 ? pkmMaxHP : 1);
  uint16_t hpCol = (pkmHP > pkmMaxHP/2) ? 0x07E0 :
                   (pkmHP > pkmMaxHP/4) ? 0xFFE0 : 0xF800;
  tft.fillRect(24, PKM_VP_H+5, barW, 7, 0x1882);
  tft.fillRect(24, PKM_VP_H+5, filled, 7, hpCol);
  tft.drawRect(24, PKM_VP_H+5, barW, 7, 0x4208);
  tft.setTextColor(hpCol);
  tft.setCursor(88, PKM_VP_H+4);
  tft.print(pkmHP); tft.print("/"); tft.print(pkmMaxHP);

  tft.setTextColor(COLNEON);
  tft.setCursor(140, PKM_VP_H+4);
  tft.print("LV:"); tft.print(pkmLevel);

  uint16_t xpThis = pgm_read_word(&pkmXPTable[pkmLevel>1?pkmLevel-1:0]);
  uint16_t xpNext = pgm_read_word(&pkmXPTable[pkmLevel<10?pkmLevel:9]);
  int xpRange = xpNext - xpThis;
  int xpFill  = xpRange>0 ? ((pkmXP-xpThis)*50/xpRange) : 50;
  if(xpFill<0)xpFill=0; if(xpFill>50)xpFill=50;
  tft.fillRect(176, PKM_VP_H+5, 50, 7, 0x1882);
  tft.fillRect(176, PKM_VP_H+5, xpFill, 7, COLDIM);
  tft.drawRect(176, PKM_VP_H+5, 50, 7, 0x4208);

  static const char* const pkmMapNames[PKM_NUM_MAPS] = {"Forest","Desert","Snow","Island"};
  static const uint16_t    pkmMapNameColors[PKM_NUM_MAPS] = {0x07E0, 0xFFE0, 0xAFFF, 0x07FF};
  uint8_t mi = pkmMapIdx < PKM_NUM_MAPS ? pkmMapIdx : 0;
  tft.setTextColor(pkmMapNameColors[mi]);
  tft.setCursor(4, PKM_VP_H+16);
  tft.print(pkmMapNames[mi]);

  tft.setTextColor(0x8410);
  tft.setCursor(90, PKM_VP_H+16);
  tft.print("Def:");
  uint8_t dcount=0;
  for(int i=0;i<PKM_MAX_ENEMIES;i++) if(pkmDefeatedMask>>i&1) dcount++;
  tft.setTextColor(COLNEON);
  tft.print(dcount); tft.print("/"); tft.print(PKM_MAX_ENEMIES);

  tft.setTextColor(0x8410);
  tft.setCursor(178, PKM_VP_H+16);
  tft.print("XP:"); tft.setTextColor(0x07FF); tft.print(pkmXP);
}

// ==================== DIBUJO DEL OVERWORLD ====================
static void pkmDrawOverworld() {
  busForTFT();
  uint16_t bgCol = pgm_read_word(&pkmMapBgColor[pkmMapIdx < PKM_NUM_MAPS ? pkmMapIdx : 0]);
  tft.fillScreen(bgCol);
  int tx0=pkmCamX/PKM_TDRAW,     ty0=pkmCamY/PKM_TDRAW;
  int tx1=(pkmCamX+PKM_VP_W)/PKM_TDRAW+1, ty1=(pkmCamY+PKM_VP_H)/PKM_TDRAW+1;
  for(int ty=ty0;ty<=ty1;ty++)
    for(int tx=tx0;tx<=tx1;tx++)
      pkmDrawTile(tx,ty);
  int spx = pkmPX*PKM_TDRAW + PKM_TDRAW/2 - pkmCamX;
  int spy = pkmPY*PKM_TDRAW + PKM_TDRAW/2 - pkmCamY;
  pkmDrawPlayer(spx, spy, pkmWalkAnim);
  pkmDrawHUD();
}

// ==================== ESCENA DE BATALLA ====================

static void pkmBattleDrawEnemyHP() {
  busForTFT();
  const PkmEnemy* e = &pkmEnemies[pkmBattleEnemyIdx];
  uint8_t emaxhp = e->maxHp;
  int barW = 70;
  int filled = (pkmEnemyHP * barW) / (emaxhp > 0 ? emaxhp : 1);
  uint16_t col = (pkmEnemyHP > emaxhp/2) ? 0x07E0 :
                 (pkmEnemyHP > emaxhp/4) ? 0xFFE0 : 0xF800;
  tft.fillRect(26, 26, 84, 17, 0x0000);
  tft.fillRect(26, 26, barW, 7, 0x1882);
  tft.fillRect(26, 26, filled, 7, col);
  tft.drawRect(26, 26, barW, 7, 0x4208);
  tft.setTextSize(1);
  tft.setTextColor(0xBDF7);
  tft.setCursor(26, 35);
  tft.print(pkmEnemyHP);
  tft.print("/");
  tft.print(emaxhp);
}

static void pkmBattleDrawPlayerHP() {
  busForTFT();
  int barW = 70;
  int filled = (pkmHP * barW) / (pkmMaxHP > 0 ? pkmMaxHP : 1);
  uint16_t col = (pkmHP > pkmMaxHP/2) ? 0x07E0 :
                 (pkmHP > pkmMaxHP/4) ? 0xFFE0 : 0xF800;
  tft.fillRect(150, 111, 84, 17, 0x0000);
  tft.fillRect(150, 111, barW, 7, 0x1882);
  tft.fillRect(150, 111, filled, 7, col);
  tft.drawRect(150, 111, barW, 7, 0x4208);
  tft.setTextSize(1);
  tft.setTextColor(0xBDF7);
  tft.setCursor(150, 120);
  tft.print(pkmHP);
  tft.print("/");
  tft.print(pkmMaxHP);
}

// Sprite de batalla del enemigo
__attribute__((optimize("O3")))
static void pkmDrawEnemySprite(bool hurt) {
  busForTFT();
  const PkmEnemy* e = &pkmEnemies[pkmBattleEnemyIdx];
  uint16_t col  = hurt ? (uint16_t)0xFFFF : e->color;
  uint16_t dark = ((col>>1)&0x7BEF);
  uint16_t lite = col | 0x0020;  // leve brillo
  uint16_t outl = 0x0000;
  uint16_t bg   = 0x0820;   // color de fondo de batalla

  int cx = 175, cy = 38;    // centro del sprite del enemigo (arriba-derecha)

  tft.fillRect(cx-28, cy-26, 56, 64, bg);
  for (int gx=120; gx<240; gx+=20) if (gx>=cx-28 && gx<cx+28) tft.drawFastVLine(gx, cy-26, 64, 0x0440);
  for (int gy=10;  gy<79;  gy+=10) if (gy>=cy-26 && gy<cy+38) tft.drawFastHLine(cx-28, gy, 56, 0x0220);
  for (int sl=2;   sl<79;  sl+=4)  if (sl>=cy-26 && sl<cy+38) tft.drawFastHLine(cx-28, sl, 56, 0x0008);

  switch(pkmBattleEnemyIdx) {
    case 0:
    case 1:
    case 7: {
      tft.fillRect(cx-18, cy-22, 36, 28, outl);
      tft.fillRect(cx-16, cy-20, 32, 26, dark);
      tft.fillRect(cx-14, cy-18, 28, 24, col);
      // Parte inferior puntiaguda
      tft.fillRect(cx-10, cy+6,  20, 6, col);
      tft.fillRect(cx-6,  cy+12, 12, 6, col);
      tft.fillRect(cx-2,  cy+18, 4,  4, col);
      tft.fillRect(cx-10, cy+6,  20, 1, dark);
      tft.fillRect(cx-8, cy-10, 16, 3, dark);
      tft.fillRect(cx-6, cy-14, 3, 10, dark);
      tft.fillRect(cx+3, cy-16, 3, 14, dark);
      tft.drawRect(cx-18, cy-22, 36, 28, lite);
      break;
    }
    case 2:
    case 5: {
      tft.fillRect(cx-16, cy-16, 32, 32, outl);
      tft.fillRect(cx-14, cy-14, 28, 28, dark);
      tft.fillRect(cx-12, cy-12, 24, 24, col);
      tft.fillRect(cx-8,  cy-8,  16, 16, dark);
      tft.fillRect(cx-5,  cy-5,  10, 10, col);
      tft.fillRect(cx-4,  cy-20, 8, 6, col);
      tft.fillRect(cx-4,  cy+14, 8, 6, col);
      tft.fillRect(cx-20, cy-4,  6, 8, col);
      tft.fillRect(cx+14, cy-4,  6, 8, col);
      tft.drawRect(cx-16, cy-16, 32, 32, lite);
      break;
    }
    case 3:
    case 8: {
      tft.fillRect(cx-18, cy-10, 36, 26, dark);
      tft.fillRect(cx-16, cy-8,  32, 24, col);
      tft.fillRect(cx-18, cy-20, 8, 12, col);
      tft.fillRect(cx-4,  cy-20, 8, 12, col);
      tft.fillRect(cx+10, cy-20, 8, 12, col);
      tft.fillRect(cx-5, cy+2, 10, 14, dark);
      tft.fillRect(cx-4, cy,   8,  4, dark);
      tft.fillRect(cx-14, cy-5, 2, 8, dark);
      tft.fillRect(cx+12, cy-5, 2, 8, dark);
      tft.drawRect(cx-18, cy-20, 36, 36, lite);
      break;
    }
    case 4:
    case 6: {
      for (int i=0;i<20;i++) {
        int w = (i*2)+2;
        if(w>40) w=40;
        tft.drawFastHLine(cx-w/2, cy-20+i, w, (i<10)?col:dark);
      }
      tft.fillRect(cx-18, cy-2, 36, 4, lite);
      tft.fillRect(cx-12, cy-1, 24, 2, outl);
      tft.fillRect(cx-5, cy-4, 10, 8, outl);
      tft.fillRect(cx-3, cy-2,  6, 4, 0xFFFF);
      tft.fillRect(cx-1, cy-1,  2, 2, outl);
      tft.drawRect(cx-18, cy-20, 36, 36, lite);
      break;
    }
    default:
    case 9: {
      for (int row=0;row<4;row++) {
        int ry = cy-18+row*9;
        uint16_t bc = (row&1) ? col : dark;
        tft.fillRect(cx-18, ry, 36, 8, bc);
        tft.drawFastHLine(cx-18, ry, 36, outl);
        int off = (row&1) ? 9 : 0;
        tft.drawFastVLine(cx-18+off,   ry, 8, outl);
        tft.drawFastVLine(cx-18+off+18,ry, 8, outl);
      }
      tft.fillRect(cx-16, cy-24, 4, 8, 0xF800);
      tft.fillRect(cx-7,  cy-26, 4, 10, 0xFD20);
      tft.fillRect(cx+3,  cy-24, 4, 8, 0xF800);
      tft.fillRect(cx+12, cy-26, 4, 10, 0xFD20);
      tft.drawRect(cx-18, cy-18, 36, 36, lite);
      break;
    }
  }

}

// Sprite de batalla del jugador
__attribute__((optimize("O3")))
static void pkmDrawPlayerBattleSprite(bool hurt) {
  busForTFT();
  uint16_t skin  = hurt ? (uint16_t)0xFFFF : (uint16_t)0xFD20;
  uint16_t hair  = 0x1082;
  uint16_t hood  = 0x2A3F;
  uint16_t hoodH = 0x3A5F;
  uint16_t hoodS = 0x1A2F;  // sombra de la sudadera
  uint16_t desk  = 0x4228;
  uint16_t lap   = 0x1082;
  uint16_t scrn  = 0x07FF;
  uint16_t scrnG = 0x07E0;  // brillo verde del texto en la terminal
  uint16_t outl  = 0x0000;
  uint16_t chair = 0x3186;
  uint16_t bg    = 0x0410;

  int cx=55, cy=136;

  tft.fillRect(cx-30, cy-46, 60, 60, bg);
  for (int gl=83; gl<156; gl+=8)
    if (gl >= cy-46 && gl < cy+14)
      tft.drawFastHLine(cx-30, gl, 60, 0x0208);

  tft.fillRect(cx-20, cy-44, 40, 32, chair);
  tft.fillRect(cx-18, cy-42, 36, 28, (uint16_t)0x4228);
  tft.drawRect(cx-20, cy-44, 40, 32, outl);

  tft.fillRect(cx-10, cy-44, 20, 16, hood);
  tft.fillRect(cx-8,  cy-42, 16, 12, hair);
  tft.fillRect(cx-6,  cy-40, 12,  8, skin);
  tft.drawRect(cx-10, cy-44, 20, 16, outl);
  tft.drawFastVLine(cx-9, cy-42, 10, hoodS);
  tft.drawFastVLine(cx+8, cy-42, 10, hoodS);

  tft.fillRect(cx-20, cy-28, 40, 18, hood);
  tft.fillRect(cx-18, cy-28, 36,  6, hoodH);
  tft.fillRect(cx-20, cy-28,  5, 18, hoodS);
  tft.fillRect(cx+15, cy-28,  5, 18, hoodS);
  tft.drawRect(cx-20, cy-28, 40, 18, outl);
  tft.fillRect(cx-7, cy-18, 14, 6, hoodS);
  tft.drawRect(cx-7, cy-18, 14, 6, outl);

  tft.fillRect(cx-24, cy-14, 10, 8, hood);
  tft.fillRect(cx+14, cy-14, 10, 8, hood);
  tft.fillRect(cx-22, cy-10,  6, 4, skin);
  tft.fillRect(cx+16, cy-10,  6, 4, skin);
  tft.drawRect(cx-24, cy-14, 10, 8, outl);
  tft.drawRect(cx+14, cy-14, 10, 8, outl);

  tft.fillRect(cx-28, cy-6,  56, 5, desk);
  tft.fillRect(cx-28, cy-7,  56, 2, (uint16_t)0x6B4D);
  tft.drawRect(cx-28, cy-6,  56, 5, outl);

  tft.fillRect(cx-18, cy-14, 36, 10, lap);
  tft.fillRect(cx-17, cy-13, 34,  8, (uint16_t)0x2104);
  tft.fillRect(cx-16, cy-24, 32, 12, lap);
  tft.fillRect(cx-15, cy-23, 30, 10, scrn);
  tft.drawFastHLine(cx-13, cy-21, 10, scrnG);
  tft.drawFastHLine(cx-13, cy-19, 14, scrnG);
  tft.drawFastHLine(cx-13, cy-17, 8,  scrnG);
  tft.drawFastHLine(cx-13, cy-15, 12, scrnG);
  tft.fillRect(cx-1, cy-21, 2, 6, (uint16_t)0xFFFF);
  tft.drawFastHLine(cx+8, cy-23, 5, (uint16_t)0x8FFF);
  tft.drawRect(cx-16, cy-24, 32, 12, outl);
  tft.drawRect(cx-18, cy-14, 36, 10, outl);

}

static void pkmBattleDrawMsg(const char* line1, const char* line2=nullptr) {
  busForTFT();
  tft.fillRect(1, 158, 238, 40, 0x0000);
  tft.drawRect(0, 157, 240, 42, COLDARK);
  tft.drawRect(1, 158, 238, 40, 0x1082);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(6, 163);
  tft.print(line1);
  if (line2) {
    tft.setCursor(6, 177);
    tft.setTextColor(0xBDF7);
    tft.print(line2);
  }
}

static void pkmBattleDrawMoves() {
  busForTFT();
  tft.fillRect(0, 200, 240, 40, 0x0000);
  tft.drawFastHLine(0, 199, 240, COLDARK);

  for (int i=0;i<PKM_MOVES;i++) {
    int mcol = (i%2) * 120;
    int mrow = (i<2) ? 201 : 218;
    bool sel  = (i == pkmBattleCursor);
    uint16_t movCol = pkmMoves[i].color;
    uint16_t bg     = sel ? (uint16_t)0x0840 : (uint16_t)0x0000;
    uint16_t bdr    = sel ? COLNEON : (uint16_t)0x2104;
    uint16_t txt    = sel ? COLNEON : movCol;

    tft.fillRect(mcol+1, mrow, 118, 15, bg);
    tft.drawRect(mcol,   mrow-1, 120, 17, bdr);
    tft.fillRect(mcol+1, mrow, 3, 15, sel ? COLNEON : movCol);

    tft.setTextSize(1);
    tft.setTextColor(txt);
    tft.setCursor(mcol+8, mrow+4);
    tft.print(pkmMoves[i].name);
    uint16_t ppCol = pkmMovePP[i] > 0 ? (uint16_t)0x8410 : (uint16_t)0xF800;
    tft.setTextColor(ppCol);
    tft.setCursor(mcol+86, mrow+4);
    tft.print(pkmMovePP[i]);
  }
}

static void pkmBattleDrawFull() {
  busForTFT();
  tft.fillScreen(0x0000);

  tft.fillRect(0, 0, 240, 79, 0x0820);
  for (int gx=120; gx<240; gx+=20) tft.drawFastVLine(gx, 0, 79, 0x0440);
  for (int gy=10;  gy<79;  gy+=10) tft.drawFastHLine(120, gy, 120, 0x0220);
  for (int sl=2;   sl<79;  sl+=4)  tft.drawFastHLine(120, sl, 120, 0x0008);

  tft.drawFastHLine(0, 79, 240, COLDARK);
  tft.drawFastHLine(0, 80, 240, 0x0440);

  const PkmEnemy* e = &pkmEnemies[pkmBattleEnemyIdx];
  tft.fillRect(2, 3, 112, 52, 0x0000);
  tft.drawRect(2, 3, 112, 52, 0x4208);
  tft.drawRect(3, 4, 110, 50, 0x1882);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(8, 10);
  { char nm[13]; snprintf(nm, sizeof(nm), "%s", e->name); tft.print(nm); }
  tft.setTextColor(0x8410);
  tft.setCursor(84, 10);
  tft.print("LV");
  tft.setTextColor(0xBDF7);
  tft.print(e->maxHp / 5 + 2);
  tft.drawFastHLine(3, 21, 110, 0x2945);
  tft.setTextColor(0x8410);
  tft.setCursor(8, 27);
  tft.print("HP");

  pkmBattleDrawEnemyHP();  // draws bar at (26,26) and numbers at (26,35)

  tft.fillRect(0, 81, 240, 75, 0x0410);
  for (int gl=83; gl<156; gl+=8) tft.drawFastHLine(0, gl, 115, 0x0208);
  tft.drawRect(0, 81, 115, 75, COLDARK);

  tft.fillRect(126, 87, 112, 56, 0x0000);
  tft.drawRect(126, 87, 112, 56, 0x4208);
  tft.drawRect(127, 88, 110, 54, 0x1882);
  tft.setTextSize(1);
  tft.setTextColor(0x07FF);
  tft.setCursor(132, 95);
  tft.print("HACKER");
  tft.setTextColor(0x8410);
  tft.setCursor(190, 95);
  tft.print("LV");
  tft.setTextColor(0xBDF7);
  tft.print(pkmLevel);
  tft.drawFastHLine(127, 106, 110, 0x2945);
  tft.setTextColor(0x8410);
  tft.setCursor(132, 112);
  tft.print("HP");

  pkmBattleDrawPlayerHP();  // draws bar at (150,111) and numbers at (150,120)

  pkmDrawEnemySprite(false);
  pkmDrawPlayerBattleSprite(false);

  char buf[48];
  snprintf(buf, sizeof(buf), "A wild %s appeared!", e->name);
  pkmBattleDrawMsg(buf, "Choose your attack!");

  pkmBattleDrawMoves();
}

// ==================== DIÁLOGO DE TERMINAL ====================
static void pkmDrawTerminal() {
  busForTFT();
  tft.fillRect(20, 60, 200, 120, 0x0000);
  tft.drawRect(20, 60, 200, 120, 0x07FF);
  tft.drawRect(21, 61, 198, 118, COLDARK);

  tft.setTextSize(1);
  tft.setTextColor(0x07FF);
  tft.setCursor(78, 70);   // "HEALING SPRING" 14*6=84 → 120-42=78
  tft.print("HEALING SPRING");

  tft.drawFastHLine(22, 82, 196, COLDARK);

  tft.setTextColor(0x07E0);
  tft.setCursor(72, 92);   // "* You drink from" 16*6=96 → 120-48=72
  tft.print("* You drink from");
  tft.setCursor(81, 104);  // "the spring..." 13*6=78 → 120-39=81
  tft.print("the spring...");
  tft.setTextColor(0xFFFF);
  tft.setCursor(66, 116);  // "HP fully restored!" 18*6=108 → 120-54=66
  tft.print("HP fully restored!");

  {
    char hpBuf[32];
    snprintf(hpBuf, sizeof(hpBuf), "HP: %d -> %d", pkmHP, pkmMaxHP);
    int startX = 120 - ((int)strlen(hpBuf) * 6) / 2;
    char preBuf[24];
    snprintf(preBuf, sizeof(preBuf), "HP: %d -> ", pkmHP);
    tft.setTextColor(0xBDF7);
    tft.setCursor(startX, 130);
    tft.print(preBuf);
    tft.setTextColor(0x07E0);
    tft.print(pkmMaxHP);
  }

  tft.setTextColor(COLDIM);
  tft.setCursor(66, 158);  // "OK or BACK to exit" 18*6=108 → 120-54=66
  tft.print("OK or BACK to exit");
}

// ==================== PANTALLA DE TÍTULO ====================
static void pkmDrawTitle() {
  busForTFT();
  tft.fillScreen(0x0000);

  for (int y = 0; y < 240; y += 6) tft.drawFastHLine(0, y, 240, 0x0820);

  tft.fillRect(10, 18, 220, 58, 0x0000);
  tft.drawRect(10, 18, 220, 58, COLNEON);
  tft.drawRect(12, 20, 216, 54, 0x0440);

  tft.setTextSize(2);
  tft.setTextColor(COLNEON);
  tft.setCursor(60, 30);
  tft.print("HACKER MON");

  tft.setTextSize(1);
  tft.setTextColor(0x07FF);
  tft.setCursor(69, 58);
  tft.print(">> Pwn the Net <<");

  tft.fillRect(20, 96, 200, 68, 0x0000);
  tft.drawRect(20, 96, 200, 68, 0x4208);

  tft.setTextSize(1);
  tft.setTextColor(0x8410);
  tft.setCursor(90, 104);
  tft.print("Best Rank:");

  const char* rname = pkmRankName(pkmBestRank);
  uint16_t    rcol  = pkmRankColor(pkmBestRank);
  tft.setTextSize(2);
  tft.setTextColor(rcol);
  tft.setCursor(120 - (int)(strlen(rname) * 12) / 2, 116);
  tft.print(rname);

  int dotX = 78;
  for (uint8_t i = 0; i < 6; i++) {
    uint16_t c = (i <= pkmBestRank) ? pkmRankColor(i) : (uint16_t)0x2104;
    tft.fillRect(dotX + i*14, 146, 10, 6, c);
  }

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(72, 182);
  tft.print("Press OK to play");

  tft.setTextColor(0x4208);
  tft.setCursor(72, 196);
  tft.print("BACK = main menu");
}

// ==================== PANTALLA DE RESULTADOS ====================
static void pkmDrawResult(bool won) {
  busForTFT();
  tft.fillRect(16, 62, 208, 122, 0x0000);
  tft.drawRect(16, 62, 208, 122, won ? (uint16_t)COLNEON : (uint16_t)0xF800);
  tft.drawRect(18, 64, 204, 118, won ? (uint16_t)COLDIM  : (uint16_t)0xA000);

  tft.setTextSize(2);
  tft.setTextColor(won ? (uint16_t)COLNEON : (uint16_t)0xF800);
  if (won) { tft.setCursor(84, 72); tft.print("PWNED!"); }
  else     { tft.setCursor(66, 72); tft.print("GAME OVER"); }

  uint8_t rank = pkmXPToRank(pkmXP);
  tft.setTextSize(1);
  tft.setTextColor(pkmRankColor(rank));
  const char* rname = pkmRankName(rank);
  tft.setCursor(120 - (int)(strlen(rname)*6)/2, 96);
  tft.print(rname);

  tft.setTextSize(1);
  tft.setTextColor(0xBDF7);

  uint32_t mm = pkmSurvivedSecs / 60;
  uint32_t ss = pkmSurvivedSecs % 60;
  char rbuf[32];
  snprintf(rbuf, sizeof(rbuf), "Time: %lum %02lus", mm, ss);
  tft.setCursor(120 - (int)(strlen(rbuf)*6)/2, 110);
  tft.print(rbuf);

  snprintf(rbuf, sizeof(rbuf), "LV:%d  XP:%d", pkmLevel, pkmXP);
  tft.setCursor(120 - (int)(strlen(rbuf)*6)/2, 122);
  tft.print(rbuf);

  uint8_t dc=0;
  for(int i=0;i<PKM_MAX_ENEMIES;i++) if(pkmDefeatedMask>>i&1) dc++;
  snprintf(rbuf, sizeof(rbuf), "Defeated: %d/%d", dc, PKM_MAX_ENEMIES);
  tft.setCursor(120 - (int)(strlen(rbuf)*6)/2, 134);
  tft.print(rbuf);

  // Indicador de nuevo mejor rango — se muestra si el rango de esta partida iguala el guardado
  if (rank >= pkmBestRank) {
    tft.setTextColor(0xFFE0);
    tft.setCursor(120 - (int)(6*9)/2, 148);
    tft.print("NEW BEST!");
  }

  tft.setTextColor(COLDIM);
  tft.setCursor(63, 166);
  tft.print("OK=retry  BACK=menu");
}

// ==================== TRANSICIÓN DE MAPA ====================
static void pkmDoPortal() {
  // Efecto de destello
  busForTFT();
  for (int i=0; i<3; i++) {
    tft.fillRect(0,0,PKM_VP_W,PKM_VP_H,0xFFFF); delay(60);
    tft.fillRect(0,0,PKM_VP_W,PKM_VP_H,0x0000); delay(60);
  }
  // Avanzar al siguiente mapa
  PkmPortal dest;
  dest.mapIdx = pgm_read_byte (&pkmPortals[pkmMapIdx].mapIdx);
  dest.px     = (int8_t)pgm_read_byte(&pkmPortals[pkmMapIdx].px);
  dest.py     = (int8_t)pgm_read_byte(&pkmPortals[pkmMapIdx].py);
  pkmMapIdx = dest.mapIdx;
  pkmPX = dest.px;
  pkmPY = dest.py;
  pkmUpdateCamera();
  pkmDrawOverworld();
}

// ==================== INICIALIZACIÓN ====================
static void pkmInit() {
  pkmMapIdx = 0;
  pkmPX     = 10;
  pkmPY     = 10;
  pkmFacing = 0;
  pkmStepCount = 0;
  pkmXP     = 0;
  pkmLevel  = 1;
  pkmMaxHP  = pkmLevelToMaxHP(1);
  pkmHP     = pkmMaxHP;
  for(int i=0;i<PKM_MOVES;i++) {
    pkmMovePP[i] = pkmMoves[i].pp;
  }
  pkmWalkAnim    = false;
  pkmWalkAnimMs  = millis();
  pkmLastMoveMs  = millis();
  pkmGameOver    = false;
  pkmGameWon     = false;
  pkmDefeatedMask = 0;
  pkmSurvivedSecs = 0;
  pkmStartMs     = millis();  // iniciar temporizador de supervivencia
  pkmScreen      = PKM_SCREEN_OVERWORLD;
  pkmBattlePhase = 0;
  pkmBattleCursor = 0;
  pkmUpdateCamera();
  pkmMusicStart(0);
}

static void drawHackerMon() {
  pkmLoadBestRank();
  pkmScreen = PKM_SCREEN_TITLE;
  pkmDrawTitle();
}

// ==================== LÓGICA DE BATALLA ====================

// Selecciona un enemigo aleatorio disponible para el nivel actual del jugador
// Usa el campo minLevel integrado en cada PkmEnemy — imposible que se desincronice
static uint8_t pkmPickEnemy() {
  uint8_t pool[PKM_MAX_ENEMIES];
  uint8_t pcount = 0;
  for (uint8_t i = 0; i < PKM_MAX_ENEMIES; i++) {
    if (pkmLevel >= pkmEnemies[i].minLevel) pool[pcount++] = i;
  }
  if (pcount == 0) return 0;
  return pool[(uint8_t)(millis() & 0xFF) % pcount];
}

static void pkmStartBattle(uint8_t enemyIdx) {
  if (enemyIdx >= PKM_MAX_ENEMIES) enemyIdx = 0;
  pkmBattleEnemyIdx = enemyIdx;
  uint8_t mhp = pkmEnemies[enemyIdx].maxHp;
  pkmEnemyHP       = mhp;
  pkmBattlePhase   = 0;
  pkmBattleCursor  = 0;
  pkmBattleOver    = false;
  pkmBattleWon     = false;
  pkmBattlePlayerHurt = false;
  pkmBattleEnemyHurt  = false;
  pkmBattleAnimFrame  = 0;
  pkmBattleAnimMs     = millis();
  pkmBattleDelayMs    = 0;
  pkmScreen = PKM_SCREEN_BATTLE;
  pkmMusicStop();
  pkmMusicStart(1); // música de batalla
  pkmBattleDrawFull();
}

static uint8_t pkmRand8() {
  // LFSR simple usando millis + XP
  static uint8_t r = 42;
  r ^= (uint8_t)(millis()>>3);
  r ^= r<<3;
  r ^= r>>2;
  r ^= (uint8_t)pkmXP;
  return r;
}

// Aplicar daño al enemigo, gestionar KO
static void pkmApplyMoveToEnemy(uint8_t moveIdx) {
  uint8_t power = pkmMoves[moveIdx].power;
  uint8_t atk   = pkmLevelToAtk(pkmLevel);
  uint8_t def   = pkmEnemies[pkmBattleEnemyIdx].defense;
  // Daño = (power + atk + rand[0..4]) - def, mínimo 1
  uint8_t dmg = power + atk + (pkmRand8()&3);
  if (dmg > def+2) dmg -= def;
  else dmg = 1;

  if (dmg >= pkmEnemyHP) {
    pkmEnemyHP = 0;
    pkmBattleWon  = true;
    pkmBattleOver = true;
  } else {
    pkmEnemyHP -= dmg;
  }
  // Parpadeo del enemigo
  pkmBattleEnemyHurt = true;
  pkmBattleAnimFrame = 0;
  pkmBattleAnimMs    = millis();

  // Construir mensaje
  snprintf(pkmBattleMsg, sizeof(pkmBattleMsg), "Hacker used %s!", pkmMoves[moveIdx].name);
  pkmBattleMsg2 = false;
}

// El enemigo ataca al jugador
static void pkmEnemyAttack() {
  uint8_t eatk = pkmEnemies[pkmBattleEnemyIdx].attack;
  uint8_t dmg  = eatk + (pkmRand8()&3);
  // La defensa del jugador escala con el nivel
  uint8_t pdef = pkmLevel / 2;
  if (dmg > pdef+1) dmg -= pdef;
  else dmg = 1;

  if (dmg >= pkmHP) {
    pkmHP = 0;
    pkmGameOver  = true;
    pkmBattleOver = true;
    pkmBattleWon  = false;
    pkmMusicStop();
    pkmSfxGameOverStart();
  } else {
    pkmHP -= dmg;
  }
  pkmBattlePlayerHurt = true;
  pkmBattleAnimFrame  = 0;
  pkmBattleAnimMs     = millis();

  snprintf(pkmBattleMsg, sizeof(pkmBattleMsg), "%s attacked!", pkmEnemies[pkmBattleEnemyIdx].name);
  pkmBattleMsg2 = false;
}

// Llamado tras ganar la batalla — otorgar XP, verificar subida de nivel
static void pkmAwardXP() {
  uint8_t xpGain = pkmEnemies[pkmBattleEnemyIdx].xpGiven;
  if (pkmBattleEnemyIdx < PKM_MAX_ENEMIES) pkmDefeatedMask |= (1 << pkmBattleEnemyIdx);
  pkmXP += xpGain;

  uint8_t newLevel = pkmXPtoLevel(pkmXP);
  if (newLevel > pkmLevel && newLevel <= 10) {
    pkmLevel  = newLevel;
    uint8_t newMax = pkmLevelToMaxHP(pkmLevel);
    pkmHP    += (newMax - pkmMaxHP); // curar al subir de nivel
    pkmMaxHP  = newMax;
    if (pkmHP > pkmMaxHP) pkmHP = pkmMaxHP;
    snprintf(pkmBattleMsg, sizeof(pkmBattleMsg), "Level UP! Now LV %d!", pkmLevel);
  } else {
    snprintf(pkmBattleMsg, sizeof(pkmBattleMsg), "+%d XP! Enemy defeated!", xpGain);
  }

  // Verificar si todos los enemigos fueron derrotados
  if ((pkmDefeatedMask & PKM_WIN_MASK) == PKM_WIN_MASK) {
    pkmGameWon = true;
  }
}

// ==================== TICK DE BATALLA ====================
static void pkmBattleTick(const ButtonsEdge& b) {
  uint32_t now = millis();

  // Animar parpadeo de daño
  if (pkmBattleEnemyHurt || pkmBattlePlayerHurt) {
    if ((int32_t)(now - pkmBattleAnimMs) >= 120) {
      pkmBattleAnimFrame++;
      pkmBattleAnimMs = now;
      if (pkmBattleAnimFrame >= 3) {
        if (pkmBattleEnemyHurt)  { pkmBattleEnemyHurt=false;  pkmDrawEnemySprite(false); }
        if (pkmBattlePlayerHurt) { pkmBattlePlayerHurt=false; pkmDrawPlayerBattleSprite(false); }
        pkmBattleDrawPlayerHP();
        pkmBattleDrawEnemyHP();
      } else {
        bool flash = (pkmBattleAnimFrame&1);
        if (pkmBattleEnemyHurt)  pkmDrawEnemySprite(flash);
        if (pkmBattlePlayerHurt) pkmDrawPlayerBattleSprite(flash);
      }
    }
    return; // esperar animación
  }

  // Fase 0: Jugador elige movimiento
  if (pkmBattlePhase == 0) {
    if (b.up)   { pkmBattleCursor=(pkmBattleCursor+2)%4; pkmBattleDrawMoves(); }
    if (b.down) { pkmBattleCursor=(pkmBattleCursor+2)%4; pkmBattleDrawMoves(); }
    if (b.ok) {
      pkmBattleCursor = (pkmBattleCursor+1)%4; // cycle right in 2-column layout
      // en realidad: tratar izq=0/2, der=1/3 como columnas; arriba/abajo cambia fila
      pkmBattleDrawMoves();
    }
    if (b.back) {
      // BACK sale de la batalla (huir) — coste de HP completo: recibe 5 de daño
      if (pkmHP > 5) pkmHP -= 5; else pkmHP = 1;
      pkmMusicStop();
      pkmMusicStart(0);
      pkmScreen = PKM_SCREEN_OVERWORLD;
      pkmUpdateCamera();
      pkmDrawOverworld();
      return;
    }

    // Verificar si ok fue "confirmar" — usar botón izquierdo como confirmar
    // ARRIBA/ABAJO desplaza el cursor, BACK = huir, OK = navegar
    // Usar ARRIBA como "confirmar ataque" — intuitivo para el badge
    if (b.up) {
      // Ya gestionado arriba, pero queremos que ARRIBA signifique "confirmar" en segunda pulsación
      // Simple: tratar b.ok como alternancia izquierda/derecha y PUSH1PIN mantenido como confirmar
    }

    // En definitiva, mapeo: ARRIBA=subir cursor, ABAJO=bajar cursor, BACK=huir, OK=confirmar
    // Reinterpretación: rejilla de 2 filas x 2 columnas
    // Simplemente: ARRIBA/ABAJO para ciclar movimientos 0-3, OK para confirmar, BACK para huir
    return;
  }

  // Fase 1: resolviendo movimiento del jugador (esperar retardo)
  if (pkmBattlePhase == 1) {
    if ((int32_t)(now - pkmBattleDelayMs) < 0) return;

    if (pkmBattleOver) {
      if (pkmBattleWon) {
        // Victoria
        pkmAwardXP();
        pkmMusicStop();
        pkmMusicStart(2); // jingle de victoria
        pkmBattleDrawMsg(pkmBattleMsg, "Press OK to continue");
        pkmBattlePhase = 3; // esperar confirmación del jugador
      } else {
        pkmBattleDrawMsg("You were defeated...", "Press OK or BACK");
        pkmBattlePhase = 3;
      }
      return;
    }

    // Turno del enemigo
    pkmEnemyAttack();
    pkmBattleDrawMsg(pkmBattleMsg, nullptr);
    pkmBattleDelayMs = now + 1200;
    pkmBattlePhase = 2;
    return;
  }

  // Fase 2: resolviendo movimiento del enemigo
  if (pkmBattlePhase == 2) {
    if ((int32_t)(now - pkmBattleDelayMs) < 0) return;

    if (pkmBattleOver) {
      pkmBattleDrawMsg("You were defeated...", "Press OK or BACK");
      pkmBattlePhase = 3;
      return;
    }

    // Volver al turno del jugador
    pkmBattlePhase = 0;
    pkmBattleDrawMsg("What will you do?", "Choose your move:");
    pkmBattleDrawMoves();
    return;
  }

  // Fase 3: esperando confirmación al final de la batalla
  if (pkmBattlePhase == 3) {
    if (b.ok || b.back) {
      pkmMusicStop();
      if (pkmGameOver) {
        pkmSurvivedSecs = (millis() - pkmStartMs) / 1000;
        pkmSaveBestRank(pkmXPToRank(pkmXP));
        pkmResultWon = false;
        pkmDrawResult(false);
        pkmScreen = PKM_SCREEN_RESULT;
        return;
      }
      if (pkmGameWon) {
        pkmSurvivedSecs = (millis() - pkmStartMs) / 1000;
        pkmSaveBestRank(pkmXPToRank(pkmXP));
        pkmResultWon = true;
        pkmDrawResult(true);
        pkmScreen = PKM_SCREEN_RESULT;
        pkmMusicStop();
        return;
      }
      // Final de batalla normal — volver al overworld
      pkmMusicStart(0);
      pkmScreen = PKM_SCREEN_OVERWORLD;
      pkmUpdateCamera();
      pkmDrawOverworld();
    }
    return;
  }
}

// ==================== TICK DEL OVERWORLD ====================
#define PKM_MOVE_DELAY 160   // ms entre pasos (movimiento fluido)
#define PKM_ENCOUNTER_RATE 5  // 1 de cada N pasos activa un encuentro

static void pkmOverworldTick(const ButtonsEdge& b) {
  uint32_t now = millis();

  if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
    pkmMusicStop();
    while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
    goTo(MODE_GAMES_MENU);
    return;
  }

  if ((int32_t)(now - pkmLastMoveMs) < PKM_MOVE_DELAY) return;

  int8_t dx=0, dy=0;
  bool moved = false;

  if (isPressed(PUSH1PIN)) { dy=-1; pkmFacing=1; moved=true; }
  if (isPressed(PUSH2PIN)) { dy=+1; pkmFacing=0; moved=true; }
  if (isPressed(PUSH4PIN)) { dx=-1; pkmFacing=2; moved=true; }
  if (isPressed(PUSH3PIN)) { dx=+1; pkmFacing=3; moved=true; }

  if (dx && dy) dy=0;

  if (moved) {
    int16_t nx = pkmPX + dx;
    int16_t ny = pkmPY + dy;

    if (nx>=0 && nx<PKM_MAP_W && ny>=0 && ny<PKM_MAP_H && pkmPassable(nx, ny)) {
      pkmErasePlayer();

      pkmPX = nx;
      pkmPY = ny;
      pkmLastMoveMs = now;

      pkmWalkAnim = !pkmWalkAnim;
      pkmWalkAnimMs = now;

      int16_t oldCX = pkmCamX, oldCY = pkmCamY;
      pkmUpdateCamera();

      if (pkmCamX != oldCX || pkmCamY != oldCY) {
        pkmDrawOverworld();
        return;
      }

      int spx = pkmPX*PKM_TDRAW + PKM_TDRAW/2 - pkmCamX;
      int spy = pkmPY*PKM_TDRAW + PKM_TDRAW/2 - pkmCamY;
      pkmDrawPlayer(spx, spy, pkmWalkAnim);
      pkmDrawHUD();

      if (pkmIsTerminal(pkmPX, pkmPY)) {
        pkmHP = pkmMaxHP;
        for(int i=0;i<PKM_MOVES;i++) pkmMovePP[i] = pkmMoves[i].pp;
        pkmScreen = PKM_SCREEN_TERMINAL;
        pkmDrawTerminal();
        return;
      }

      if (pkmIsPortal(pkmPX, pkmPY)) {
        pkmDoPortal();
        return;
      }

      if (pkmIsEncounterTile(pkmPX, pkmPY)) {
        pkmStepCount++;
        uint8_t roll = pkmRand8();
        if (pkmStepCount >= 3 && (roll % PKM_ENCOUNTER_RATE) == 0) {
          pkmStepCount = 0;
          busForTFT();
          tft.fillScreen(0xFFFF);
          if (buzzerEnabled) { buzzerTone(880); delay(60); buzzerTone(1175); delay(60); buzzerNoTone(); }
          delay(80);
          uint8_t enemy = pkmPickEnemy();
          pkmStartBattle(enemy);
          return;
        }
      } else {
        pkmStepCount = 0;
      }
    }
  }
}

// ==================== TICK PRINCIPAL ====================
static void hackerMonTick(const ButtonsEdge& b) {
  pkmMusicTick();
  pkmGoSfxTick();  // se ejecuta cada frame — continúa durante la pantalla de resultados

  if (pkmScreen == PKM_SCREEN_OVERWORLD) {
    pkmOverworldTick(b);
    return;
  }

  if (pkmScreen == PKM_SCREEN_BATTLE) {
    if (pkmBattlePhase == 0 && !pkmBattleEnemyHurt && !pkmBattlePlayerHurt) {
      if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
        if (pkmHP > 5) pkmHP -= 5; else pkmHP = 1;
        pkmMusicStop();
        pkmMusicStart(0);
        while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
        pkmScreen = PKM_SCREEN_OVERWORLD;
        pkmUpdateCamera();
        pkmDrawOverworld();
        return;
      }
      bool navChanged = false;
      if (b.up)   { pkmBattleCursor = (pkmBattleCursor + 3) % 4; navChanged = true; }
      if (b.down) { pkmBattleCursor = (pkmBattleCursor + 1) % 4; navChanged = true; }
      if (b.ok) {
        if (pkmMovePP[pkmBattleCursor] > 0) {
          pkmMovePP[pkmBattleCursor]--;
          pkmApplyMoveToEnemy(pkmBattleCursor);
          pkmBattleDrawMsg(pkmBattleMsg, nullptr);
          pkmBattleDrawMoves();
          pkmBattleDelayMs = millis() + 1300;
          pkmBattlePhase = 1;
        } else {
          // Sin PP — no avanzar el turno, dejar elegir otro ataque
          pkmBattleDrawMsg("No PP! Pick another.", nullptr);
          return;
        }
        pkmBattleDrawEnemyHP();
        return;
      }
      if (navChanged) pkmBattleDrawMoves();
      return;
    }
    pkmBattleTick(b);
    return;
  }

  if (pkmScreen == PKM_SCREEN_TERMINAL) {
    if (b.ok || b.back) {
      pkmScreen = PKM_SCREEN_OVERWORLD;
      pkmDrawOverworld();
    }
    return;
  }

  if (pkmScreen == PKM_SCREEN_TITLE) {
    if (b.ok) {
      pkmInit();
      pkmDrawOverworld();
      return;
    }
    if (b.back) {
      goTo(MODE_GAMES_MENU);
      return;
    }
    return;
  }

  if (pkmScreen == PKM_SCREEN_RESULT) {
    if (b.ok) {
      pkmLoadBestRank();
      pkmScreen = PKM_SCREEN_TITLE;
      pkmDrawTitle();
      return;
    }
    if (b.back) {
      pkmMusicStop();
      goTo(MODE_GAMES_MENU);
      return;
    }
  }
}
