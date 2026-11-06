// Doom Maze raycaster. Same map, gems, procedural wall textures and movement as the badge. Each
// wall texel keeps its original RGB565 value and is ordered-dithered to 1-bit at draw time.
#include "../features.h"
#include "../sound.h"
#include "../ui.h"

#include <math.h>

// ---- Music ------------------------------------------------------------------
#define Q  400
#define E  200
#define S  100
#define DQ 600
#define H  800
static const Note MAZE_MELODY[] = {
    // Section A
    {330, E}, {494, E}, {659, Q}, {587, E}, {494, E},
    {392, E}, {587, E}, {784, Q}, {740, E}, {587, E},
    {659, E}, {587, E}, {494, E}, {392, E}, {440, E}, {494, E}, {523, E}, {587, E},
    {659, H}, {0, Q},
    // Section B
    {659, S}, {587, S}, {523, S}, {494, S}, {440, S}, {392, S}, {370, S}, {330, S},
    {392, E}, {440, E}, {494, E}, {523, E}, {587, E}, {659, DQ}, {0, E},
    {740, E}, {659, E}, {587, E}, {523, E}, {494, DQ}, {440, E},
    {392, H}, {0, Q},
    // Section C
    {494, S}, {659, S}, {784, S}, {988, S}, {880, E}, {784, E}, {740, E}, {659, E},
    {587, E}, {659, E}, {740, E}, {784, DQ}, {740, E},
    {659, E}, {587, E}, {523, E}, {494, E}, {440, E}, {392, E}, {370, E}, {330, E},
    {659, Q}, {494, Q}, {330, Q}, {0, Q},
};
#undef Q
#undef E
#undef S
#undef DQ
#undef H

// ---- Map --------------------------------------------------------------------
#define MAP_W  12
#define MAP_H  12
#define VIEW_W SCREEN_W
#define VIEW_H 56
#define TICK_MS 50

