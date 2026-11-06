// HackerMon: the badge's open-world RPG. Same four maps, enemies, moves, XP table and ranks. Tiles
// shrink from 12 to 6 px, so the 20x17 map no longer fits vertically and the camera now scrolls.
#include "../features.h"
#include "../prefs.h"
#include "../sound.h"
#include "../ui.h"

// ---- Maps -------------------------------------------------------------------
// 0 = open ground (random encounters)  1 = wall  3 = healing spring  4 = portal
#define MAP_W    20
#define MAP_H    17
#define NUM_MAPS 4
#define TILE     6
#define VIEW_H   56
#define MAP_X    ((SCREEN_W - MAP_W * TILE) / 2)

static const uint8_t MAPS[NUM_MAPS][MAP_H][MAP_W] = {
    {
        // Forest
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
    },
    {
        // Desert
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
    },
    {
        // Snow
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
    },
    {
        // Island
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
    },
};

static const char* const MAP_NAMES[NUM_MAPS] = {"FOREST", "DESERT", "SNOW", "ISLAND"};

// ---- Music ------------------------------------------------------------------
#define P8(f)  {(f), 100}, {0, 25}
#define P4(f)  {(f), 210}, {0, 50}
#define P2(f)  {(f), 430}, {0, 80}
#define P16(f) {(f), 45}, {0, 12}

static const Note OVERWORLD_MELODY[] = {
    P8(523), P8(587), P8(659), P4(784), P8(659), P8(587), P8(523), P4(440),
    P8(494), P8(523), P8(587), P4(659), P8(587), P8(523), P8(494), P2(440),
    P8(659), P8(698), P8(784), P4(880), P8(784), P8(698), P8(659), P4(587),
    P8(523), P8(587), P8(659), P8(587), P8(523), P8(494), P2(523),
};
static const Note BATTLE_MELODY[] = {
    P16(880), P16(988), P8(1047), P16(880), P16(988), P8(784),
    P16(880), P16(988), P8(1047), P8(1175), P4(1047),
    P16(659), P16(698), P8(784), P16(659), P16(698), P8(587),
    P16(523), P16(587), P8(659), P8(784), P4(659),
    P8(880), P8(784), P8(698), P8(659), P8(587), P8(523), P2(440),
};
static const Note VICTORY_MELODY[] = {
    P8(784), P8(784), P8(784), P8(784), P8(659), P8(698), P8(784), {0, 80}, P8(698), P8(784),
};
#undef P8
#undef P4
#undef P2
#undef P16

static const Note SFX_GAME_OVER[] = {{523, 200}, {415, 200}, {330, 200}, {262, 200}, {208, 200}, {165, 200}};
static const Note SFX_ENCOUNTER[] = {{880, 60}, {1175, 60}};

// ---- Enemies and moves ------------------------------------------------------
#define MAX_ENEMIES 10

typedef struct {
    const char* name;
    uint8_t max_hp;
    uint8_t attack;
    uint8_t defense;
    uint16_t xp_given;
    uint8_t min_level;
} Enemy;

static const Enemy ENEMIES[MAX_ENEMIES] = {
    {"AVAST", 30, 7, 7, 14, 1},
    {"Win Defender", 36, 8, 8, 18, 1},
    {"Malwarebytes", 32, 8, 7, 16, 1},
    {"Kaspersky", 52, 10, 11, 28, 2},
    {"Fortinet", 65, 12, 13, 34, 3},
    {"CrowdStrike", 68, 13, 13, 38, 4},
    {"SentinelOne", 66, 13, 12, 36, 4},
    {"Palo Alto", 72, 14, 15, 42, 5},
    {"Check Point", 75, 15, 15, 48, 6},
    {"Cisco ASA", 100, 20, 19, 85, 8},
};

#define MOVES 4

typedef struct {
    const char* name;
    uint8_t power;
    uint8_t pp;
} Move;

static const Move MOVE_LIST[MOVES] = {
    {"SQL Inject", 12, 15},
    {"0-day", 25, 4},
    {"Exploit Kit", 18, 8},
    {"Port Scan", 8, 20},
};

static const uint16_t XP_TABLE[10] = {0, 20, 45, 80, 130, 200, 290, 400, 540, 720};

