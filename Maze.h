// ======================= MAZE RUNNER (Raycaster Game) =======================
// Self-contained game module. Include once from badge.cpp.
//
// Externals required from badge.cpp:
//   tft, busForTFT(), beep(), buzzerTone(), buzzerNoTone(), buzzerEnabled,
//   isPressed(), PUSH1PIN..PUSH4PIN,
//   uiFrame(), uiMsgCenter(), COLDIM, COLBLACK,
//   goTo(), MODE_MENU, ButtonsEdge

// ---- Music engine -------------------------------------------------------

struct GameNote { uint16_t freq; uint16_t dur; };

// Doom E1M1 "At Doom's Gate" - converted from hibit-dev/buzzer
// Format: {freq_hz, dur_ms}, freq=0 is a rest/pause between notes
// Duration 8 => 125ms note + 38ms pause; Duration 16 => 62ms note + 19ms pause
// Duration 2 => 500ms note + 150ms pause
// Macro Hz values: E2=82 D3=147 C3=131 AS2=117 B2=123
//   E3=165 B3=247 G3=196 A3=220 FS3=185 D3=147
//   A2=110 G3=196 F3=175 DS3=156
//   CS3=139 B3=247 A3=220 GS3=208 F3=175
//   FS3=185 DS3=156 G3=196 E4=330 B4=494 G4=392
//   A3=220 D4=294 C4=262 F4=349
//   E4=330 G4=392 B3=247 D4=294
//   DS4=311
#define N8  125  // eighth note ms
#define P8   38  // eighth pause ms
#define N16  62  // sixteenth note ms
#define P16  19  // sixteenth pause ms
#define N2  500  // half note ms
#define P2  150  // half pause ms

// Helper macros for note+pause pairs
#define E8(f)  {f,N8},{0,P8}
#define E16(f) {f,N16},{0,P16}
#define E2(f)  {f,N2},{0,P2}

static const GameNote gameMelody[] PROGMEM = {
  // Section 1: E2 riff x3 + resolve
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E8(117),E8(82),E8(82),E8(123),E8(131),
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E2(117),

  // Section 2: E2 riff again
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E8(117),E8(82),E8(82),E8(123),E8(131),
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E2(117),

  // Section 3: E2 riff + FS3 run
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E8(117),E8(82),E8(82),E8(123),E8(131),
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E16(185),E16(147),E16(123),E16(220),E16(185),E16(123),E16(147),E16(185),E16(220),E16(185),E16(147),E16(123),

  // Section 4: E2 riff + B3 run
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E8(117),E8(82),E8(82),E8(123),E8(131),
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E16(247),E16(196),E16(165),E16(196),E16(247),E16(330),E16(196),E16(247),E16(330),E16(247),E16(392),E16(494),

  // Section 5: A2 riff
  E8(110),E8(110),E8(220),E8(110),E8(110),E8(196),E8(110),E8(110),
  E8(175),E8(110),E8(110),E8(156),E8(110),E8(110),E8(165),E8(175),
  E8(110),E8(110),E8(220),E8(110),E8(110),E8(196),E8(110),E8(110),
  E8(175),E8(110),E8(110),E2(156),

  // Section 6: A2 riff + A3 run
  E8(110),E8(110),E8(220),E8(110),E8(110),E8(196),E8(110),E8(110),
  E8(175),E8(110),E8(110),E8(156),E8(110),E8(110),E8(165),E8(175),
  E8(110),E8(110),E8(220),E8(110),E8(110),E8(196),E8(110),E8(110),
  E16(220),E16(175),E16(147),E16(220),E16(175),E16(147),E16(262),E16(220),E16(175),E16(220),E16(175),E16(147),

  // Section 7: CS3 variation
  E8(139),E8(139),E8(277),E8(139),E8(139),E8(247),E8(139),E8(139),
  E8(220),E8(139),E8(139),E8(196),E8(139),E8(139),E8(208),E8(220),
  E8(123),E8(123),E8(247),E8(123),E8(123),E8(220),E8(123),E8(123),
  E8(196),E8(123),E8(123),E2(175),

  // Section 8: E2 riff final
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E8(131),E8(82),E8(82),E8(117),E8(82),E8(82),E8(123),E8(131),
  E8(82),E8(82),E8(165),E8(82),E8(82),E8(147),E8(82),E8(82),
  E16(247),E16(196),E16(165),E16(196),E16(247),E16(330),E16(196),E16(247),E16(330),E16(247),E16(392),E16(494),
};

