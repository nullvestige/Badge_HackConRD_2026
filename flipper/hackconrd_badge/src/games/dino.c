// Dino Run. Physics, spawning and collisions run in the badge's original 240 px world coordinates;
// only the drawing is projected to the 128x64 screen.
#include "../features.h"
#include "../prefs.h"
#include "../sound.h"
#include "../ui.h"

// ---- Music ------------------------------------------------------------------
#define Q  375
#define QD 563
#define E  188
#define H  750

static const Note DINO_MELODY[] = {
    // Phrase A
    {659, Q},  {784, Q},  {880, Q}, {784, Q},  {659, Q},  {523, Q}, {440, H},
    {523, Q},  {587, Q},  {659, Q}, {784, Q},  {880, QD}, {784, E}, {659, H},
    // Phrase B
    {784, Q},  {880, Q},  {988, Q}, {880, Q},  {784, Q},  {659, Q}, {523, H},
    {587, Q},  {659, Q},  {784, Q}, {880, Q},  {988, QD}, {880, E}, {784, H},
    // Bridge
    {1047, Q}, {880, E},  {784, E}, {659, Q},  {523, Q},  {440, Q}, {494, Q},
    {523, Q},  {587, Q},  {659, QD}, {587, E}, {523, Q},  {440, Q}, {494, Q},
    {440, E},  {0, E},    {0, H},
    // Phrase A (repeat)
    {659, Q},  {784, Q},  {880, Q}, {784, Q},  {659, Q},  {523, Q}, {440, H},
    {523, Q},  {587, Q},  {659, Q}, {784, Q},  {880, QD}, {784, E}, {659, H},
    {0, E},
};
#undef Q
#undef QD
#undef E
#undef H

static const Note SFX_JUMP[] = {{880, 40}, {1047, 30}};
static const Note SFX_GAME_OVER[] = {
    {180, 80}, {120, 80}, {80, 100}, {880, 65}, {784, 65}, {698, 65},
    {659, 65}, {587, 65}, {523, 65}, {494, 65}, {392, 65},
};

// ---- Constants --------------------------------------------------------------
#define TICK_MS        33
#define WORLD_W        240
#define GROUND_Y       180
#define DINO_X         28
#define DINO_W         18
#define DINO_H_RUN     28
#define DINO_H_DUCK    14
#define JUMP_VY        -14
#define GRAVITY        1
#define MAX_SPEED      12
#define OBS_MAX        3
#define CLOUD_COUNT    3

#define OBS_CACTUS_S 0
#define OBS_CACTUS_L 1
#define OBS_BIRD_HI  2
#define OBS_BIRD_LO  3
#define OBS_ROCK     4
#define OBS_PAIR_SS  5
#define OBS_PAIR_SL  6
#define OBS_PAIR_LS  7
#define OBS_PAIR_LL  8
#define OBS_TRIO_SSS 9
#define OBS_TRIO_SLS 10
#define OBS_TRIO_LSL 11

// World to screen. Ground sits at y=56, the top of a full jump at y=7.
#define SCREEN_GROUND 56
static inline int sx(int wx) {
    return (wx * SCREEN_W) / WORLD_W;
}
static inline int sy(int wy) {
    return SCREEN_GROUND - ((GROUND_Y - wy) * 49) / 141;
}

typedef struct {
    int16_t x;
    uint8_t kind;
} Obstacle;

static struct {
    int16_t dino_y;
    int8_t dino_vy;
    bool ducking;
    uint8_t leg_phase, leg_timer;
    Obstacle obs[OBS_MAX];
    uint32_t score;
    uint8_t speed;
    uint32_t last_speed_up;
    uint8_t lives;
    uint16_t spawn_timer;
    uint32_t frame;
    int16_t cloud_x[CLOUD_COUNT];
    uint32_t hit_until;
    bool game_over, title;
    uint32_t next_tick;
    bool jump_pending;
    uint16_t scroll;
} d;

