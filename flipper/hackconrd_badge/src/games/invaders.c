// Star Invaders. Same rules, waves and timing as the badge; grid, sprites and speeds are rescaled
// from 240x240 to the 128x64 arena.
#include "../features.h"
#include "../prefs.h"
#include "../sound.h"
#include "../ui.h"

// ---- Music ------------------------------------------------------------------
#define SQ  400
#define SDQ 600
#define S8  200
#define S16 100
static const Note SI_MELODY[] = {
    // Intro fanfare
    {440, S16}, {523, S16}, {659, S16}, {880, S8}, {0, S16},
    {784, S16}, {659, S16}, {523, S16}, {440, S8}, {0, S16},
    {440, S16}, {523, S16}, {659, S16}, {784, S16}, {659, SQ}, {0, S8},
    // Section A
    {330, S8}, {392, S8}, {440, S8}, {523, S8}, {494, S8}, {440, S8}, {392, S8}, {330, S8},
    {349, S8}, {440, S8}, {523, S8}, {659, S8}, {587, S8}, {523, S8}, {440, S8}, {349, S8},
    {392, S8}, {440, S8}, {494, S8}, {523, S8}, {587, S8}, {659, S8}, {587, S8}, {523, S8},
    {494, SQ}, {440, SQ}, {0, SQ},
    // Section B
    {880, S8}, {784, S8}, {698, S8}, {659, S8}, {587, S8}, {523, S8}, {494, S8}, {440, S8},
    {392, S8}, {440, S8}, {494, S8}, {523, S8}, {587, S8}, {659, SDQ}, {0, S8},
    {659, S8}, {587, S8}, {523, S8}, {494, S8}, {440, S8}, {392, S8}, {349, S8}, {330, S8},
    {440, SQ}, {659, SQ}, {880, SQ}, {0, SQ},
};
#undef SQ
#undef SDQ
#undef S8
#undef S16

static const Note SFX_GAME_OVER[] = {
    {494, 120}, {440, 120}, {392, 120}, {349, 120}, {294, 120}, {262, 120}, {220, 120},
};

// ---- Constants --------------------------------------------------------------
#define COLS         10
#define ROWS         5
#define CELL_W       12
#define CELL_H       6
#define ALIEN_W      9
#define ALIEN_H      5
#define GRID_X       4
#define GRID_Y       8
#define GRID_Y_MAX   20
#define EDGE_RIGHT   125
#define EDGE_LEFT    3
#define STEP_X       2
#define STEP_Y       2
#define PLAYER_Y     58
#define PLAYER_W     9
#define PLAYER_H     5
#define PLAYER_MOVE_MS 17
#define PBUL_MS      24
#define ABUL_MS      50
#define BULLET_H     3
#define SHOOT_CD_MS  180
#define MAX_PBUL     3
#define MAX_ABUL     4
#define WAVE_PAUSE_MS 1800

static struct {
    bool alive[ROWS][COLS];
    int16_t grid_x, grid_y;
    int8_t dir_x, dir_y;
    int16_t player_x;
    int16_t pbul_x[MAX_PBUL], pbul_y[MAX_PBUL];
    int16_t abul_x[MAX_ABUL], abul_y[MAX_ABUL];
    uint32_t score;
    uint8_t lives;
    bool game_over, title;
    uint8_t wave;
    uint32_t wave_clear_until;
    uint32_t move_next, shoot_next, player_move_next, pbul_next, abul_next, shoot_cd;
    uint8_t anim;
    uint8_t aliens_alive;
    uint32_t flash_until;
} si;

static int16_t right_edge(void) {
    for(int c = COLS - 1; c >= 0; c--) {
        for(int r = 0; r < ROWS; r++) {
            if(si.alive[r][c]) return si.grid_x + c * CELL_W + ALIEN_W;
        }
    }
    return si.grid_x;
}

static int16_t left_edge(void) {
    for(int c = 0; c < COLS; c++) {
        for(int r = 0; r < ROWS; r++) {
            if(si.alive[r][c]) return si.grid_x + c * CELL_W;
        }
    }
    return si.grid_x;
}

static int16_t bottom_edge(void) {
    for(int r = ROWS - 1; r >= 0; r--) {
        for(int c = 0; c < COLS; c++) {
            if(si.alive[r][c]) return si.grid_y + r * CELL_H + ALIEN_H;
        }
    }
    return si.grid_y;
}

static uint32_t move_interval(void) {
    if(si.aliens_alive <= 1) return 30; // last alien goes turbo
    const uint32_t base = 550;
    const uint32_t fast = 80;
    return fast + (base - fast) * si.aliens_alive / (ROWS * COLS);
}