#undef N8
#undef P8
#undef N16
#undef P16
#undef N2
#undef P2
#undef E8
#undef E16
#undef E2
static const int gameMelodyLen = sizeof(gameMelody) / sizeof(gameMelody[0]);

static int      gameMusicIdx    = 0;
static uint32_t gameMusicNextMs = 0;
static bool     gameMusicPlaying = false;

static void gameMusicStart() {
  gameMusicIdx     = 0;
  gameMusicNextMs  = 0;
  gameMusicPlaying = true;
}

static void gameMusicStop() {
  gameMusicPlaying = false;
  buzzerNoTone();
}

// Call every frame - non-blocking, advances melody by wall-clock time
static void gameMusicTick() {
  if (!gameMusicPlaying || !buzzerEnabled) {
    if (gameMusicPlaying) buzzerNoTone();
    return;
  }
  uint32_t now = millis();
  if (now < gameMusicNextMs) return;

  uint16_t freq = pgm_read_word(&gameMelody[gameMusicIdx].freq);
  uint16_t dur  = pgm_read_word(&gameMelody[gameMusicIdx].dur);

  buzzerTone(freq);  // freq=0 → buzzerTone silences automatically

  gameMusicNextMs = now + dur;
  if (++gameMusicIdx >= gameMelodyLen) gameMusicIdx = 0; // loop
}

// ---- Map & constants ----------------------------------------------------

#define GAME_W 240
#define GAME_H 220   // leave 20px for HUD at bottom
#define MAP_W  12
#define MAP_H  12

// Wall types: 1=brick, 2=exit(red), 3=stone, 4=skull(easter egg), 5=circuit, 6=vines
static const uint8_t gameMap[MAP_H][MAP_W] PROGMEM = {
  {1,3,1,3,1,3,1,3,1,3,1,1},
  {3,0,0,0,1,0,0,0,0,0,0,5},
  {1,0,5,0,3,0,1,3,1,0,4,1},
  {3,0,5,0,0,0,0,0,3,0,0,3},
  {1,0,3,1,6,1,0,1,1,3,0,1},
  {3,0,0,0,0,6,0,0,0,0,0,3},
  {1,3,1,0,6,1,3,1,0,1,3,1},
  {3,0,0,0,0,0,0,0,0,0,0,3},
  {1,0,3,1,0,3,3,0,1,3,0,1},
  {3,0,0,1,0,0,0,0,4,2,0,3},
  {1,0,0,0,0,6,0,0,0,0,0,1},
  {1,3,1,3,1,3,1,3,1,3,1,1},
};

// ---- Collectible gems ---------------------------------------------------

#define GAME_MAX_GEMS 5
struct GameGem { float x, y; bool collected; };
static GameGem  gameGems[GAME_MAX_GEMS];
static uint8_t  gameTotalGems = 0;
static uint8_t  gameCollected = 0;
static bool     gameWon       = false;

// ---- Player state -------------------------------------------------------

static float gpX, gpY; // position
static float gdX, gdY; // direction vector
static float gcX, gcY; // camera plane

// ---- Render state -------------------------------------------------------

// 2px-wide column strip; each entry is one RGB565 pixel
static uint16_t gameColBuf[GAME_H * 2];
static bool     gameMoved = true;                    // dirty flag - redraw minimap
static uint16_t gameMinimapBuf[MAP_W * 3 * MAP_H * 3]; // 36x36

// ---- Helpers ------------------------------------------------------------

static uint8_t gameMapAt(int x, int y) {
  if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 1;
  return pgm_read_byte(&gameMap[y][x]);
}

// ---- Procedural wall textures -------------------------------------------

