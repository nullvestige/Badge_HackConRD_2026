// Persistent settings and progress. Replaces the badge's NVS "hackcon" namespace.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FRIENDS_MAX        32
#define QUEST_ALIEN_NEED   5
#define QUEST_ALIEN_TRIGGER 5
#define CTF_BEARER_POWERS  4

typedef enum {
    ROLE_SCRPTK1D = 0,
    ROLE_H4CK3R = 1,
    ROLE_PHR3AK = 2,
    ROLE_R3DT34M = 3,
    ROLE_PWN3D = 4,
} BadgeRole;

typedef struct {
    uint16_t short_id;
    uint8_t role;
    char nick[9];
} FriendEntry;

typedef struct {
    // Profile
    char nick[8]; // empty == first boot
    uint8_t role;
    // LEDs
    uint8_t led_brightness;
    uint8_t led_theme;
    uint8_t led_set;
    // UI / sound
    uint8_t ui_theme;
    uint8_t buzzer_on;
    uint8_t buzzer_volume;
    uint8_t saver;
    uint8_t solo_mode;
    // Radio
    uint32_t fox_freq100;
    // Quest
    uint8_t key_verified;
    uint8_t qa_armed;
    uint8_t qa_done;
    uint8_t qa_left;
    uint8_t qa_seen_count;
    uint16_t qa_seen[QUEST_ALIEN_NEED];
    uint8_t bearers_found; // bitmask, solo mode only
    // CTFs
    uint8_t ctf_bearers; // bitmask of the pairs the Bearer has revealed
    uint8_t ctf_bearer_power; // index into the Bearer's power table
    // Scores
    uint32_t dino_hi;
    uint32_t si_hi;
    uint8_t pkm_best_rank;
    // Friends
    uint8_t friends_count;
    FriendEntry friends[FRIENDS_MAX];
} BadgePrefs;

extern BadgePrefs prefs;

void prefs_load(void);
void prefs_save(void);
void prefs_factory_reset(void);
bool prefs_first_boot(void);

static inline bool key_unlocked(void) {
    return prefs.qa_done != 0;
}