// 1=brick 2=exit 3=stone 4=skull (easter egg) 5=circuit 6=vines
static const uint8_t MAZE_MAP[MAP_H][MAP_W] = {
    {1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 1},
    {3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 5},
    {1, 0, 5, 0, 3, 0, 1, 3, 1, 0, 4, 1},
    {3, 0, 5, 0, 0, 0, 0, 0, 3, 0, 0, 3},
    {1, 0, 3, 1, 6, 1, 0, 1, 1, 3, 0, 1},
    {3, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 3},
    {1, 3, 1, 0, 6, 1, 3, 1, 0, 1, 3, 1},
    {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {1, 0, 3, 1, 0, 3, 3, 0, 1, 3, 0, 1},
    {3, 0, 0, 1, 0, 0, 0, 0, 4, 2, 0, 3},
    {1, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 1},
    {1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 1},
};

#define MAX_GEMS 5

typedef struct {
    float x, y;
    bool collected;
} Gem;

static struct {
    Gem gems[MAX_GEMS];
    uint8_t collected;
    bool won, paused, show_map;
    uint8_t pause_idx;
    float px, py, dx, dy, cx, cy;
    uint8_t rot_frames;
    uint32_t next_tick;
} m;

static float zbuf[VIEW_W];

static uint8_t map_at(int x, int y) {
    if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 1;
    return MAZE_MAP[y][x];
}

// ---- Procedural wall textures (same formulas as the badge) ----------------------
static uint16_t wall_pixel(uint8_t type, bool side, int tx, int ty) {
    switch(type) {
    case 1: {
        const uint16_t mortar = 0x4A49;
        const int row = ty >> 3;
        if((ty & 7) == 0) return mortar;
        const int bx = tx + ((row & 1) << 4);
        if((bx & 31) == 0) return mortar;
        return ((tx ^ ty) & 4) ? (side ? 0x9980 : 0xC260) : (side ? 0x6940 : 0x8B00);
    }
    case 2: {
        const int cx = abs(tx - 32);
        if(cx < 10 && ty > 12) return 0x2000;
        if(cx < 12 && ty > 10) return 0xFBE0;
        if((ty & 7) == 0) return 0x4000;
        return side ? 0xA800 : 0xF800;
    }
    case 3: {
        if((tx & 15) == 0 || (ty & 15) == 0) return 0x39C7;
        const uint8_t hash = ((tx >> 4) * 7 + (ty >> 4) * 13) & 0x07;
        return side ? (0x4A69 + hash * 0x0040) : (0x6B6D + hash * 0x0040);
    }
    case 4: {
        const int sx = (tx - 16) >> 1;
        const int sy = (ty - 8) >> 1;
        if(sx >= 0 && sx < 16 && sy >= 0 && sy < 15) {
            const int cx = abs(sx - 7);
            if(sy < 10 && cx < 7 && cx < (10 - sy)) {
                if(sy >= 3 && sy <= 6 && ((sx >= 3 && sx <= 5) || (sx >= 9 && sx <= 11))) return 0x0000;
                if(sy >= 7 && sy <= 9 && sx >= 6 && sx <= 8) return 0x0000;
                return 0xFFFF;
            }
            if(sy >= 10 && sy < 14 && cx < 5) {
                if(sy >= 12 && (sx & 1) == 0) return 0x0000;
                return side ? 0xBDF7 : 0xFFFF;
            }
        }
        return side ? 0x18C3 : 0x2945;
    }
    case 5: {
        const uint16_t trace = side ? 0x03E0 : 0x07E0;
        if((tx & 7) == 0 || (ty & 7) == 0) return trace;
        if((tx & 15) < 3 && (ty & 15) < 3) return 0x07FF;
        return side ? 0x0200 : 0x0320;
    }
    case 6: {
        uint16_t base = side ? 0x528A : 0x6B6D;
        if((tx % 12) == 0 || (ty % 12) == 0) base = 0x39C7;
        const int phase = ty % 20;
        int wave = (phase < 10) ? (phase - 5) : (15 - phase);
        if(wave < -3) wave = -3;
        if(wave > 3) wave = 3;
        const uint16_t vine = side ? 0x0380 : 0x04A0;
        if(abs(tx - 20 - wave) < 2) return vine;
        if(abs(tx - 44 + wave) < 2) return vine;
        if((ty & 15) < 3 && abs(tx - 20 - wave) < 4) return side ? 0x0400 : 0x07E0;
        return base;
    }
    default:
        return side ? 0x4208 : 0x6B4D;
    }
}

// Brightest channel plus a contrast stretch, not perceptual luminance: the badge separates bricks
// and mortar mostly by hue, and that contrast would vanish in plain grayscale.
static inline uint8_t luminance(uint16_t c) {
    const uint16_t r = (c >> 11) << 3;
    const uint16_t g = ((c >> 5) & 0x3F) << 2;
    const uint16_t b = (c & 0x1F) << 3;
    uint16_t v = r > g ? r : g;
    if(b > v) v = b;
    int stretched = ((int)v - 60) * 8 / 5;
    if(stretched < 0) stretched = 0;
    if(stretched > 255) stretched = 255;
    return (uint8_t)stretched;
}

static const uint8_t BAYER4[4][4] = {
    {8, 136, 40, 168},
    {200, 72, 232, 104},
    {56, 184, 24, 152},
    {248, 120, 216, 88},
};

// ---- Logic ------------------------------------------------------------------
static void maze_init(void) {
    m.px = 1.5f;
    m.py = 1.5f;
    m.dx = 1.0f;
    m.dy = 0.0f;
    m.cx = 0.0f;
    m.cy = 0.66f;
    m.paused = false;
    m.pause_idx = 0;
    m.won = false;
    m.collected = 0;
    m.rot_frames = 0;
    m.show_map = true;
    static const float GEM_POS[MAX_GEMS][2] = {
        {1.5f, 3.5f}, {10.5f, 1.5f}, {6.5f, 5.5f}, {1.5f, 9.5f}, {10.5f, 9.5f}};
    for(uint8_t i = 0; i < MAX_GEMS; i++) {
        m.gems[i].x = GEM_POS[i][0];
        m.gems[i].y = GEM_POS[i][1];
        m.gems[i].collected = false;
    }
    m.next_tick = millis();
}

static void music_start(void) {
    sound_music_start(MAZE_MELODY, COUNT_OF(MAZE_MELODY), true);
}

void maze_enter(bool resume) {
    if(!resume) maze_init();
    if(!m.won) music_start();
    m.next_tick = millis();
}

void maze_exit(void) {
    sound_stop_all();
}

static void try_move(float amount) {
    const float nx = m.px + m.dx * amount;
    const float ny = m.py + m.dy * amount;
    if(map_at((int)nx, (int)m.py) == 0) m.px = nx;
    if(map_at((int)m.px, (int)ny) == 0) m.py = ny;
}

static void rotate(float s) {
    const float rc = 0.9968f; // cos(0.08)
    const float od = m.dx;
    const float oc = m.cx;
    m.dx = m.dx * rc - m.dy * s;
    m.dy = od * s + m.dy * rc;
    m.cx = m.cx * rc - m.cy * s;
    m.cy = oc * s + m.cy * rc;
}

void maze_tick(const ButtonsEdge* b) {
    if(m.won) {
        if(b->back || b->ok) go_to(MODE_GAMES_MENU, false);
        return;
    }

    if(m.paused) {
        if(b->up) m.pause_idx = 0;
        if(b->down) m.pause_idx = 1;
        if(b->back) {
            m.paused = false;
        } else if(b->ok) {
            if(m.pause_idx == 0) {
                m.paused = false;
            } else {
                go_to(MODE_GAMES_MENU, false);
            }
        }
        return;
    }

    if(b->back) {
        m.paused = true;
        m.pause_idx = 0;
        return;
    }
    if(b->ok) m.show_map = !m.show_map;

    const uint32_t now = millis();
    if((int32_t)(now - m.next_tick) < 0) return;
    m.next_tick = now + TICK_MS;

    const float rot_s = 0.0799f; // sin(0.08)
    bool moved = false;
    if(is_pressed(InputKeyUp)) {
        try_move(0.12f);
        moved = true;
    }
    if(is_pressed(InputKeyDown)) {
        try_move(-0.12f);
        moved = true;
    }
    if(is_pressed(InputKeyLeft)) {
        rotate(-rot_s);
        moved = true;
    }
    if(is_pressed(InputKeyRight)) {
        rotate(rot_s);
        moved = true;
    }

    // Keep direction and camera plane from drifting.
    if(moved && ++m.rot_frames >= 60) {
        m.rot_frames = 0;
        const float mag = sqrtf(m.dx * m.dx + m.dy * m.dy);
        if(mag > 0.0f) {
            m.dx /= mag;
            m.dy /= mag;
            m.cx /= mag;
            m.cy /= mag;
        }
    }

    for(uint8_t i = 0; i < MAX_GEMS; i++) {
        if(m.gems[i].collected) continue;
        const float gx = m.px - m.gems[i].x;
        const float gy = m.py - m.gems[i].y;
        if(gx * gx + gy * gy < 0.25f) {
            m.gems[i].collected = true;
            m.collected++;
            sound_beep(15);
        }
    }

    if(m.collected >= MAX_GEMS) {
        const int mx = (int)m.px;
        const int my = (int)m.py;
        for(int oy = -1; oy <= 1; oy++) {
            for(int ox = -1; ox <= 1; ox++) {
                if(map_at(mx + ox, my + oy) != 2) continue;
                const float ex = m.px - (mx + ox + 0.5f);
                const float ey = m.py - (my + oy + 0.5f);
                if(ex * ex + ey * ey < 1.44f) {
                    m.won = true;
                    sound_music_stop();
                    sound_beep(80);
                    return;
                }
            }
        }
    }
}

// ---- Rendering --------------------------------------------------------------
static bool near_wall_type(uint8_t type) {
    const int px = (int)m.px;
    const int py = (int)m.py;
    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            if(map_at(px + dx, py + dy) == type) return true;
        }
    }
    return false;
}