__attribute__((optimize("O3")))
static uint16_t gameWallPixel(uint8_t wallType, bool side, int texX, int texY) {
  switch (wallType) {
    case 1: { // BRICK - mortar grid + two-tone bricks
      uint16_t mortar = 0x4A49;
      int row = texY >> 3;
      if ((texY & 7) == 0) return mortar;
      int bx = texX + ((row & 1) << 4);
      if ((bx & 31) == 0) return mortar;
      return ((texX ^ texY) & 4) ? (side ? 0x9980 : 0xC260) : (side ? 0x6940 : 0x8B00);
    }
    case 2: { // EXIT - red wall with dark archway
      int cx = abs(texX - 32);
      if (cx < 10 && texY > 12) return 0x2000;
      if (cx < 12 && texY > 10) return 0xFBE0;
      if ((texY & 7) == 0) return 0x4000;
      return side ? 0xA800 : 0xF800;
    }
    case 3: { // STONE - block grid with shade variation
      if ((texX & 15) == 0 || (texY & 15) == 0) return 0x39C7;
      uint8_t hash = ((texX >> 4) * 7 + (texY >> 4) * 13) & 0x07;
      return side ? (0x4A69 + hash * 0x0040) : (0x6B6D + hash * 0x0040);
    }
    case 4: { // SKULL easter egg
      int sx = (texX - 16) >> 1;
      int sy = (texY - 8)  >> 1;
      if (sx >= 0 && sx < 16 && sy >= 0 && sy < 15) {
        int cx = abs(sx - 7);
        if (sy < 10 && cx < 7 && cx < (10 - sy)) {
          if (sy >= 3 && sy <= 6 && ((sx >= 3 && sx <= 5) || (sx >= 9 && sx <= 11))) return 0x0000;
          if (sy >= 7 && sy <= 9 && sx >= 6 && sx <= 8) return 0x0000;
          return 0xFFFF;
        }
        if (sy >= 10 && sy < 14 && cx < 5) {
          if (sy >= 12 && (sx & 1) == 0) return 0x0000;
          return side ? 0xBDF7 : 0xFFFF;
        }
      }
      return side ? 0x18C3 : 0x2945;
    }
    case 5: { // CIRCUIT - PCB traces
      uint16_t trace = side ? 0x03E0 : 0x07E0;
      if ((texX & 7) == 0 || (texY & 7) == 0) return trace;
      if ((texX & 15) < 3 && (texY & 15) < 3)  return 0x07FF;
      return side ? 0x0200 : 0x0320;
    }
    case 6: { // VINES - stone with integer zigzag vines
      uint16_t base = side ? 0x528A : 0x6B6D;
      if ((texX % 12) == 0 || (texY % 12) == 0) base = 0x39C7;
      int phase = texY % 20;
      int wave  = (phase < 10) ? (phase - 5) : (15 - phase);
      if (wave < -3) wave = -3;
      if (wave >  3) wave =  3;
      uint16_t vine = side ? 0x0380 : 0x04A0;
      if (abs(texX - 20 - wave) < 2) return vine;
      if (abs(texX - 44 + wave) < 2) return vine;
      if ((texY & 15) < 3 && abs(texX - 20 - wave) < 4) return side ? 0x0400 : 0x07E0;
      return base;
    }
    default:
      return side ? 0x4208 : 0x6B4D;
  }
}

// ---- Raycaster frame ----------------------------------------------------