// ---- Sprites ----------------------------------------------------------------
static const char* const SPR_RUN_A[] = {
    "....######", "....#.####", "....######", "....###...", "#..#####..", "##.#####..",
    ".#######..", "..#####...", "...###....", "...#..#...", "...#......",
};
static const char* const SPR_RUN_B[] = {
    "....######", "....#.####", "....######", "....###...", "#..#####..", "##.#####..",
    ".#######..", "..#####...", "...###....", "...#..#...", "......#...",
};
static const char* const SPR_DUCK[] = {
    ".......######", ".......#.####", "#.###########", "##########...", ".########....", "..#.....#....",
};
static const char* const SPR_CACTUS_S[] = {
    "..##...", "#.##...", "#.##.#.", "####.#.", "..####.", "..##...", "..##...",
};
static const char* const SPR_CACTUS_L[] = {
    "..##...", "..##...", "#.##...", "#.##.#.", "#.##.#.", "####.#.",
    "..####.", "..##...", "..##...", "..##...", "..##...", "..##...",
};
static const char* const SPR_ROCK[] = {
    "....####.......", "..########.###.", ".#############.", "###############", "###############",
};
static const char* const SPR_BIRD_UP[] = {
    "..###.....", "...###....", ".#########", "#########.", "..........",
};
static const char* const SPR_BIRD_DOWN[] = {
    "..........", ".#########", "#########.", "...###....", "..###.....",
};

// ---- Logic ------------------------------------------------------------------
static void init_game(void) {
    memset(&d.obs, 0, sizeof(d.obs));
    d.dino_y = GROUND_Y - DINO_H_RUN;
    d.dino_vy = 0;
    d.ducking = false;
    d.leg_phase = 0;
    d.leg_timer = 8;
    d.speed = 3;
    d.score = 0;
    d.last_speed_up = 0;
    d.lives = 1;
    d.spawn_timer = 60;
    d.frame = 0;
    d.hit_until = 0;
    d.game_over = false;
    d.title = false;
    d.jump_pending = false;
    d.scroll = 0;
    for(uint8_t i = 0; i < OBS_MAX; i++) d.obs[i].x = -1;
    for(uint8_t i = 0; i < CLOUD_COUNT; i++) {
        d.cloud_x[i] = (int16_t)(i * (WORLD_W / CLOUD_COUNT) + 20);
    }
    d.next_tick = millis();
}

static void spawn_obstacle(void) {
    for(uint8_t i = 0; i < OBS_MAX; i++) {
        if(d.obs[i].x >= 0) continue;
        d.obs[i].x = WORLD_W + 4;
        const uint8_t r = (uint8_t)(d.frame & 0xFF) ^ (uint8_t)(d.score & 0xFF) ^ (uint8_t)(d.frame >> 4);
        const uint8_t sp = d.speed;

        uint8_t pool[14];
        uint8_t n = 0;
        pool[n++] = OBS_CACTUS_S;
        pool[n++] = OBS_CACTUS_L;
        if(sp >= 5) {
            pool[n++] = OBS_PAIR_SS;
            pool[n++] = OBS_PAIR_SL;
            pool[n++] = OBS_PAIR_LS;
        }
        if(sp >= 6) pool[n++] = OBS_ROCK;
        if(sp >= 7) {
            pool[n++] = OBS_PAIR_LL;
            pool[n++] = OBS_BIRD_LO;
            pool[n++] = OBS_BIRD_HI;
        }
        if(sp >= 9) {
            pool[n++] = OBS_TRIO_SSS;
            pool[n++] = OBS_TRIO_SLS;
        }
        if(sp >= 10) pool[n++] = OBS_TRIO_LSL;
        d.obs[i].kind = pool[r % n];
        return;
    }
}