#define WIN_MASK       ((1 << MAX_ENEMIES) - 1)
#define MOVE_DELAY_MS  160
#define ENCOUNTER_RATE 5
#define FLASH_MS       200
#define PORTAL_FLASH_MS 360

typedef enum { SCR_TITLE, SCR_OVERWORLD, SCR_BATTLE, SCR_RESULT, SCR_TERMINAL } Screen;
typedef enum { FLASH_NONE, FLASH_BATTLE, FLASH_PORTAL } FlashKind;

static struct {
    Screen screen;
    bool result_won;

    // Overworld
    uint8_t map_idx;
    int16_t px, py;
    int8_t facing; // 0=down 1=up 2=left 3=right
    uint8_t step_count;
    uint16_t xp;
    uint8_t level;
    uint8_t hp, max_hp;
    uint8_t move_pp[MOVES];
    bool walk_anim;
    uint32_t last_move_ms;
    bool game_over, game_won;
    uint32_t start_ms;
    uint32_t survived_secs;
    uint8_t heal_from;

    FlashKind flash;
    uint32_t flash_until;

    // Battle
    uint8_t enemy_idx;
    uint8_t enemy_hp;
    uint8_t phase;
    uint8_t cursor;
    char msg1[40];
    char msg2[32];
    uint32_t delay_until;
    bool player_hurt, enemy_hurt;
    bool battle_over, battle_won;
    uint8_t anim_frame;
    uint32_t anim_ms;

    uint16_t defeated_mask;
    uint8_t rand_state;
} h;

// ---- Helpers ----------------------------------------------------------------
static uint8_t xp_to_level(uint16_t xp) {
    for(int lv = 9; lv >= 1; lv--) {
        if(xp >= XP_TABLE[lv]) return (uint8_t)(lv + 1);
    }
    return 1;
}

static uint8_t xp_to_rank(uint16_t xp) {
    if(xp < 50) return 0;
    if(xp < 150) return 1;
    if(xp < 300) return 2;
    if(xp < 500) return 3;
    if(xp < 720) return 4;
    return 5;
}

static const char* rank_name(uint8_t rank) {
    switch(rank) {
    case 0:
        return "SCRIPT KIDDIE";
    case 1:
        return "NOOB HACKER";
    case 2:
        return "GREY HAT";
    case 3:
        return "BLACK HAT";
    case 4:
        return "ELITE HACKER";
    default:
        return "LEGENDARY";
    }
}

static uint8_t level_to_max_hp(uint8_t lv) {
    return 20 + lv * 5;
}

static uint8_t level_to_atk(uint8_t lv) {
    return 3 + lv * 2;
}

