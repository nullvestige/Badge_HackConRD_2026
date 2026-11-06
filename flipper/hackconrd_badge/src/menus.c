// Main menu, games menu and hunting mode chooser.
#include "features.h"
#include "prefs.h"
#include "ui.h"

extern void lock_now(void);

// --- Main menu --------------------------------------------------------------

static uint8_t menu_index;

static uint8_t menu_count(void) {
    return key_unlocked() ? 8 : 7;
}

static const char* menu_item(uint8_t i) {
    if(i == 0) return "Go Hunting";
    if(i == 1) return "Friends";
    if(i == 2) return "Games";
    if(key_unlocked()) {
        if(i == 3) return "THE KEY";
        if(i == 4) return "THE BOX";
        if(i == 5) return "Learning";
        if(i == 6) return "CTFs";
        if(i == 7) return "Settings";
    } else {
        if(i == 3) return "THE BOX";
        if(i == 4) return "Learning";
        if(i == 5) return "CTFs";
        if(i == 6) return "Settings";
    }
    return "???";
}

void menu_tick(const ButtonsEdge* b) {
    const uint8_t total = menu_count();
    if(menu_index >= total) menu_index = 0;

    if(b->up) menu_index = (menu_index == 0) ? (total - 1) : (menu_index - 1);
    if(b->down) menu_index = (menu_index + 1) % total;

    if(b->ok) {
        if(menu_index == 0) {
            go_to(solo_enabled() ? MODE_HUNT_MENU : MODE_FOX_HUNT, false);
        } else if(menu_index == 1) {
            go_to(MODE_FRIENDS_MENU, false);
        } else if(menu_index == 2) {
            go_to(MODE_GAMES_MENU, false);
        } else if(key_unlocked()) {
            if(menu_index == 3) go_to(MODE_THE_KEY, false);
            if(menu_index == 4) go_to(MODE_THE_BOX, false);
            if(menu_index == 5) go_to(MODE_LEARNING, false);
            if(menu_index == 6) go_to(MODE_CTF_MENU, false);
            if(menu_index == 7) go_to(MODE_SETTINGS, false);
        } else {
            if(menu_index == 3) go_to(MODE_THE_BOX, false);
            if(menu_index == 4) go_to(MODE_LEARNING, false);
            if(menu_index == 5) go_to(MODE_CTF_MENU, false);
            if(menu_index == 6) go_to(MODE_SETTINGS, false);
        }
        return;
    }

    if(hold_once(InputKeyBack, 800)) {
        lock_now();
        return;
    }
    if(b->back_short) request_app_exit();
}

void menu_draw(Canvas* c) {
    ui_title(c, "MAIN MENU");
    ui_status_freq(c);

    const uint8_t total = menu_count();
    const uint8_t top = ui_list_top(menu_index, total, UI_LIST_ROWS);
    for(uint8_t r = 0; r < UI_LIST_ROWS; r++) {
        const uint8_t idx = top + r;
        if(idx >= total) break;
        ui_list_row(c, r, menu_item(idx), NULL, idx == menu_index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, total, top);
    ui_footer(c, "OK SELECT  HOLD BACK LOCK");
}

// --- Games menu -------------------------------------------------------------

static uint8_t games_index;
static const char* const GAMES[] = {"Dino Run", "Doom Maze", "HackerMon", "Star Invaders"};
#define GAMES_COUNT 4

void games_menu_tick(const ButtonsEdge* b) {
    if(b->up) games_index = (games_index == 0) ? (GAMES_COUNT - 1) : (games_index - 1);
    if(b->down) games_index = (games_index + 1) % GAMES_COUNT;
    if(b->ok) {
        static const ScreenMode targets[GAMES_COUNT] = {
            MODE_DINO, MODE_MAZE, MODE_HACKMON, MODE_STAR_INVADERS};
        go_to(targets[games_index], false);
        return;
    }
    if(b->back) go_to(MODE_MENU, false);
}

void games_menu_draw(Canvas* c) {
    ui_title(c, "GAMES");
    for(uint8_t r = 0; r < GAMES_COUNT; r++) {
        ui_list_row(c, r, GAMES[r], NULL, r == games_index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, GAMES_COUNT, 0);
    ui_footer(c, "OK PLAY  BACK MENU");
}

// --- Hunting chooser, shown only in solo mode --------------------------------

static uint8_t hunt_index;

void hunt_menu_tick(const ButtonsEdge* b) {
    if(b->up || b->down) hunt_index ^= 1;
    if(b->ok) {
        go_to(hunt_index == 0 ? MODE_FOX_SIM : MODE_FOX_HUNT, false);
        return;
    }
    if(b->back) go_to(MODE_MENU, false);
}

void hunt_menu_draw(Canvas* c) {
    ui_title(c, "GO HUNTING");
    ui_list_row(c, 0, "Virtual Bearers", NULL, hunt_index == 0);
    ui_list_row(c, 1, "Real Signal (RF)", NULL, hunt_index == 1);
    ui_footer(c, "OK START  BACK MENU");
}