__attribute__((optimize("O3")))
static void gameRenderFrame() {
  busForTFT();

  // Half-resolution: render 120 columns, each 2px wide = 240px total
  const int HALF_W = GAME_W / 2;

  for (int col = 0; col < HALF_W; col++) {
    int x = col * 2;

    float camX  = 2.0f * col / (float)HALF_W - 1.0f;
    float rayDX = gdX + gcX * camX;
    float rayDY = gdY + gcY * camX;

    int mapX = (int)gpX;
    int mapY = (int)gpY;

    float ddX = (rayDX == 0) ? 1e30f : fabsf(1.0f / rayDX);
    float ddY = (rayDY == 0) ? 1e30f : fabsf(1.0f / rayDY);

    float sdX, sdY;
    int stepX, stepY;

    if (rayDX < 0) { stepX = -1; sdX = (gpX - mapX) * ddX; }
    else           { stepX =  1; sdX = (mapX + 1.0f - gpX) * ddX; }
    if (rayDY < 0) { stepY = -1; sdY = (gpY - mapY) * ddY; }
    else           { stepY =  1; sdY = (mapY + 1.0f - gpY) * ddY; }

    bool    side = false;
    uint8_t wall = 0;
    for (int i = 0; i < 16; i++) {
      if (sdX < sdY) { sdX += ddX; mapX += stepX; side = false; }
      else           { sdY += ddY; mapY += stepY; side = true;  }
      wall = gameMapAt(mapX, mapY);
      if (wall > 0) break;
    }

    float perpDist = side ? (sdY - ddY) : (sdX - ddX);
    if (perpDist < 0.1f) perpDist = 0.1f;

    int lineH     = (int)(GAME_H / perpDist);
    int drawStart = -lineH / 2 + GAME_H / 2;
    int drawEnd   =  lineH / 2 + GAME_H / 2;
    if (drawStart < 0)        drawStart = 0;
    if (drawEnd >= GAME_H)    drawEnd   = GAME_H - 1;

    float wallX;
    if (!side) wallX = gpY + perpDist * rayDY;
    else       wallX = gpX + perpDist * rayDX;
    wallX -= floorf(wallX);
    int texX = (int)(wallX * 64.0f);
    if (texX < 0)  texX = 0;
    if (texX > 63) texX = 63;

    // Ceiling (black) - 32-bit writes
    {
      uint32_t *p = (uint32_t *)gameColBuf;
      for (int y = 0; y < drawStart; y++) p[y] = 0x00000000u;
    }

    // Wall - textured, duplicated to 2nd column
    int wallH = drawEnd - drawStart + 1;
    for (int y = drawStart; y <= drawEnd; y++) {
      int texY = ((y - drawStart) << 6) / wallH;
      if (texY > 63) texY = 63;
      uint16_t c = gameWallPixel(wall, side, texX, texY);
      gameColBuf[y * 2]     = c;
      gameColBuf[y * 2 + 1] = c;
    }

    // Floor - 32-bit writes
    {
      uint32_t  floorPair = 0x18C318C3u;
      uint32_t *p = (uint32_t *)gameColBuf + drawEnd + 1;
      int pairs = GAME_H - (drawEnd + 1);
      for (int y = 0; y < pairs; y++) p[y] = floorPair;
    }

    tft.drawRGBBitmap(x, 0, gameColBuf, 2, GAME_H);
  }

  // Gem sprites (projected circles)
  float invDet = 1.0f / (gcX * gdY - gdX * gcY);
  for (uint8_t i = 0; i < gameTotalGems; i++) {
    if (gameGems[i].collected) continue;
    float sx = gameGems[i].x - gpX;
    float sy = gameGems[i].y - gpY;
    float tx = invDet * (gdY * sx - gdX * sy);
    float ty = invDet * (-gcY * sx + gcX * sy);
    if (ty <= 0.2f) continue;
    int sprScreenX = (int)((GAME_W / 2) * (1.0f + tx / ty));
    int sprH = (int)(GAME_H / ty);
    if (sprH > 60) sprH = 60;
    if (sprH < 3)  continue;
    int cx = sprScreenX;
    int cy = GAME_H / 2;
    int r  = sprH / 4;
    if (r < 2)  r = 2;
    if (r > 10) r = 10;
    if (cx >= 2 && cx < GAME_W - 2 && cy >= 2 && cy < GAME_H - 2) {
      tft.fillCircle(cx, cy, r, 0xFFE0);
      tft.drawCircle(cx, cy, r, 0xFDA0);
    }
  }
}

// ---- HUD & minimap ------------------------------------------------------

static bool gameNearWallType(uint8_t wtype) {
  int px = (int)gpX, py = (int)gpY;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
      if (gameMapAt(px + dx, py + dy) == wtype) return true;
  return false;
}