static uint8_t map_at(int tx, int ty) {
    if(tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return MAPS[h.map_idx][ty][tx];
}

static uint8_t rand_battle(void) {
    uint8_t r = h.rand_state;
    r ^= (uint8_t)(millis() >> 3);
    r ^= r << 3;
    r ^= r >> 2;
    r ^= (uint8_t)h.xp;
    h.rand_state = r;
    return r;
}

static void save_best_rank(uint8_t rank) {
    if(rank > prefs.pkm_best_rank) {
        prefs.pkm_best_rank = rank;
        prefs_save();
    }
}

static void music_overworld(void) {
    sound_music_start(OVERWORLD_MELODY, COUNT_OF(OVERWORLD_MELODY), true);
}

static void music_battle(void) {
    sound_music_start(BATTLE_MELODY, COUNT_OF(BATTLE_MELODY), true);
}

// ---- Setup ------------------------------------------------------------------
static void new_game(void) {
    h.map_idx = 0;
    h.px = 10;
    h.py = 10;
    h.facing = 0;
    h.step_count = 0;
    h.xp = 0;
    h.level = 1;
    h.max_hp = level_to_max_hp(1);
    h.hp = h.max_hp;
    for(int i = 0; i < MOVES; i++) h.move_pp[i] = MOVE_LIST[i].pp;
    h.walk_anim = false;
    h.last_move_ms = millis();
    h.game_over = false;
    h.game_won = false;
    h.defeated_mask = 0;
    h.survived_secs = 0;
    h.start_ms = millis();
    h.screen = SCR_OVERWORLD;
    h.phase = 0;
    h.cursor = 0;
    h.flash = FLASH_NONE;
    h.rand_state = 42;
    music_overworld();
}

void hackermon_enter(bool resume) {
    if(resume) {
        if(h.screen == SCR_OVERWORLD) music_overworld();
        if(h.screen == SCR_BATTLE) music_battle();
        return;
    }
    h.screen = SCR_TITLE;
}

void hackermon_exit(void) {
    sound_stop_all();
}

// ---- Battle -----------------------------------------------------------------
static uint8_t pick_enemy(void) {
    uint8_t pool[MAX_ENEMIES];
    uint8_t count = 0;
    for(uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if(h.level >= ENEMIES[i].min_level) pool[count++] = i;
    }
    if(count == 0) return 0;
    return pool[(uint8_t)(millis() & 0xFF) % count];
}

static void set_msg(const char* a, const char* b) {
    strlcpy(h.msg1, a, sizeof(h.msg1));
    strlcpy(h.msg2, b ? b : "", sizeof(h.msg2));
}

static void start_battle(uint8_t enemy) {
    h.enemy_idx = enemy;
    h.enemy_hp = ENEMIES[enemy].max_hp;
    h.phase = 0;
    h.cursor = 0;
    h.battle_over = false;
    h.battle_won = false;
    h.player_hurt = false;
    h.enemy_hurt = false;
    h.anim_frame = 0;
    h.anim_ms = millis();
    h.delay_until = 0;
    h.screen = SCR_BATTLE;
    music_battle();
    char buf[40];
    snprintf(buf, sizeof(buf), "A WILD %s APPEARED!", ENEMIES[enemy].name);
    set_msg(buf, "CHOOSE YOUR ATTACK!");
}

static void apply_move_to_enemy(uint8_t move) {
    const uint8_t power = MOVE_LIST[move].power;
    const uint8_t atk = level_to_atk(h.level);
    const uint8_t def = ENEMIES[h.enemy_idx].defense;
    uint8_t dmg = power + atk + (rand_battle() & 3);
    if(dmg > def + 2) {
        dmg -= def;
    } else {
        dmg = 1;
    }

    if(dmg >= h.enemy_hp) {
        h.enemy_hp = 0;
        h.battle_won = true;
        h.battle_over = true;
    } else {
        h.enemy_hp -= dmg;
    }
    h.enemy_hurt = true;
    h.anim_frame = 0;
    h.anim_ms = millis();

    char buf[40];
    snprintf(buf, sizeof(buf), "HACKER USED %s!", MOVE_LIST[move].name);
    set_msg(buf, NULL);
}

static void enemy_attack(void) {
    uint8_t dmg = ENEMIES[h.enemy_idx].attack + (rand_battle() & 3);
    const uint8_t pdef = h.level / 2;
    if(dmg > pdef + 1) {
        dmg -= pdef;
    } else {
        dmg = 1;
    }

    if(dmg >= h.hp) {
        h.hp = 0;
        h.game_over = true;
        h.battle_over = true;
        h.battle_won = false;
        sound_music_stop();
        sound_sfx(SFX_GAME_OVER, COUNT_OF(SFX_GAME_OVER));
    } else {
        h.hp -= dmg;
    }
    h.player_hurt = true;
    h.anim_frame = 0;
    h.anim_ms = millis();

    char buf[40];
    snprintf(buf, sizeof(buf), "%s ATTACKED!", ENEMIES[h.enemy_idx].name);
    set_msg(buf, NULL);
}

static void award_xp(void) {
    const uint8_t gain = (uint8_t)ENEMIES[h.enemy_idx].xp_given;
    h.defeated_mask |= (1 << h.enemy_idx);
    h.xp += gain;

    const uint8_t new_level = xp_to_level(h.xp);
    char buf[40];
    if(new_level > h.level && new_level <= 10) {
        h.level = new_level;
        const uint8_t new_max = level_to_max_hp(h.level);
        h.hp += (new_max - h.max_hp); // heal on level up
        h.max_hp = new_max;
        if(h.hp > h.max_hp) h.hp = h.max_hp;
        snprintf(buf, sizeof(buf), "LEVEL UP! NOW LV %u!", h.level);
    } else {
        snprintf(buf, sizeof(buf), "+%u XP! ENEMY DEFEATED!", gain);
    }
    set_msg(buf, "PRESS OK TO CONTINUE");

    if((h.defeated_mask & WIN_MASK) == WIN_MASK) h.game_won = true;
}

static void to_overworld(void) {
    h.screen = SCR_OVERWORLD;
}

static void finish_run(bool won) {
    h.survived_secs = (millis() - h.start_ms) / 1000;
    save_best_rank(xp_to_rank(h.xp));
    h.result_won = won;
    h.screen = SCR_RESULT;
    sound_music_stop();
}

static void battle_tick(const ButtonsEdge* b) {
    const uint32_t now = millis();

    if(h.enemy_hurt || h.player_hurt) {
        if((int32_t)(now - h.anim_ms) >= 120) {
            h.anim_frame++;
            h.anim_ms = now;
            if(h.anim_frame >= 3) {
                h.enemy_hurt = false;
                h.player_hurt = false;
            }
        }
        return;
    }

    switch(h.phase) {
    case 0: // choose a move
        if(hold_once(InputKeyBack, 400) || b->back_short) {
            // Fleeing costs 5 HP, as on the badge.
            if(h.hp > 5) {
                h.hp -= 5;
            } else {
                h.hp = 1;
            }
            music_overworld();
            to_overworld();
            return;
        }
        if(b->up) h.cursor = (h.cursor + 2) % MOVES;
        if(b->down) h.cursor = (h.cursor + 2) % MOVES;
        if(b->left) h.cursor = (h.cursor + 3) % MOVES;
        if(b->right) h.cursor = (h.cursor + 1) % MOVES;
        if(b->ok) {
            if(h.move_pp[h.cursor] > 0) {
                h.move_pp[h.cursor]--;
                apply_move_to_enemy(h.cursor);
                h.delay_until = now + 1300;
                h.phase = 1;
            } else {
                set_msg("NO PP! PICK ANOTHER.", NULL);
            }
        }
        break;

    case 1: // player move resolved
        if((int32_t)(now - h.delay_until) < 0) return;
        if(h.battle_over) {
            if(h.battle_won) {
                award_xp();
                sound_music_stop();
                sound_music_start(VICTORY_MELODY, COUNT_OF(VICTORY_MELODY), false);
            } else {
                set_msg("YOU WERE DEFEATED...", "PRESS OK OR BACK");
            }
            h.phase = 3;
            return;
        }
        enemy_attack();
        h.delay_until = now + 1200;
        h.phase = 2;
        break;

    case 2: // enemy move resolved
        if((int32_t)(now - h.delay_until) < 0) return;
        if(h.battle_over) {
            set_msg("YOU WERE DEFEATED...", "PRESS OK OR BACK");
            h.phase = 3;
            return;
        }
        h.phase = 0;
        set_msg("WHAT WILL YOU DO?", "CHOOSE YOUR MOVE:");
        break;

    case 3: // end of battle
        if(b->ok || b->back) {
            sound_music_stop();
            if(h.game_over) {
                finish_run(false);
            } else if(h.game_won) {
                finish_run(true);
            } else {
                music_overworld();
                to_overworld();
            }
        }
        break;
    default:
        break;
    }
}

// ---- Overworld --------------------------------------------------------------
static void do_portal(void) {
    // Each map's portal leads to the next map's west entrance.
    h.map_idx = (h.map_idx + 1) % NUM_MAPS;
    h.px = 1;
    h.py = 10;
}

static void overworld_tick(void) {
    const uint32_t now = millis();

    if(h.flash != FLASH_NONE) {
        if((int32_t)(now - h.flash_until) >= 0) {
            const FlashKind kind = h.flash;
            h.flash = FLASH_NONE;
            if(kind == FLASH_BATTLE) {
                start_battle(pick_enemy());
            } else {
                do_portal();
            }
        }
        return;
    }

    if(hold_once(InputKeyBack, 500) || (is_pressed(InputKeyDown) && is_pressed(InputKeyBack))) {
        go_to(MODE_GAMES_MENU, false);
        return;
    }

    if((int32_t)(now - h.last_move_ms) < MOVE_DELAY_MS) return;

    int8_t dx = 0;
    int8_t dy = 0;
    bool moved = false;
    if(is_pressed(InputKeyUp)) {
        dy = -1;
        h.facing = 1;
        moved = true;
    }
    if(is_pressed(InputKeyDown)) {
        dy = 1;
        h.facing = 0;
        moved = true;
    }
    if(is_pressed(InputKeyLeft)) {
        dx = -1;
        h.facing = 2;
        moved = true;
    }
    if(is_pressed(InputKeyRight)) {
        dx = 1;
        h.facing = 3;
        moved = true;
    }
    if(dx && dy) dy = 0;
    if(!moved) return;

    const int16_t nx = h.px + dx;
    const int16_t ny = h.py + dy;
    if(map_at(nx, ny) == 1) return;

    h.px = nx;
    h.py = ny;
    h.last_move_ms = now;
    h.walk_anim = !h.walk_anim;

    const uint8_t tile = map_at(h.px, h.py);
    if(tile == 3) {
        h.heal_from = h.hp;
        h.hp = h.max_hp;
        for(int i = 0; i < MOVES; i++) h.move_pp[i] = MOVE_LIST[i].pp;
        h.screen = SCR_TERMINAL;
        return;
    }
    if(tile == 4) {
        h.flash = FLASH_PORTAL;
        h.flash_until = now + PORTAL_FLASH_MS;
        return;
    }
    if(tile == 0) {
        h.step_count++;
        const uint8_t roll = rand_battle();
        if(h.step_count >= 3 && (roll % ENCOUNTER_RATE) == 0) {
            h.step_count = 0;
            sound_sfx(SFX_ENCOUNTER, COUNT_OF(SFX_ENCOUNTER));
            h.flash = FLASH_BATTLE;
            h.flash_until = now + FLASH_MS;
        }
    } else {
        h.step_count = 0;
    }
}

void hackermon_tick(const ButtonsEdge* b) {
    switch(h.screen) {
    case SCR_OVERWORLD:
        overworld_tick();
        break;
    case SCR_BATTLE:
        battle_tick(b);
        break;
    case SCR_TERMINAL:
        if(b->ok || b->back) h.screen = SCR_OVERWORLD;
        break;
    case SCR_TITLE:
        if(b->ok) {
            new_game();
        } else if(b->back) {
            go_to(MODE_GAMES_MENU, false);
        }
        break;
    case SCR_RESULT:
        if(b->ok) {
            h.screen = SCR_TITLE;
        } else if(b->back) {
            go_to(MODE_GAMES_MENU, false);
        }
        break;
    }
}

// ---- Tile art ---------------------------------------------------------------
static const char* const WALLS[NUM_MAPS][TILE] = {
    {".####.", "######", "##.###", ".####.", "..##..", "..##.."}, // tree
    {"..##..", "#.##..", "#.##.#", "#####.", "..##..", "..##.."}, // cactus
    {"..##..", ".####.", ".#..#.", "#.##.#", "#....#", "######"}, // ice rock
    {"##.###", ".####.", "###.##", "..##..", "..##..", ".####."}, // palm
};
static const char* const SPRING[TILE] = {"######", "#....#", "#.##.#", "#.##.#", "#....#", "######"};
static const char* const PORTAL_A[TILE] = {"######", "#.##.#", "##..##", "##..##", "#.##.#", "######"};
static const char* const PORTAL_B[TILE] = {"######", "#....#", "#.##.#", "#.##.#", "#....#", "######"};

static const char* const PLAYER_DOWN_A[TILE] = {".####.", ".#..#.", ".####.", "#.##.#", ".####.", ".#..#."};
static const char* const PLAYER_DOWN_B[TILE] = {".####.", ".#..#.", ".####.", "#.##.#", ".####.", ".##.#."};
static const char* const PLAYER_UP_A[TILE] = {".####.", ".####.", ".####.", "#.##.#", ".####.", ".#..#."};
static const char* const PLAYER_UP_B[TILE] = {".####.", ".####.", ".####.", "#.##.#", ".####.", ".#.##."};
static const char* const PLAYER_LEFT[TILE] = {".####.", ".#.##.", ".####.", "..###.", ".####.", "..#.#."};
static const char* const PLAYER_RIGHT[TILE] = {".####.", ".##.#.", ".####.", ".###..", ".####.", ".#.#.."};

static void draw_floor(Canvas* c, int sx, int sy, int tx, int ty) {
    const uint8_t hash = (uint8_t)(tx * 5 + ty * 7);
    switch(h.map_idx) {
    case 0: // grass tufts
        if((hash & 3) == 0) {
            canvas_draw_dot(c, sx + 1, sy + 3);
            canvas_draw_dot(c, sx + 2, sy + 2);
        }
        break;
    case 1: // sand specks
        if((hash & 7) == 2) canvas_draw_dot(c, sx + 3, sy + 3);
        break;
    case 2: // snow, mostly clean
        if((hash & 15) == 5) canvas_draw_dot(c, sx + 2, sy + 1);
        break;
    default: // beach
        if((hash & 7) == 1) {
            canvas_draw_dot(c, sx + 1, sy + 4);
            canvas_draw_dot(c, sx + 4, sy + 2);
        }
        break;
    }
}

static void draw_player_sprite(Canvas* c, int sx, int sy) {
    const char* const* spr;
    switch(h.facing) {
    case 1:
        spr = h.walk_anim ? PLAYER_UP_B : PLAYER_UP_A;
        break;
    case 2:
        spr = PLAYER_LEFT;
        break;
    case 3:
        spr = PLAYER_RIGHT;
        break;
    default:
        spr = h.walk_anim ? PLAYER_DOWN_B : PLAYER_DOWN_A;
        break;
    }
    ui_sprite(c, sx, sy, spr, TILE);
}

static void draw_hud(Canvas* c) {
    canvas_draw_line(c, 0, VIEW_H, SCREEN_W - 1, VIEW_H);
    char buf[24];
    ui_tiny(c, 0, 58, "HP");
    ui_bar(c, 9, 58, 24, 5, h.hp, h.max_hp);
    snprintf(buf, sizeof(buf), "%u/%u", h.hp, h.max_hp);
    ui_tiny(c, 35, 58, buf);
    snprintf(buf, sizeof(buf), "LV%u", h.level);
    ui_tiny(c, 58, 58, buf);
    uint8_t dc = 0;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(h.defeated_mask >> i & 1) dc++;
    }
    snprintf(buf, sizeof(buf), "D%u", dc);
    ui_tiny(c, 75, 58, buf);
    ui_tiny_right(c, SCREEN_W - 1, 58, MAP_NAMES[h.map_idx]);
}