// Draws with the current color, which always means "lit" here; see maze_draw().
static void render_column(Canvas* c, int col) {
    const float cam = 2.0f * col / (float)VIEW_W - 1.0f;
    const float ray_x = m.dx + m.cx * cam;
    const float ray_y = m.dy + m.cy * cam;

    int map_x = (int)m.px;
    int map_y = (int)m.py;

    const float dd_x = (ray_x == 0.0f) ? 1e30f : fabsf(1.0f / ray_x);
    const float dd_y = (ray_y == 0.0f) ? 1e30f : fabsf(1.0f / ray_y);

    float sd_x, sd_y;
    int step_x, step_y;
    if(ray_x < 0) {
        step_x = -1;
        sd_x = (m.px - map_x) * dd_x;
    } else {
        step_x = 1;
        sd_x = (map_x + 1.0f - m.px) * dd_x;
    }
    if(ray_y < 0) {
        step_y = -1;
        sd_y = (m.py - map_y) * dd_y;
    } else {
        step_y = 1;
        sd_y = (map_y + 1.0f - m.py) * dd_y;
    }

    bool side = false;
    uint8_t wall = 0;
    for(int i = 0; i < 16; i++) {
        if(sd_x < sd_y) {
            sd_x += dd_x;
            map_x += step_x;
            side = false;
        } else {
            sd_y += dd_y;
            map_y += step_y;
            side = true;
        }
        wall = map_at(map_x, map_y);
        if(wall > 0) break;
    }

    float dist = side ? (sd_y - dd_y) : (sd_x - dd_x);
    if(dist < 0.1f) dist = 0.1f;
    zbuf[col] = dist;

    const int line_h = (int)(VIEW_H / dist);
    int start = -line_h / 2 + VIEW_H / 2;
    int end = line_h / 2 + VIEW_H / 2;
    const int full_start = start;
    if(start < 0) start = 0;
    if(end >= VIEW_H) end = VIEW_H - 1;

    float wall_x = side ? (m.px + dist * ray_x) : (m.py + dist * ray_y);
    wall_x -= floorf(wall_x);
    int tex_x = (int)(wall_x * 64.0f);
    if(tex_x < 0) tex_x = 0;
    if(tex_x > 63) tex_x = 63;

    // Distance fog does the depth cue that color can't.
    int shade = (int)(280.0f / (1.0f + dist * 0.22f));
    if(shade > 256) shade = 256;

    for(int y = start; y <= end; y++) {
        int tex_y = line_h > 0 ? ((y - full_start) << 6) / line_h : 0;
        if(tex_y < 0) tex_y = 0;
        if(tex_y > 63) tex_y = 63;
        const uint8_t lum = (uint8_t)((luminance(wall_pixel(wall, side, tex_x, tex_y)) * shade) >> 8);
        if(lum > BAYER4[y & 3][col & 3]) canvas_draw_dot(c, col, y);
    }

    // Sparse floor so the corridor reads as a corridor.
    for(int y = end + 1; y < VIEW_H; y++) {
        if(((col + y * 3) & 7) == 0) canvas_draw_dot(c, col, y);
    }
}