static bool check_hit(const Obstacle* o) {
    const int dx1 = DINO_X + 3;
    const int dy1 = d.dino_y + 3;
    const int dx2 = DINO_X + DINO_W - 3;
    const int dy2 = d.ducking ? d.dino_y + DINO_H_DUCK - 3 : d.dino_y + DINO_H_RUN - 3;

    int ox1, oy1, ox2, oy2;
    switch(o->kind) {
    case OBS_CACTUS_S:
        ox1 = o->x + 1, oy1 = GROUND_Y - 20, ox2 = o->x + 11, oy2 = GROUND_Y;
        break;
    case OBS_CACTUS_L:
        ox1 = o->x + 1, oy1 = GROUND_Y - 36, ox2 = o->x + 11, oy2 = GROUND_Y;
        break;
    case OBS_ROCK:
        ox1 = o->x + 1, oy1 = GROUND_Y - 16, ox2 = o->x + 27, oy2 = GROUND_Y;
        break;
    case OBS_PAIR_SS:
        ox1 = o->x + 1, oy1 = GROUND_Y - 20, ox2 = o->x + 26, oy2 = GROUND_Y;
        break;
    case OBS_PAIR_SL:
    case OBS_PAIR_LS:
    case OBS_PAIR_LL:
        ox1 = o->x + 1, oy1 = GROUND_Y - 36, ox2 = o->x + 26, oy2 = GROUND_Y;
        break;
    case OBS_TRIO_SSS:
        ox1 = o->x + 1, oy1 = GROUND_Y - 20, ox2 = o->x + 40, oy2 = GROUND_Y;
        break;
    case OBS_TRIO_SLS:
    case OBS_TRIO_LSL:
        ox1 = o->x + 1, oy1 = GROUND_Y - 36, ox2 = o->x + 40, oy2 = GROUND_Y;
        break;
    default: {
        const int by = (o->kind == OBS_BIRD_HI) ? GROUND_Y - 60 : GROUND_Y - 35;
        ox1 = o->x + 2, oy1 = by, ox2 = o->x + 16, oy2 = by + 10;
        break;
    }
    }
    return dx1 < ox2 && dx2 > ox1 && dy1 < oy2 && dy2 > oy1;
}

static void physics_step(void) {
    d.frame++;
    const bool on_ground = d.dino_y >= GROUND_Y - DINO_H_RUN;

    if(d.jump_pending && on_ground && !d.ducking) d.dino_vy = JUMP_VY;
    if(d.jump_pending && on_ground && !d.ducking) sound_sfx(SFX_JUMP, 2);
    d.jump_pending = false;
    d.ducking = is_pressed(InputKeyDown) && on_ground;

    d.dino_vy += GRAVITY;
    d.dino_y += d.dino_vy;
    const int ground_top = GROUND_Y - (d.ducking ? DINO_H_DUCK : DINO_H_RUN);
    if(d.dino_y >= ground_top) {
        d.dino_y = ground_top;
        d.dino_vy = 0;
    }

    if(on_ground && !d.ducking && --d.leg_timer == 0) {
        d.leg_phase ^= 1;
        d.leg_timer = 8;
    }

    for(uint8_t i = 0; i < OBS_MAX; i++) {
        if(d.obs[i].x < 0) continue;
        d.obs[i].x -= d.speed;
        if(d.obs[i].x < -30) d.obs[i].x = -1;
    }
    d.scroll += d.speed;

    if(--d.spawn_timer == 0) {
        spawn_obstacle();
        int gap = 100 - d.speed * 6;
        const int min_gap = (d.score > 500) ? 20 : 30;
        if(gap < min_gap) gap = min_gap;
        d.spawn_timer = (uint16_t)(gap + (d.frame % 20));
    }

    if((d.frame & 1) == 0) {
        const uint8_t half = (d.speed >> 1) + 1;
        for(uint8_t i = 0; i < CLOUD_COUNT; i++) {
            d.cloud_x[i] -= half;
            if(d.cloud_x[i] < -40) d.cloud_x[i] = WORLD_W + 4;
        }
    }

    for(uint8_t i = 0; i < OBS_MAX; i++) {
        if(d.obs[i].x < 0 || !check_hit(&d.obs[i])) continue;
        d.obs[i].x = -1;
        d.lives--;
        if(d.lives == 0) {
            d.game_over = true;
            sound_music_stop();
            sound_sfx(SFX_GAME_OVER, COUNT_OF(SFX_GAME_OVER));
            if(d.score > prefs.dino_hi) {
                prefs.dino_hi = d.score;
                prefs_save();
            }
            return;
        }
        d.hit_until = millis() + 150;
        break;
    }

    d.score++;
    if(d.score - d.last_speed_up >= 100 && d.speed < MAX_SPEED) {
        d.speed++;
        d.last_speed_up = d.score;
    }
}