static void clear_bullets(void) {
    for(uint8_t i = 0; i < MAX_PBUL; i++) si.pbul_x[i] = -1;
    for(uint8_t i = 0; i < MAX_ABUL; i++) si.abul_x[i] = -1;
}

static void reset_formation(void) {
    memset(si.alive, 1, sizeof(si.alive));
    si.aliens_alive = ROWS * COLS;
    si.grid_x = GRID_X;
    si.grid_y = GRID_Y;
    si.dir_x = 1;
    si.dir_y = 1;
    si.player_x = (SCREEN_W - PLAYER_W) / 2;
    si.anim = 0;
    si.flash_until = 0;
    clear_bullets();
}

static void save_hi_score(void) {
    if(si.score > prefs.si_hi) {
        prefs.si_hi = si.score;
        prefs_save();
    }
}

static void next_wave(void) {
    const uint32_t now = millis();
    si.wave++;
    if(si.lives < 3) si.lives++;
    reset_formation();
    si.move_next = now + WAVE_PAUSE_MS + 500;
    si.shoot_next = now + WAVE_PAUSE_MS + 2000;
    si.shoot_cd = now + WAVE_PAUSE_MS + 500;
    si.wave_clear_until = now + WAVE_PAUSE_MS;
}

static void init_game(void) {
    const uint32_t now = millis();
    reset_formation();
    si.game_over = false;
    si.wave = 1;
    si.wave_clear_until = 0;
    si.player_move_next = 0;
    si.pbul_next = 0;
    si.abul_next = 0;
    si.shoot_cd = 0;
    si.move_next = now + 500;
    si.shoot_next = now + 2000;
    sound_music_start(SI_MELODY, COUNT_OF(SI_MELODY), true);
}

static void end_game(void) {
    si.game_over = true;
    sound_music_stop();
    sound_sfx(SFX_GAME_OVER, COUNT_OF(SFX_GAME_OVER));
    save_hi_score();
}

void invaders_enter(bool resume) {
    if(resume) {
        if(!si.title && !si.game_over) sound_music_start(SI_MELODY, COUNT_OF(SI_MELODY), true);
        return;
    }
    si.title = true;
    si.game_over = false;
    si.lives = 3;
    si.score = 0;
}

void invaders_exit(void) {
    sound_stop_all();
}