static void draw_overworld(Canvas* c) {
    // Camera follows the player vertically, clamped to the map.
    int cam_y = h.py * TILE + TILE / 2 - VIEW_H / 2;
    const int max_cam = MAP_H * TILE - VIEW_H;
    if(cam_y < 0) cam_y = 0;
    if(cam_y > max_cam) cam_y = max_cam;

    const int first = cam_y / TILE;
    const int last = (cam_y + VIEW_H - 1) / TILE;
    const bool blink = (millis() >> 7) & 1;

    for(int ty = first; ty <= last && ty < MAP_H; ty++) {
        const int sy = ty * TILE - cam_y;
        for(int tx = 0; tx < MAP_W; tx++) {
            const int sx = MAP_X + tx * TILE;
            switch(MAPS[h.map_idx][ty][tx]) {
            case 0:
                draw_floor(c, sx, sy, tx, ty);
                break;
            case 1:
                ui_sprite(c, sx, sy, WALLS[h.map_idx], TILE);
                break;
            case 3:
                ui_sprite(c, sx, sy, SPRING, TILE);
                break;
            case 4:
                ui_sprite(c, sx, sy, blink ? PORTAL_A : PORTAL_B, TILE);
                break;
            default:
                break;
            }
        }
    }

    // A cleared halo keeps the player readable against the busy tiles around it.
    const int player_x = MAP_X + h.px * TILE;
    const int player_y = h.py * TILE - cam_y;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, player_x - 1, player_y > 0 ? player_y - 1 : 0, TILE + 2, TILE + 2);
    canvas_set_color(c, ColorBlack);
    draw_player_sprite(c, player_x, player_y);

    // Rows partly scrolled off the top or bottom must not bleed into the HUD.
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 0, VIEW_H, SCREEN_W, SCREEN_H - VIEW_H);
    canvas_set_color(c, ColorBlack);
    draw_hud(c);

    if(h.flash != FLASH_NONE && ((millis() >> 6) & 1)) {
        canvas_set_color(c, ColorXOR);
        canvas_draw_box(c, 0, 0, SCREEN_W, VIEW_H);
        canvas_set_color(c, ColorBlack);
    }
}