static void render_gems(Canvas* c) {
    const float inv_det = 1.0f / (m.cx * m.dy - m.dx * m.cy);
    for(uint8_t i = 0; i < MAX_GEMS; i++) {
        if(m.gems[i].collected) continue;
        const float sx = m.gems[i].x - m.px;
        const float sy = m.gems[i].y - m.py;
        const float tx = inv_det * (m.dy * sx - m.dx * sy);
        const float ty = inv_det * (-m.cy * sx + m.cx * sy);
        if(ty <= 0.2f) continue;
        const int screen_x = (int)((VIEW_W / 2) * (1.0f + tx / ty));
        int spr_h = (int)(VIEW_H / ty);
        if(spr_h > 30) spr_h = 30;
        if(spr_h < 2) continue;
        int r = spr_h / 4;
        if(r < 1) r = 1;
        if(r > 6) r = 6;
        if(screen_x - r < 0 || screen_x + r >= VIEW_W) continue;
        if(ty >= zbuf[screen_x] || ty >= zbuf[screen_x - r] || ty >= zbuf[screen_x + r]) continue;
        canvas_draw_disc(c, screen_x, VIEW_H / 2, r);
    }
}

static void render_minimap(Canvas* c, Color lit, Color unlit) {
    const int ox = VIEW_W - MAP_W * 2 - 1;
    const int oy = 1;
    canvas_set_color(c, unlit);
    canvas_draw_box(c, ox - 1, oy - 1, MAP_W * 2 + 2, MAP_H * 2 + 2);
    canvas_set_color(c, lit);
    canvas_draw_frame(c, ox - 1, oy - 1, MAP_W * 2 + 2, MAP_H * 2 + 2);
    const bool blink = (millis() >> 8) & 1;
    for(int y = 0; y < MAP_H; y++) {
        for(int x = 0; x < MAP_W; x++) {
            const uint8_t w = map_at(x, y);
            if(w == 0) continue;
            if(w == 2 && !blink) continue;
            canvas_draw_box(c, ox + x * 2, oy + y * 2, 2, 2);
        }
    }
    canvas_draw_box(c, ox + (int)m.px * 2, oy + (int)m.py * 2, 2, 2);
}

void maze_draw(Canvas* c) {
    if(m.won) {
        ui_center(c, 22, FontPrimary, "YOU ESCAPED!");
        ui_center(c, 36, FontPrimary, "ALL GEMS FOUND");
        ui_tiny_center(c, SCREEN_W / 2, 52, "BACK TO EXIT");
        return;
    }

    if(m.paused) {
        ui_title(c, "PAUSED");
        ui_list_row(c, 0, "Resume", NULL, m.pause_idx == 0);
        ui_list_row(c, 1, "Exit", NULL, m.pause_idx == 1);
        ui_footer(c, "OK CONFIRM  BACK RESUME");
        return;
    }

    // The final XOR in dark themes turns set pixels bright. In the light theme walls must be
    // drawn the other way around, or the corridor would look like a photo negative.
    const bool dark = ui_is_dark();
    const Color lit = dark ? ColorBlack : ColorWhite;
    const Color unlit = dark ? ColorWhite : ColorBlack;
    if(!dark) canvas_draw_box(c, 0, 0, VIEW_W, VIEW_H);
    canvas_set_color(c, lit);
    for(int col = 0; col < VIEW_W; col++) render_column(c, col);
    render_gems(c);
    if(m.show_map) render_minimap(c, lit, unlit);
    canvas_set_color(c, ColorBlack);

    char hud[12];
    snprintf(hud, sizeof(hud), "GEMS:%u/%u", m.collected, MAX_GEMS);
    ui_tiny(c, 1, 58, hud);

    const char* msg = "D-PAD MOVE";
    if(near_wall_type(4)) {
        msg = "* MEMENTO MORI *";
    } else if(near_wall_type(5)) {
        msg = "HACK THE PLANET";
    } else if(m.collected >= MAX_GEMS) {
        msg = ">> FIND THE EXIT!";
    }
    ui_tiny_right(c, SCREEN_W - 1, 58, msg);
}