void invaders_tick(const ButtonsEdge* b) {
    if(si.title) {
        if(b->ok) {
            si.title = false;
            init_game();
        } else if(b->back) {
            go_to(MODE_GAMES_MENU, false);
        }
        return;
    }

    if(si.game_over) {
        if(b->ok) {
            si.lives = 3;
            si.score = 0;
            init_game();
        } else if(b->back) {
            go_to(MODE_GAMES_MENU, false);
        }
        return;
    }

    const uint32_t now = millis();

    if(si.wave_clear_until > 0) {
        if((int32_t)(now - si.wave_clear_until) >= 0) si.wave_clear_until = 0;
        return;
    }

    if(si.flash_until > 0) {
        if((int32_t)(now - si.flash_until) >= 0) {
            si.flash_until = 0;
            if(si.lives == 0) end_game();
        }
        return;
    }

    if(hold_once(InputKeyBack, 500) || (is_pressed(InputKeyDown) && is_pressed(InputKeyBack))) {
        save_hi_score();
        go_to(MODE_GAMES_MENU, false);
        return;
    }

    // Player. UP/DOWN keep the badge's mapping; LEFT/RIGHT are what a Flipper user expects.
    if((int32_t)(now - si.player_move_next) >= 0) {
        const bool go_left = is_pressed(InputKeyLeft) || is_pressed(InputKeyUp);
        const bool go_right = is_pressed(InputKeyRight) || is_pressed(InputKeyDown);
        if(go_left && si.player_x > 1) si.player_x--;
        if(go_right && si.player_x < SCREEN_W - PLAYER_W - 1) si.player_x++;
        if(go_left || go_right) si.player_move_next = now + PLAYER_MOVE_MS;
    }

    if(b->ok && (int32_t)(now - si.shoot_cd) >= 0) {
        for(uint8_t i = 0; i < MAX_PBUL; i++) {
            if(si.pbul_x[i] == -1) {
                si.pbul_x[i] = si.player_x + PLAYER_W / 2;
                si.pbul_y[i] = PLAYER_Y - BULLET_H;
                si.shoot_cd = now + SHOOT_CD_MS;
                break;
            }
        }
    }

    // Player bullets
    if((int32_t)(now - si.pbul_next) >= 0) {
        si.pbul_next = now + PBUL_MS;
        for(uint8_t i = 0; i < MAX_PBUL; i++) {
            if(si.pbul_x[i] == -1) continue;
            si.pbul_y[i]--;
            if(si.pbul_y[i] < 7) {
                si.pbul_x[i] = -1;
                continue;
            }
            bool hit = false;
            for(uint8_t r = 0; r < ROWS && !hit; r++) {
                for(uint8_t c = 0; c < COLS && !hit; c++) {
                    if(!si.alive[r][c]) continue;
                    const int16_t ax = si.grid_x + c * CELL_W;
                    const int16_t ay = si.grid_y + r * CELL_H;
                    if(si.pbul_x[i] >= ax && si.pbul_x[i] < ax + ALIEN_W &&
                       si.pbul_y[i] + BULLET_H > ay && si.pbul_y[i] < ay + ALIEN_H) {
                        si.alive[r][c] = false;
                        si.aliens_alive--;
                        si.score += (uint32_t)(ROWS - r) * 10;
                        si.pbul_x[i] = -1;
                        hit = true;
                        if(si.aliens_alive == 0) {
                            save_hi_score();
                            next_wave();
                            return;
                        }
                    }
                }
            }
        }
    }

    // Alien grid
    if((int32_t)(now - si.move_next) >= 0) {
        si.move_next = now + move_interval();
        si.anim ^= 1;

        bool descend = false;
        if(si.dir_x > 0 && right_edge() >= EDGE_RIGHT) descend = true;
        if(si.dir_x < 0 && left_edge() <= EDGE_LEFT) descend = true;

        if(descend) {
            si.dir_x = -si.dir_x;
            si.grid_y += si.dir_y * STEP_Y;
            if(si.dir_y > 0 && si.grid_y >= GRID_Y_MAX) {
                si.grid_y = GRID_Y_MAX;
                si.dir_y = -1;
            }
            if(si.dir_y < 0 && si.grid_y <= GRID_Y) {
                si.grid_y = GRID_Y;
                si.dir_y = 1;
            }
            if(bottom_edge() >= PLAYER_Y - 2) {
                si.lives = 0;
                end_game();
                return;
            }
        } else {
            si.grid_x += si.dir_x * STEP_X;
        }
    }

    // Alien fire
    if((int32_t)(now - si.shoot_next) >= 0 && si.aliens_alive > 0) {
        si.shoot_next = now + 300 + (rand_u32() % 500);
        for(uint8_t i = 0; i < MAX_ABUL; i++) {
            if(si.abul_x[i] != -1) continue;
            const uint8_t col = rand8() % COLS;
            bool found = false;
            for(uint8_t attempt = 0; attempt < COLS && !found; attempt++) {
                const uint8_t c = (col + attempt) % COLS;
                for(int r = ROWS - 1; r >= 0; r--) {
                    if(si.alive[r][c]) {
                        si.abul_x[i] = si.grid_x + c * CELL_W + ALIEN_W / 2;
                        si.abul_y[i] = si.grid_y + r * CELL_H + ALIEN_H;
                        found = true;
                        break;
                    }
                }
            }
            break;
        }
    }

    // Alien bullets
    if((int32_t)(now - si.abul_next) >= 0) {
        si.abul_next = now + ABUL_MS;
        for(uint8_t i = 0; i < MAX_ABUL; i++) {
            if(si.abul_x[i] == -1) continue;
            si.abul_y[i]++;
            if(si.abul_y[i] > SCREEN_H - 1) {
                si.abul_x[i] = -1;
                continue;
            }
            if(si.abul_y[i] + BULLET_H >= PLAYER_Y && si.abul_y[i] <= PLAYER_Y + PLAYER_H &&
               si.abul_x[i] >= si.player_x && si.abul_x[i] <= si.player_x + PLAYER_W) {
                si.abul_x[i] = -1;
                si.lives--;
                si.flash_until = now + 600;
            }
        }
    }
}

// ---- Drawing ----------------------------------------------------------------
static void draw_alien(Canvas* c, int x, int y, uint8_t row, uint8_t anim) {
    canvas_draw_box(c, x + 1, y + 1, 7, 3); // body
    canvas_draw_box(c, x, y + 2, 9, 1); // arms
    // Eyes are holes so rows stay readable at this size.
    canvas_set_color(c, ColorWhite);
    canvas_draw_dot(c, x + 3, y + 2);
    canvas_draw_dot(c, x + 5, y + 2);
    canvas_set_color(c, ColorBlack);
    if(row <= 1) { // antennae
        canvas_draw_dot(c, x + 2, y);
        canvas_draw_dot(c, x + 6, y);
    } else {
        canvas_draw_line(c, x + 3, y, x + 5, y);
    }
    if(anim == 0) { // legs
        canvas_draw_dot(c, x + 1, y + 4);
        canvas_draw_dot(c, x + 7, y + 4);
    } else {
        canvas_draw_dot(c, x + 3, y + 4);
        canvas_draw_dot(c, x + 5, y + 4);
    }
}