// ---- Battle art -------------------------------------------------------------
static void draw_enemy_sprite(Canvas* c) {
    const int cx = 114;
    const int cy = 11;
    switch(h.enemy_idx) {
    case 0:
    case 1:
    case 7: // shield
        canvas_draw_box(c, cx - 9, cy - 10, 18, 13);
        canvas_draw_box(c, cx - 7, cy + 3, 14, 3);
        canvas_draw_box(c, cx - 4, cy + 6, 8, 2);
        canvas_draw_box(c, cx - 1, cy + 8, 2, 2);
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, cx - 1, cy - 7, 2, 12);
        canvas_draw_box(c, cx - 6, cy - 3, 12, 2);
        canvas_set_color(c, ColorBlack);
        break;
    case 2:
    case 5: // crosshair core
        canvas_draw_frame(c, cx - 8, cy - 8, 16, 16);
        canvas_draw_box(c, cx - 4, cy - 4, 8, 8);
        canvas_draw_box(c, cx - 2, cy - 10, 4, 3);
        canvas_draw_box(c, cx - 2, cy + 7, 4, 3);
        canvas_draw_box(c, cx - 10, cy - 2, 3, 4);
        canvas_draw_box(c, cx + 7, cy - 2, 3, 4);
        break;
    case 3:
    case 8: // castle
        canvas_draw_box(c, cx - 9, cy - 5, 18, 14);
        canvas_draw_box(c, cx - 9, cy - 10, 4, 6);
        canvas_draw_box(c, cx - 2, cy - 10, 4, 6);
        canvas_draw_box(c, cx + 5, cy - 10, 4, 6);
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, cx - 3, cy + 2, 6, 7);
        canvas_set_color(c, ColorBlack);
        break;
    case 4:
    case 6: // watching eye
        for(int i = 0; i < 10; i++) {
            const int w = i * 2 + 2;
            canvas_draw_line(c, cx - w / 2, cy - 10 + i, cx + w / 2 - 1, cy - 10 + i);
        }
        canvas_draw_box(c, cx - 10, cy, 20, 3);
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, cx - 3, cy - 4, 6, 6);
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, cx - 1, cy - 2, 2, 2);
        break;
    default: // brick firewall in flames
        for(int row = 0; row < 4; row++) {
            const int ry = cy - 5 + row * 4;
            canvas_draw_frame(c, cx - 10, ry, 20, 5);
            const int off = (row & 1) ? 5 : 0;
            canvas_draw_line(c, cx - 10 + off, ry, cx - 10 + off, ry + 4);
            canvas_draw_line(c, cx + off, ry, cx + off, ry + 4);
        }
        canvas_draw_box(c, cx - 8, cy - 9, 2, 4);
        canvas_draw_box(c, cx - 3, cy - 11, 2, 6);
        canvas_draw_box(c, cx + 2, cy - 9, 2, 4);
        canvas_draw_box(c, cx + 7, cy - 11, 2, 6);
        break;
    }
}

