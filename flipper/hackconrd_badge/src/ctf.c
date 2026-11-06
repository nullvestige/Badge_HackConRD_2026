// CTFs menu, plus the two roles that deal with the digits of THE KEY: the Bearer, which a badge hunts
// for real, and the KEY screen, which keeps what the Bearer revealed.
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

#define BEARERS 3

static uint8_t found_count(void) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < BEARERS; i++) {
        if(prefs.ctf_bearers & (1 << i)) n++;
    }
    return n;
}

// --- Menu -------------------------------------------------------------------

#define CTF_ROLES 5

static const char* const ROLE_NAMES[CTF_ROLES] = {
    "Bearer",
    "Crowd",
    "THE KEY",
    "THE BOX",
    "Lab Partner",
};
static const ScreenMode ROLE_MODES[CTF_ROLES] = {
    MODE_CTF_BEARER,
    MODE_CTF_CROWD,
    MODE_CTF_KEY,
    MODE_CTF_BOX,
    MODE_CTF_LAB,
};

static uint8_t menu_index;

void ctf_menu_tick(const ButtonsEdge* b) {
    if(b->up) menu_index = (menu_index == 0) ? (CTF_ROLES - 1) : (menu_index - 1);
    if(b->down) menu_index = (menu_index + 1) % CTF_ROLES;
    if(b->ok) {
        go_to(ROLE_MODES[menu_index], false);
        return;
    }
    if(b->back) go_to(MODE_MENU, false);
}