static void gameDrawHUD() {
  busForTFT();
  tft.fillRect(0, GAME_H, 240, 20, COLBLACK);
  tft.setTextSize(1);
  tft.setTextColor(0x07E0);
  tft.setCursor(4, GAME_H + 6);
  tft.print("GEMS:");
  tft.print(gameCollected);
  tft.print("/");
  tft.print(gameTotalGems);

  if (gameNearWallType(4)) {
    tft.setTextColor(0xFFFF);
    tft.setCursor(70, GAME_H + 6);
    tft.print("* MEMENTO MORI *");
  } else if (gameNearWallType(5)) {
    tft.setTextColor(0x07FF);
    tft.setCursor(70, GAME_H + 6);
    tft.print("HACK THE PLANET");
  } else if (gameCollected >= gameTotalGems) {
    tft.setTextColor(0xF800);
    tft.setCursor(70, GAME_H + 6);
    tft.print(">> FIND THE EXIT!");
  } else {
    tft.setCursor(140, GAME_H + 6);
    tft.setTextColor(COLDIM);
    tft.print("UP/DN OK/BK");
  }

  // Minimap - only redraw on movement (36x36 sent in one SPI transaction)
  if (gameMoved) {
    const int mmX = GAME_W - 40, mmY = 4;
    const int mmS = 3;
    int playerX = (int)gpX, playerY = (int)gpY;

    uint16_t *mp = gameMinimapBuf;
    for (int my = 0; my < MAP_H; my++) {
      for (int py = 0; py < mmS; py++) {
        for (int mx = 0; mx < MAP_W; mx++) {
          uint8_t  w  = gameMapAt(mx, my);
          uint16_t mc;
          if      (mx == playerX && my == playerY) mc = 0xFFFF;
          else if (w == 0)                         mc = 0x0841;
          else if (w == 2)                         mc = 0xF800;
          else if (w == 4)                         mc = 0xFFE0;
          else                                     mc = 0x2945;
          mp[0] = mc; mp[1] = mc; mp[2] = mc;
          mp += mmS;
        }
      }
    }
    tft.drawRGBBitmap(mmX, mmY, gameMinimapBuf, MAP_W * mmS, MAP_H * mmS);
    tft.drawRect(mmX - 1, mmY - 1, MAP_W * mmS + 2, MAP_H * mmS + 2, 0x2945);
  }
}

// ---- Pause menu ---------------------------------------------------------

static bool    gamePaused     = false;
static uint8_t gamePauseIdx   = 0;  // 0=Resume, 1=Exit

static void gameDrawPauseMenu() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiTitle("PAUSED");

  const char* items[] = {"Resume", "Exit"};
  for (uint8_t i = 0; i < 2; i++) {
    bool sel = (i == gamePauseIdx);
    int y = 90 + i * 50;
    if (sel) tft.fillRoundRect(20, y - 4, 200, 36, 6, COLDARK);
    tft.drawRoundRect(20, y - 4, 200, 36, 6, sel ? COLNEON : COLDIM);
    tft.setTextSize(2);
    tft.setTextColor(sel ? COLNEON : COLDIM);
    tft.setCursor(36, y + 6);
    tft.print(items[i]);
  }

  tft.setTextSize(1);
  tft.setTextColor(COLDIM);
  tft.setCursor(10, 210);
  tft.print("UP/DOWN sel   OK confirm   BACK resume");
}

// ---- Win screen ---------------------------------------------------------

static void gameDrawWinScreen() {
  busForTFT();
  tft.fillScreen(COLBLACK);
  uiFrame();
  uiMsgCenter("YOU ESCAPED!", 80, 0x07E0, 2);
  uiMsgCenter("ALL GEMS FOUND", 110, 0xFFE0, 2);
  uiMsgCenter("BACK to exit", 180, COLDIM, 1);
}

// ---- Init & entry point -------------------------------------------------

static void gameInit() {
  gpX = 1.5f; gpY = 1.5f;
  gdX = 1.0f; gdY = 0.0f;
  gcX = 0.0f; gcY = 0.66f;
  gamePaused   = false;
  gamePauseIdx = 0;
  gameWon       = false;
  gameCollected = 0;

  const float gemPos[][2] = {
    {3.5f, 3.5f}, {5.5f, 7.5f}, {9.5f, 1.5f}, {1.5f, 9.5f}, {9.5f, 9.5f}
  };
  gameTotalGems = GAME_MAX_GEMS;
  for (uint8_t i = 0; i < gameTotalGems; i++) {
    gameGems[i].x         = gemPos[i][0];
    gameGems[i].y         = gemPos[i][1];
    gameGems[i].collected = false;
  }
  gameMusicStart();
}

static void drawGame() {
  gameInit();
  gameRenderFrame();
  gameDrawHUD();
}

// ---- Per-frame tick (input + physics + redraw) --------------------------