static const char* const SPR_HACKER[] = {
    "......######........",
    ".....########.......",
    ".....##....##.......",
    ".....##....##.......",
    ".....########.......",
    "....##########......",
    "...############.....",
    "..##############....",
    "..#.##########.#....",
    "..#..........#.#....",
    "....##########......",
    "....#........#......",
    "....#.##.###.#......",
    "....#.#####..#......",
    "....#........#......",
    "...############.....",
    ".################...",
};

static void draw_battle(Canvas* c) {
    const Enemy* e = &ENEMIES[h.enemy_idx];
    char buf[24];

    // Enemy
    ui_tiny(c, 1, 1, e->name);
    snprintf(buf, sizeof(buf), "LV%u", e->max_hp / 5 + 2);
    ui_tiny(c, 76, 1, buf);
    ui_bar(c, 1, 8, 60, 5, h.enemy_hp, e->max_hp);
    snprintf(buf, sizeof(buf), "%u/%u", h.enemy_hp, e->max_hp);
    ui_tiny(c, 64, 8, buf);
    if(!(h.enemy_hurt && (h.anim_frame & 1) == 0)) draw_enemy_sprite(c);

    // Player
    if(!(h.player_hurt && (h.anim_frame & 1) == 0)) ui_sprite(c, 0, 15, SPR_HACKER, 17);
    snprintf(buf, sizeof(buf), "HACKER LV%u", h.level);
    ui_tiny(c, 24, 17, buf);
    ui_bar(c, 24, 24, 50, 5, h.hp, h.max_hp);
    snprintf(buf, sizeof(buf), "%u/%u", h.hp, h.max_hp);
    ui_tiny(c, 77, 24, buf);

    // Messages
    canvas_draw_line(c, 0, 33, SCREEN_W - 1, 33);
    ui_tiny(c, 1, 35, h.msg1);
    ui_tiny(c, 1, 41, h.msg2);
    canvas_draw_line(c, 0, 47, SCREEN_W - 1, 47);

    // Moves in a 2x2 grid
    for(int i = 0; i < MOVES; i++) {
        const int x = (i % 2) * 64;
        const int y = (i < 2) ? 49 : 57;
        const bool sel = (i == h.cursor) && h.phase == 0;
        if(sel) {
            canvas_draw_box(c, x, y - 1, 63, 7);
            canvas_set_color(c, ColorWhite);
        }
        ui_tiny(c, x + 1, y, MOVE_LIST[i].name);
        snprintf(buf, sizeof(buf), "%u", h.move_pp[i]);
        ui_tiny_right(c, x + 61, y, buf);
        canvas_set_color(c, ColorBlack);
    }
}