static void start_run(void) {
    init_game();
    sound_music_start(DINO_MELODY, COUNT_OF(DINO_MELODY), true);
}

void dino_enter(bool resume) {
    if(resume) {
        if(!d.title && !d.game_over) {
            sound_music_start(DINO_MELODY, COUNT_OF(DINO_MELODY), true);
            d.next_tick = millis();
        }
        return;
    }
    d.title = true;
    d.game_over = false;
}

void dino_exit(void) {
    sound_stop_all();
}

void dino_tick(const ButtonsEdge* b) {
    if(d.title || d.game_over) {
        if(b->ok || b->up) {
            start_run();
        } else if(b->back) {
            go_to(MODE_GAMES_MENU, false);
        }
        return;
    }

    if(hold_once(InputKeyBack, 500) || (is_pressed(InputKeyDown) && is_pressed(InputKeyBack))) {
        go_to(MODE_GAMES_MENU, false);
        return;
    }

    if(b->up || b->ok) d.jump_pending = true;

    const uint32_t now = millis();
    if((int32_t)(now - d.hit_until) < 0) return;
    if((int32_t)(now - d.next_tick) < 0) return;
    d.next_tick = now + TICK_MS;
    physics_step();
}

// ---- Drawing ----------------------------------------------------------------
static void draw_cactus(Canvas* c, int wx, bool tall) {
    const int x = sx(wx);
    if(tall) {
        ui_sprite(c, x, SCREEN_GROUND - 12, SPR_CACTUS_L, 12);
    } else {
        ui_sprite(c, x, SCREEN_GROUND - 7, SPR_CACTUS_S, 7);
    }
}

static void draw_obstacle(Canvas* c, const Obstacle* o) {
    switch(o->kind) {
    case OBS_CACTUS_S:
        draw_cactus(c, o->x, false);
        break;
    case OBS_CACTUS_L:
        draw_cactus(c, o->x, true);
        break;
    case OBS_ROCK:
        ui_sprite(c, sx(o->x), SCREEN_GROUND - 5, SPR_ROCK, 5);
        break;
    case OBS_PAIR_SS:
        draw_cactus(c, o->x, false);
        draw_cactus(c, o->x + 14, false);
        break;
    case OBS_PAIR_SL:
        draw_cactus(c, o->x, false);
        draw_cactus(c, o->x + 14, true);
        break;
    case OBS_PAIR_LS:
        draw_cactus(c, o->x, true);
        draw_cactus(c, o->x + 14, false);
        break;
    case OBS_PAIR_LL:
        draw_cactus(c, o->x, true);
        draw_cactus(c, o->x + 14, true);
        break;
    case OBS_TRIO_SSS:
        draw_cactus(c, o->x, false);
        draw_cactus(c, o->x + 14, false);
        draw_cactus(c, o->x + 28, false);
        break;
    case OBS_TRIO_SLS:
        draw_cactus(c, o->x, false);
        draw_cactus(c, o->x + 14, true);
        draw_cactus(c, o->x + 28, false);
        break;
    case OBS_TRIO_LSL:
        draw_cactus(c, o->x, true);
        draw_cactus(c, o->x + 14, false);
        draw_cactus(c, o->x + 28, true);
        break;
    default: {
        const int by = (o->kind == OBS_BIRD_HI) ? GROUND_Y - 60 : GROUND_Y - 35;
        ui_sprite(c, sx(o->x), sy(by + 10), (d.frame >> 3) & 1 ? SPR_BIRD_UP : SPR_BIRD_DOWN, 5);
        break;
    }
    }
}