static void gameTick(ButtonsEdge &b) {
  if (gameWon) {
    if (b.back || b.ok) goTo(MODE_MENU);
    return;
  }

  // ---- Pause menu --------------------------------------------------------
  if (gamePaused) {
    if (b.up)   { gamePauseIdx = 0; gameDrawPauseMenu(); }
    if (b.down) { gamePauseIdx = 1; gameDrawPauseMenu(); }
    if (b.back) { gamePaused = false; gameRenderFrame(); gameDrawHUD(); }
    if (b.ok) {
      if (gamePauseIdx == 0) { gamePaused = false; gameRenderFrame(); gameDrawHUD(); }
      else                   { goTo(MODE_MENU); return; }
    }
    return;
  }

  // ---- Combo: DOWN + OK → open pause menu --------------------------------
  if (isPressed(PUSH2PIN) && isPressed(PUSH4PIN)) {
    gamePaused   = true;
    gamePauseIdx = 0;
    gameDrawPauseMenu();
    // Wait for both buttons to be released before continuing
    while (isPressed(PUSH2PIN) || isPressed(PUSH4PIN)) delay(10);
    return;
  }

  // ---- Combo: UP + BACK → interact ---------------------------------------
  if (isPressed(PUSH1PIN) && isPressed(PUSH3PIN)) {
    // Placeholder: flash "INTERACT" hint — future use for doors/NPCs
    busForTFT();
    tft.setTextSize(1);
    tft.setTextColor(COLNEON);
    tft.setCursor(80, 228);
    tft.print("INTERACT");
    beep(8);
    uint32_t t0 = millis();
    while (millis() - t0 < 400 && (isPressed(PUSH1PIN) || isPressed(PUSH3PIN))) delay(10);
    // Redraw HUD to clear the hint
    gameDrawHUD();
    return;
  }

  bool moved = false;
  const float moveSpd = 0.12f;
  static const float rotC = 0.9968f; // cos(0.08 rad)
  static const float rotS = 0.0799f; // sin(0.08 rad)

  if (isPressed(PUSH1PIN)) { // UP = forward
    float nx = gpX + gdX * moveSpd;
    float ny = gpY + gdY * moveSpd;
    if (gameMapAt((int)nx, (int)gpY) == 0) gpX = nx;
    if (gameMapAt((int)gpX, (int)ny) == 0) gpY = ny;
    moved = true;
  }
  if (isPressed(PUSH2PIN)) { // DOWN = backward
    float nx = gpX - gdX * moveSpd;
    float ny = gpY - gdY * moveSpd;
    if (gameMapAt((int)nx, (int)gpY) == 0) gpX = nx;
    if (gameMapAt((int)gpX, (int)ny) == 0) gpY = ny;
    moved = true;
  }
  if (isPressed(PUSH4PIN)) { // OK = turn right
    float od = gdX, oc = gcX;
    gdX = gdX * rotC + gdY * rotS;  gdY = -od * rotS + gdY * rotC;
    gcX = gcX * rotC + gcY * rotS;  gcY = -oc * rotS + gcY * rotC;
    moved = true;
  }
  if (isPressed(PUSH3PIN)) { // BACK = turn left
    float od = gdX, oc = gcX;
    gdX = gdX * rotC - gdY * rotS;  gdY = od  * rotS + gdY * rotC;
    gcX = gcX * rotC - gcY * rotS;  gcY = oc  * rotS + gcY * rotC;
    moved = true;
  }

  gameMoved = moved;

  // Gem collection
  for (uint8_t i = 0; i < gameTotalGems; i++) {
    if (gameGems[i].collected) continue;
    float dx = gpX - gameGems[i].x;
    float dy = gpY - gameGems[i].y;
    if (dx * dx + dy * dy < 0.25f) {
      gameGems[i].collected = true;
      gameCollected++;
      beep(15);
    }
  }

  // Exit detection (wall type 2, all gems collected)
  if (gameCollected >= gameTotalGems) {
    int mx = (int)gpX, my = (int)gpY;
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (gameMapAt(mx + dx, my + dy) == 2) {
          float dist = fabsf(gpX - (mx + dx + 0.5f)) + fabsf(gpY - (my + dy + 0.5f));
          if (dist < 1.2f) {
            gameWon = true;
            beep(80);
            gameDrawWinScreen();
            return;
          }
        }
      }
    }
  }

  if (moved) {
    gameRenderFrame();
    gameDrawHUD();
  }
}