static void draw_title(Canvas* c) {
    canvas_draw_rframe(c, 4, 1, SCREEN_W - 8, 24, 2);
    ui_center(c, 13, FontPrimary, "HACKER MON");
    ui_tiny_center(c, SCREEN_W / 2, 16, ">> PWN THE NET <<");

    ui_tiny_center(c, SCREEN_W / 2, 28, "BEST RANK:");
    ui_center(c, 43, FontSecondary, rank_name(prefs.pkm_best_rank));
    for(uint8_t i = 0; i < 6; i++) {
        const int x = 41 + i * 8;
        if(i <= prefs.pkm_best_rank) {
            canvas_draw_box(c, x, 45, 6, 3);
        } else {
            canvas_draw_frame(c, x, 45, 6, 3);
        }
    }
    ui_footer(c, "OK PLAY  BACK MENU");
}

static void draw_terminal(Canvas* c) {
    draw_overworld(c);
    ui_dialog_box(c, 12, 6, SCREEN_W - 24, 44);
    ui_tiny_center(c, SCREEN_W / 2, 9, "HEALING SPRING");
    canvas_draw_line(c, 14, 15, SCREEN_W - 15, 15);
    ui_tiny_center(c, SCREEN_W / 2, 18, "YOU DRINK FROM THE");
    ui_tiny_center(c, SCREEN_W / 2, 24, "SPRING...");
    char buf[24];
    snprintf(buf, sizeof(buf), "HP: %u -> %u", h.heal_from, h.max_hp);
    ui_tiny_center(c, SCREEN_W / 2, 32, buf);
    ui_tiny_center(c, SCREEN_W / 2, 41, "OK TO EXIT");
}