void ctf_menu_draw(Canvas* c) {
    ui_title(c, "CTFs");
    ui_tiny_right(c, SCREEN_W - 2, 2, "FOR YOUR BADGE");
    const uint8_t top = ui_list_top(menu_index, CTF_ROLES, UI_LIST_ROWS);
    char found[6];
    snprintf(found, sizeof(found), "%u/%u", found_count(), BEARERS);
    for(uint8_t r = 0; r < UI_LIST_ROWS; r++) {
        const uint8_t idx = top + r;
        if(idx >= CTF_ROLES) break;
        ui_list_row(c, r, ROLE_NAMES[idx], ROLE_MODES[idx] == MODE_CTF_BEARER ? found : NULL, idx == menu_index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, CTF_ROLES, top);
    ui_footer(c, "OK SELECT  BACK MENU");
}

// --- Bearer -----------------------------------------------------------------
// The badge's hunt only reads RSSI and never confirms a find, so the finder tells the Flipper. At the
// event the bearers were people who handed over one digit pair each.

// Nominal power from TI's CC1101 table for 433 MHz. The hunt's scale was tuned for 1-2 m through a
// backpack, so full power would pin the badge's bar at maximum across a whole house. Measured on a badge
// at about 2 m: MED reads -60 dBm, and MAX reads -40 dBm, already past the top of its -45 scale.
static const uint8_t POWER_PA[CTF_BEARER_POWERS] = {0x0E, 0x34, 0x60, 0xC0}; // -20, -10, 0, +10 dBm
static const char* const POWER_NAMES[CTF_BEARER_POWERS] = {"MIN", "LOW", "MED", "MAX"};

// Nobody may be around to stop a transmitter hidden in a closet.
#define BEARER_MAX_AIR_MS (30UL * 60UL * 1000UL)
#define BEARER_FOUND_HOLD 1500

#define FREQ100_MIN  43000
#define FREQ100_MAX  44000
#define FREQ100_STEP 5

static const StoryPage BEARER_INTRO[] = {
    {"BEARER",
     "HIDE ME",
     "Hide me, then hunt me with your badge. Same frequency on both.",
     "OK START  BACK SKIP",
     true,
     false},
};

typedef enum { BEARER_SETUP, BEARER_ON_AIR } BearerStage;

static struct {
    BearerStage stage;
    uint32_t started;
    bool settings_dirty;
    uint8_t revealing;
    char reveal_title[16];
    char reveal_big[16];
    StoryPage reveal_page;
} bearer;

// The next pair nobody has revealed. Once all are out, the hunt can go on for fun.
static uint8_t bearer_current(void) {
    for(uint8_t i = 0; i < BEARERS; i++) {
        if(!(prefs.ctf_bearers & (1 << i))) return i;
    }
    return 0;
}

static void bearer_save_settings(void) {
    if(!bearer.settings_dirty) return;
    bearer.settings_dirty = false;
    prefs_save();
}

static void bearer_start(void) {
    bearer_save_settings();
    const uint32_t hz = prefs.fox_freq100 * 10000UL;
    if(!radio_freq_tx_allowed(hz) || !radio_carrier_start(hz, POWER_PA[prefs.ctf_bearer_power])) {
        sound_beep(50);
        toast_small("TX REFUSED", 900);
        return;
    }
    leds_set_brightness_preview(0); // a blinking LED would give the hiding place away
    bearer.stage = BEARER_ON_AIR;
    bearer.started = millis();
    input_consume_held(); // this press must not count as the hold that reports a find
}

static void bearer_stop(void) {
    radio_carrier_stop();
    leds_set_brightness_preview(0xFF);
    bearer.stage = BEARER_SETUP;
}

static void bearer_reveal(void) {
    const uint8_t i = bearer_current();
    prefs.ctf_bearers |= (1 << i);
    prefs_save();

    snprintf(bearer.reveal_title, sizeof(bearer.reveal_title), "BEARER %u/%u", i + 1, BEARERS);
    snprintf(
        bearer.reveal_big,
        sizeof(bearer.reveal_big),
        "TRUTH: %c%c",
        key_pair_char(i * 2),
        key_pair_char(i * 2 + 1));
    bearer.reveal_page.title = bearer.reveal_title;
    bearer.reveal_page.big = bearer.reveal_big;
    bearer.reveal_page.text = found_count() >= BEARERS ?
                                  "All truths found. The order is yours to discover." :
                                  "A piece of the code. Remember it and hide me again.";
    bearer.reveal_page.footer = "OK CLOSE";
    bearer.reveal_page.beep = true;
    bearer.reveal_page.is_flag = false;
    leds_quest_alert();
    story_start(&bearer.reveal_page, 1, NULL);
}

void ctf_bearer_enter(bool skip_intro) {
    bearer.stage = BEARER_SETUP;
    if(!skip_intro) story_start(BEARER_INTRO, 1, NULL);
}

void ctf_bearer_exit(void) {
    if(bearer.stage == BEARER_ON_AIR) bearer_stop();
    bearer_save_settings();
}

void ctf_bearer_tick(const ButtonsEdge* b) {
    if(bearer.stage == BEARER_ON_AIR) {
        if(b->back) {
            bearer_stop();
            return;
        }
        if(millis() - bearer.started > BEARER_MAX_AIR_MS) {
            bearer_stop();
            sound_beep(50);
            toast_full("BEARER", "30 MIN: TX OFF", 1500);
            return;
        }
        if(hold_once(InputKeyOk, BEARER_FOUND_HOLD)) {
            bearer_stop();
            bearer_reveal();
        }
        return;
    }

    if(b->back) {
        go_to(MODE_CTF_MENU, false);
        return;
    }
    if(b->ok) {
        bearer_start();
        return;
    }
    if(b->up) {
        prefs.fox_freq100 = (prefs.fox_freq100 >= FREQ100_MAX) ? FREQ100_MIN :
                                                                 prefs.fox_freq100 + FREQ100_STEP;
        bearer.settings_dirty = true;
    }
    if(b->down) {
        prefs.fox_freq100 = (prefs.fox_freq100 <= FREQ100_MIN) ? FREQ100_MAX :
                                                                 prefs.fox_freq100 - FREQ100_STEP;
        bearer.settings_dirty = true;
    }
    if(b->left) {
        prefs.ctf_bearer_power = (prefs.ctf_bearer_power == 0) ? (CTF_BEARER_POWERS - 1) :
                                                                 (prefs.ctf_bearer_power - 1);
        bearer.settings_dirty = true;
    }
    if(b->right) {
        prefs.ctf_bearer_power = (prefs.ctf_bearer_power + 1) % CTF_BEARER_POWERS;
        bearer.settings_dirty = true;
    }
}

static void draw_bearer_setup(Canvas* c) {
    char ln[34];
    if(found_count() >= BEARERS) {
        snprintf(ln, sizeof(ln), "ALL %u FOUND", BEARERS);
    } else {
        snprintf(ln, sizeof(ln), "BEARER %u OF %u  FOUND %u", bearer_current() + 1, BEARERS, found_count());
    }
    ui_title(c, "BEARER");
    ui_tiny(c, 2, 13, ln);

    snprintf(
        ln,
        sizeof(ln),
        "%lu.%02lu MHz",
        (unsigned long)(prefs.fox_freq100 / 100),
        (unsigned long)(prefs.fox_freq100 % 100));
    ui_center(c, 32, FontPrimary, ln);

    if(radio_freq_tx_allowed(prefs.fox_freq100 * 10000UL)) {
        ui_tiny_center(c, SCREEN_W / 2, 35, "U/D: MATCH THE BADGE FREQUENCY");
    } else {
        ui_tiny_center(c, SCREEN_W / 2, 35, "BLOCKED HERE: CHANGE BOTH");
    }
    snprintf(ln, sizeof(ln), "L/R POWER: %s", POWER_NAMES[prefs.ctf_bearer_power]);
    ui_tiny_center(c, SCREEN_W / 2, 44, ln);
    ui_footer(c, "OK TRANSMIT  BACK MENU");
}

static void draw_bearer_on_air(Canvas* c) {
    char ln[34];
    if(found_count() >= BEARERS) {
        strlcpy(ln, "BEARER", sizeof(ln));
    } else {
        snprintf(ln, sizeof(ln), "BEARER %u/%u", bearer_current() + 1, BEARERS);
    }
    ui_title(c, ln);
    ui_status_freq(c);

    ui_center(c, 25, FontPrimary, "ON AIR");
    const uint32_t secs = (millis() - bearer.started) / 1000;
    snprintf(
        ln,
        sizeof(ln),
        "POWER %s   %02lu:%02lu",
        POWER_NAMES[prefs.ctf_bearer_power],
        (unsigned long)(secs / 60),
        (unsigned long)(secs % 60));
    ui_tiny_center(c, SCREEN_W / 2, 29, ln);
    ui_tiny_center(c, SCREEN_W / 2, 38, "HUNT ME WITH THE BADGE");
    ui_tiny_center(c, SCREEN_W / 2, 45, "TAP A BADGE KEY EVERY MINUTE");
    ui_footer(c, "HOLD OK WHEN FOUND  BACK STOP");
}

void ctf_bearer_draw(Canvas* c) {
    if(bearer.stage == BEARER_ON_AIR) {
        draw_bearer_on_air(c);
    } else {
        draw_bearer_setup(c);
    }
}

// --- THE KEY ----------------------------------------------------------------
// No radio here. THE KEY runs on the badge; this keeps the pairs together and says what is left to do.

#define KEY_FORGET_HOLD 1200

void ctf_key_tick(const ButtonsEdge* b) {
    if(b->back) {
        go_to(MODE_CTF_MENU, false);
        return;
    }
    if(prefs.ctf_bearers && hold_once(InputKeyOk, KEY_FORGET_HOLD)) {
        prefs.ctf_bearers = 0;
        prefs_save();
        sound_beep(15);
        toast_small("FORGOTTEN", 800);
    }
}

void ctf_key_draw(Canvas* c) {
    ui_title(c, "THE KEY");
    ui_tiny_right(c, SCREEN_W - 2, 2, "FOR YOUR BADGE");

    for(uint8_t i = 0; i < BEARERS; i++) {
        const int y = UI_CONTENT_Y + 8 + i * 11;
        char name[10];
        char pair[3] = {'?', '?', 0};
        snprintf(name, sizeof(name), "Bearer %u", i + 1);
        if(prefs.ctf_bearers & (1 << i)) {
            pair[0] = key_pair_char(i * 2);
            pair[1] = key_pair_char(i * 2 + 1);
        }
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 3, y, name);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, SCREEN_W - 6, y, AlignRight, AlignBottom, pair);
    }

    if(found_count() >= BEARERS) {
        ui_tiny_center(c, SCREEN_W / 2, 44, "ENTER THEM ON THE BADGE");
        ui_tiny_center(c, SCREEN_W / 2, 50, "ORDER IS RANDOM. TRIES ARE FREE");
    } else {
        ui_tiny_center(c, SCREEN_W / 2, 44, "HUNT THE BEARERS FOR THE DIGITS");
        ui_tiny_center(c, SCREEN_W / 2, 50, "THE BADGE UNLOCKS IT AT FLAG 2");
    }
    ui_footer(c, prefs.ctf_bearers ? "BACK MENU  HOLD OK FORGET" : "BACK MENU");
}