static void draw_player(Canvas* c, int x) {
    canvas_draw_box(c, x, PLAYER_Y + 3, PLAYER_W, 2);
    canvas_draw_box(c, x + 2, PLAYER_Y + 1, 5, 2);
    canvas_draw_box(c, x + 4, PLAYER_Y, 1, 1);
}

static void draw_hud(Canvas* c) {
    char buf[16];
    if(si.score > 99999) {
        strlcpy(buf, "SC:99999+", sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), "SC:%lu", (unsigned long)si.score);
    }
    ui_tiny(c, 0, 0, buf);
    snprintf(buf, sizeof(buf), "W:%u", si.wave);
    ui_tiny(c, 40, 0, buf);
    snprintf(buf, sizeof(buf), "HI:%lu", (unsigned long)prefs.si_hi);
    ui_tiny(c, 60, 0, buf);
    for(uint8_t i = 0; i < si.lives; i++) canvas_draw_box(c, 116 + i * 4, 1, 3, 3);
    canvas_draw_line(c, 0, 6, SCREEN_W - 1, 6);
}

void invaders_draw(Canvas* c) {
    if(si.title) {
        ui_center(c, 11, FontPrimary, "STAR");
        ui_center(c, 22, FontPrimary, "INVADERS");
        draw_alien(c, 59, 27, 0, 0);
        char buf[20];
        snprintf(buf, sizeof(buf), "HI-SCORE: %lu", (unsigned long)prefs.si_hi);
        ui_tiny_center(c, SCREEN_W / 2, 36, buf);
        ui_tiny_center(c, SCREEN_W / 2, 46, "[OK] TO START");
        ui_tiny_center(c, SCREEN_W / 2, 54, "[BACK] TO MENU");
        return;
    }

    if(si.game_over) {
        ui_center(c, 12, FontPrimary, "GAME OVER");
        char buf[20];
        snprintf(buf, sizeof(buf), "WAVE: %u", si.wave);
        ui_tiny_center(c, SCREEN_W / 2, 18, buf);
        snprintf(buf, sizeof(buf), "SCORE: %lu", (unsigned long)si.score);
        ui_tiny_center(c, SCREEN_W / 2, 26, buf);
        snprintf(buf, sizeof(buf), "HI-SCORE: %lu", (unsigned long)prefs.si_hi);
        ui_tiny_center(c, SCREEN_W / 2, 34, buf);
        ui_tiny_center(c, SCREEN_W / 2, 46, "[OK] TO RETRY");
        ui_tiny_center(c, SCREEN_W / 2, 54, "[BACK] TO MENU");
        return;
    }

    draw_hud(c);

    for(uint8_t r = 0; r < ROWS; r++) {
        for(uint8_t col = 0; col < COLS; col++) {
            if(si.alive[r][col]) {
                draw_alien(c, si.grid_x + col * CELL_W, si.grid_y + r * CELL_H, r, si.anim);
            }
        }
    }

    const bool show_player = si.flash_until == 0 || ((millis() / 100) & 1);
    if(show_player) draw_player(c, si.player_x);

    for(uint8_t i = 0; i < MAX_PBUL; i++) {
        if(si.pbul_x[i] != -1) {
            canvas_draw_line(c, si.pbul_x[i], si.pbul_y[i], si.pbul_x[i], si.pbul_y[i] + BULLET_H - 1);
        }
    }
    for(uint8_t i = 0; i < MAX_ABUL; i++) {
        if(si.abul_x[i] != -1) {
            // Dotted, so enemy fire can be told from yours.
            canvas_draw_dot(c, si.abul_x[i], si.abul_y[i]);
            canvas_draw_dot(c, si.abul_x[i], si.abul_y[i] + 2);
        }
    }

    if(si.wave_clear_until > 0) {
        ui_dialog_box(c, 24, 18, SCREEN_W - 48, 26);
        char buf[12];
        snprintf(buf, sizeof(buf), "WAVE %u", si.wave - 1);
        ui_center(c, 29, FontPrimary, buf);
        ui_center(c, 40, FontPrimary, "CLEAR!");
    }
}