static void draw_result(Canvas* c) {
    ui_center(c, 11, FontPrimary, h.result_won ? "PWNED!" : "GAME OVER");
    const uint8_t rank = xp_to_rank(h.xp);
    ui_tiny_center(c, SCREEN_W / 2, 14, rank_name(rank));
    char buf[28];
    snprintf(
        buf,
        sizeof(buf),
        "TIME: %lum %02lus",
        (unsigned long)(h.survived_secs / 60),
        (unsigned long)(h.survived_secs % 60));
    ui_tiny_center(c, SCREEN_W / 2, 22, buf);
    snprintf(buf, sizeof(buf), "LV:%u  XP:%u", h.level, h.xp);
    ui_tiny_center(c, SCREEN_W / 2, 29, buf);
    uint8_t dc = 0;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(h.defeated_mask >> i & 1) dc++;
    }
    snprintf(buf, sizeof(buf), "DEFEATED: %u/%u", dc, MAX_ENEMIES);
    ui_tiny_center(c, SCREEN_W / 2, 36, buf);
    if(rank >= prefs.pkm_best_rank) ui_tiny_center(c, SCREEN_W / 2, 44, "NEW BEST!");
    ui_footer(c, "OK RETRY  BACK MENU");
}

void hackermon_draw(Canvas* c) {
    switch(h.screen) {
    case SCR_TITLE:
        draw_title(c);
        break;
    case SCR_OVERWORLD:
        draw_overworld(c);
        break;
    case SCR_BATTLE:
        draw_battle(c);
        break;
    case SCR_TERMINAL:
        draw_terminal(c);
        break;
    case SCR_RESULT:
        draw_result(c);
        break;
    }
}