static void draw_ground(Canvas* c) {
    canvas_draw_line(c, 0, SCREEN_GROUND, SCREEN_W - 1, SCREEN_GROUND);
    // Pebbles give a sense of speed on a screen with no color.
    for(int i = 0; i < 8; i++) {
        int x = (i * 37 + 11 - (d.scroll * SCREEN_W) / WORLD_W) % SCREEN_W;
        if(x < 0) x += SCREEN_W;
        canvas_draw_dot(c, x, SCREEN_GROUND + 2 + (i % 3) * 2);
    }
}

void dino_draw(Canvas* c) {
    if(d.title) {
        ui_center(c, 16, FontPrimary, "DINO RUN");
        ui_sprite(c, 12, SCREEN_GROUND - 11, SPR_RUN_A, 11);
        ui_tiny_center(c, SCREEN_W / 2, 24, "OK = START");
        ui_tiny_center(c, SCREEN_W / 2, 31, "BACK = MENU");
        char hi[16];
        snprintf(hi, sizeof(hi), "HI:%lu", (unsigned long)prefs.dino_hi);
        ui_tiny_center(c, SCREEN_W / 2, 41, hi);
        canvas_draw_line(c, 0, SCREEN_GROUND, SCREEN_W - 1, SCREEN_GROUND);
        return;
    }

    if(d.game_over) {
        ui_center(c, 14, FontPrimary, "GAME OVER");
        ui_tiny_center(c, SCREEN_W / 2, 20, "OK = RETRY");
        ui_tiny_center(c, SCREEN_W / 2, 27, "BACK = MENU");
        char buf[16];
        snprintf(buf, sizeof(buf), "SC:%lu", (unsigned long)d.score);
        ui_tiny_center(c, SCREEN_W / 2, 38, buf);
        snprintf(buf, sizeof(buf), "HI:%lu", (unsigned long)prefs.dino_hi);
        ui_tiny_center(c, SCREEN_W / 2, 45, buf);
        return;
    }

    char hud[16];
    snprintf(hud, sizeof(hud), "SC:%lu", (unsigned long)d.score);
    ui_tiny(c, 1, 0, hud);
    snprintf(hud, sizeof(hud), "HI:%lu", (unsigned long)prefs.dino_hi);
    ui_tiny(c, 50, 0, hud);
    ui_bar(c, SCREEN_W - 26, 0, 25, 4, d.speed, MAX_SPEED);

    for(uint8_t i = 0; i < CLOUD_COUNT; i++) {
        const int x = sx(d.cloud_x[i]);
        const int y = 9 + i * 4;
        // canvas_draw_line() misbehaves with negative coordinates, so clouds pop in at the edges.
        if(x < 0 || x + 8 >= SCREEN_W) continue;
        canvas_draw_line(c, x, y, x + 8, y);
        canvas_draw_line(c, x + 2, y - 1, x + 5, y - 1);
    }

    draw_ground(c);

    if(d.ducking) {
        ui_sprite(c, sx(DINO_X), sy(d.dino_y + DINO_H_DUCK) - 6, SPR_DUCK, 6);
    } else {
        ui_sprite(c, sx(DINO_X), sy(d.dino_y + DINO_H_RUN) - 11, d.leg_phase ? SPR_RUN_B : SPR_RUN_A, 11);
    }

    for(uint8_t i = 0; i < OBS_MAX; i++) {
        if(d.obs[i].x >= 0) draw_obstacle(c, &d.obs[i]);
    }
}
