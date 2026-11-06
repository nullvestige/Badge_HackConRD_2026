// HackConRD 2026 badge - Flipper Zero port. Shared types and core API.
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BADGE_VERSION "1.0"

// Press edges for one logic tick. `*_short` fire on release unless a hold consumed the press.
typedef struct {
    bool up, down, left, right, ok, back;
    bool ok_short, back_short;
    bool any;
} ButtonsEdge;

typedef enum {
    MODE_BOOT = 0,
    MODE_MENU,
    MODE_HUNT_MENU,
    MODE_FOX_HUNT,
    MODE_FOX_SIM,
    MODE_FRIENDS_MENU,
    MODE_FRIENDS_SCAN,
    MODE_FRIENDS_REQUESTING,
    MODE_FRIENDS_LIST,
    MODE_FRIEND_REQ_PROMPT,
    MODE_THE_BOX,
    MODE_LEARNING,
    MODE_SETTINGS,
    MODE_THE_KEY,
    MODE_FIRST_BOOT,
    MODE_GAMES_MENU,
    MODE_DINO,
    MODE_MAZE,
    MODE_HACKMON,
    MODE_STAR_INVADERS,
    MODE_CTF_MENU,
    MODE_CTF_BEARER,
    MODE_CTF_CROWD,
    MODE_CTF_KEY,
    MODE_CTF_BOX,
    MODE_CTF_LAB,
} ScreenMode;

static inline uint32_t millis(void) {
    return furi_get_tick();
}

static inline uint8_t rand8(void) {
    return (uint8_t)(furi_hal_random_get() & 0xFF);
}

static inline uint32_t rand_u32(void) {
    return furi_hal_random_get();
}

// --- Navigation -----------------------------------------------------------
// Port of goTo(): applies the radio policy of the target mode and draws its entry state.
// skip_intro is set when returning from the lock screen, so screens resume instead of restart.
void go_to(ScreenMode mode, bool skip_intro);
ScreenMode current_mode(void);
void request_app_exit(void);
void note_activity(void);
void reset_idle_timer(void);

// --- Input state ----------------------------------------------------------
bool is_pressed(InputKey key);
// True exactly once per press, when the key has been held for at least hold_ms.
bool hold_once(InputKey key, uint16_t hold_ms);
void input_consume_held(void);

// --- Toasts: short non-blocking overlays that swallow input until they expire.
void toast_full(const char* title, const char* msg, uint16_t ms);
void toast_small(const char* msg, uint16_t ms);

// --- Identity -------------------------------------------------------------
uint16_t self_short_id(void);
