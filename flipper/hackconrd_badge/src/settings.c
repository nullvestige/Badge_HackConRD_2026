// Settings and the first-boot nickname editor.
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "radio.h"
#include "sound.h"
#include "ui.h"

extern void lock_now(void);

enum {
    S_NICK,
    S_BADGE_ROLE,
    S_FREQUENCY,
    S_UI_THEME,
    S_LED_BRIGHT,
    S_LED_THEME,
    S_LED_SET,
    S_BUZZER,
    S_BUZZER_VOL,
    S_SAVER,
    S_SOLO,
    S_RADIO_INFO,
    S_FACTORY_RESET,
    S_COUNT
};

static const char* const TITLES[S_COUNT] = {
    "Nickname",
    "Badge Role",
    "Frequency",
    "UI Theme",
    "LED Bright",
    "LED Theme",
    "LED Set",
    "Buzzer",
    "Buzzer Volume",
    "Lock Screen",
    "Solo Mode",
    "Radio Info",
    "Factory Reset",
};

static const char* const ROLE_NAMES[5] = {"SCRPTK1D", "H4CK3R", "PHR3AK", "R3DT34M", "PWN3D"};
static const char* const VOL_NAMES[3] = {"Low", "Medium", "High"};
static const char NICK_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

static struct {
    uint8_t index;
    bool editing;
    int32_t value;
    char nick[8];
    uint8_t nick_pos;
    uint8_t factory_ok_count;
    uint32_t factory_ok_until;
} set;

// --- Nickname editor, shared with first boot --------------------------------------

static char nick_next(char ch, int dir) {
    const int n = (int)sizeof(NICK_CHARS) - 1;
    int idx = 0;
    for(int i = 0; i < n; i++) {
        if(NICK_CHARS[i] == ch) {
            idx = i;
            break;
        }
    }
    idx += dir;
    if(idx < 0) idx = n - 1;
    if(idx >= n) idx = 0;
    return NICK_CHARS[idx];
}

static void nick_begin(const char* from) {
    memset(set.nick, ' ', 7);
    set.nick[7] = 0;
    for(uint8_t i = 0; i < 7 && from[i]; i++) set.nick[i] = from[i];
    set.nick_pos = 0;
}

static void nick_input(const ButtonsEdge* b) {
    if(b->up) set.nick[set.nick_pos] = nick_next(set.nick[set.nick_pos], +1);
    if(b->down) set.nick[set.nick_pos] = nick_next(set.nick[set.nick_pos], -1);
    if(b->right || b->ok_short) set.nick_pos = (set.nick_pos + 1) % 7;
    if(b->left) set.nick_pos = (set.nick_pos == 0) ? 6 : (set.nick_pos - 1);
}

static void nick_commit(void) {
    char tmp[8];
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, set.nick, 7);
    for(int i = 6; i >= 0; i--) {
        if(tmp[i] == ' ') {
            tmp[i] = 0;
        } else {
            break;
        }
    }
    if(tmp[0] == 0) strlcpy(tmp, "ANON", sizeof(tmp));
    memset(prefs.nick, 0, sizeof(prefs.nick));
    strlcpy(prefs.nick, tmp, sizeof(prefs.nick));
    prefs_save();
}

static void nick_draw(Canvas* c, const char* title, const char* hint, const char* footer) {
    ui_title(c, title);
    ui_tiny(c, 2, 13, hint);
    canvas_draw_rframe(c, 4, 21, SCREEN_W - 8, 28, 2);
    canvas_set_font(c, FontPrimary);
    const int step = 16;
    const int base = (SCREEN_W - 7 * step) / 2 + 5;
    for(uint8_t i = 0; i < 7; i++) {
        char s[2] = {set.nick[i], 0};
        canvas_draw_str(c, base + i * step, 39, s);
    }
    canvas_draw_box(c, base - 1 + set.nick_pos * step, 42, 9, 2);
    ui_footer(c, footer);
}

void first_boot_enter(void) {
    nick_begin("AAAAAAA");
}

void first_boot_tick(const ButtonsEdge* b) {
    nick_input(b);
    if(hold_once(InputKeyOk, 900)) {
        nick_commit();
        sound_beep(15);
        go_to(MODE_MENU, true);
        lock_now();
    }
}

void first_boot_draw(Canvas* c) {
    nick_draw(c, "SET YOUR NICK", "HOLD OK TO SAVE (REQUIRED)", "U/D EDIT  L/R MOVE");
}

// --- Settings ---------------------------------------------------------------------

static void value_string(uint8_t i, char* out, size_t len) {
    out[0] = 0;
    switch(i) {
    case S_NICK:
        strlcpy(out, prefs.nick, len);
        break;
    case S_BADGE_ROLE:
        strlcpy(out, ROLE_NAMES[prefs.role > 4 ? 0 : prefs.role], len);
        break;
    case S_FREQUENCY:
        snprintf(
            out,
            len,
            "%lu.%02lu",
            (unsigned long)(prefs.fox_freq100 / 100),
            (unsigned long)(prefs.fox_freq100 % 100));
        break;
    case S_UI_THEME:
        strlcpy(out, UI_THEME_NAMES[prefs.ui_theme % UI_THEME_COUNT], len);
        break;
    case S_LED_BRIGHT:
        snprintf(out, len, "%u", prefs.led_brightness);
        break;
    case S_LED_THEME:
        strlcpy(out, LED_THEME_NAMES[prefs.led_theme % LED_THEME_COUNT], len);
        break;
    case S_LED_SET:
        strlcpy(out, LED_SET_NAMES[prefs.led_set % LED_SET_COUNT], len);
        break;
    case S_BUZZER:
        strlcpy(out, prefs.buzzer_on ? "ON" : "OFF", len);
        break;
    case S_BUZZER_VOL:
        strlcpy(out, VOL_NAMES[prefs.buzzer_volume], len);
        break;
    case S_SAVER:
        strlcpy(out, SAVER_NAMES[prefs.saver % SAVER_COUNT], len);
        break;
    case S_SOLO:
        strlcpy(out, prefs.solo_mode ? "ON" : "OFF", len);
        break;
    default:
        break;
    }
}

void settings_enter(void) {
    set.editing = false;
}

static void edit_begin(void) {
    set.editing = true;
    switch(set.index) {
    case S_NICK:
        nick_begin(prefs.nick);
        break;
    case S_BADGE_ROLE:
        set.value = prefs.role;
        break;
    case S_FREQUENCY:
        set.value = (int32_t)prefs.fox_freq100;
        break;
    case S_UI_THEME:
        set.value = prefs.ui_theme;
        break;
    case S_LED_BRIGHT:
        set.value = prefs.led_brightness;
        break;
    case S_LED_THEME:
        set.value = prefs.led_theme;
        break;
    case S_LED_SET:
        set.value = prefs.led_set;
        break;
    case S_BUZZER:
        set.value = prefs.buzzer_on;
        break;
    case S_BUZZER_VOL:
        set.value = prefs.buzzer_volume;
        break;
    case S_SAVER:
        set.value = prefs.saver;
        lock_load_art((uint8_t)set.value);
        break;
    case S_SOLO:
        set.value = prefs.solo_mode;
        break;
    case S_FACTORY_RESET:
        set.factory_ok_count = 0;
        break;
    default:
        break;
    }
}

static void edit_cycle(int dir, int count) {
    set.value += dir;
    if(set.value < 0) set.value = count - 1;
    if(set.value >= count) set.value = 0;
}

static void edit_commit(void) {
    switch(set.index) {
    case S_BADGE_ROLE:
        prefs.role = (uint8_t)set.value;
        break;
    case S_FREQUENCY:
        prefs.fox_freq100 = (uint32_t)set.value;
        break;
    case S_UI_THEME:
        prefs.ui_theme = (uint8_t)set.value;
        break;
    case S_LED_BRIGHT:
        prefs.led_brightness = (uint8_t)set.value;
        leds_set_brightness_preview(0xFF);
        break;
    case S_LED_THEME:
        prefs.led_theme = (uint8_t)set.value;
        break;
    case S_LED_SET:
        prefs.led_set = (uint8_t)set.value;
        break;
    case S_BUZZER:
        prefs.buzzer_on = (uint8_t)set.value;
        break;
    case S_BUZZER_VOL:
        prefs.buzzer_volume = (uint8_t)set.value;
        break;
    case S_SAVER:
        prefs.saver = (uint8_t)set.value;
        break;
    case S_SOLO:
        prefs.solo_mode = (uint8_t)set.value;
        break;
    default:
        break;
    }
    prefs_save();
    sound_beep(15);
    set.editing = false;
}

static void edit_tick(const ButtonsEdge* b) {
    const int dir = b->up ? 1 : (b->down ? -1 : 0);

    switch(set.index) {
    case S_NICK:
        nick_input(b);
        if(hold_once(InputKeyOk, 900)) {
            nick_commit();
            sound_beep(15);
            set.editing = false;
        }
        if(b->back) set.editing = false;
        return;

    case S_RADIO_INFO:
        if(b->back || b->ok) set.editing = false;
        return;

    case S_FACTORY_RESET: {
        const uint32_t now = millis();
        if(set.factory_ok_count == 1 && (int32_t)(now - set.factory_ok_until) >= 0) {
            set.factory_ok_count = 0;
        }
        if(b->ok) {
            if(set.factory_ok_count == 0) {
                set.factory_ok_count = 1;
                set.factory_ok_until = now + 1800;
                sound_beep(12);
            } else {
                prefs_factory_reset();
                set.editing = false;
                go_to(MODE_FIRST_BOOT, true);
                return;
            }
        }
        if(b->back) set.editing = false;
        return;
    }

    case S_FREQUENCY:
        if(dir > 0) {
            set.value += 5;
            if(set.value > 44000) set.value = 43000;
        } else if(dir < 0) {
            set.value = (set.value <= 43000) ? 44000 : set.value - 5;
        }
        break;

    case S_LED_BRIGHT:
        if(dir != 0) {
            set.value += dir * 10;
            if(set.value > 50) set.value = 50;
            if(set.value < 0) set.value = 0;
            leds_set_brightness_preview((uint8_t)set.value);
        }
        break;

    case S_BADGE_ROLE:
        if(dir) edit_cycle(dir, 5);
        break;
    case S_UI_THEME:
        if(dir) edit_cycle(dir, UI_THEME_COUNT);
        break;
    case S_LED_THEME:
        if(dir) edit_cycle(dir, LED_THEME_COUNT);
        break;
    case S_LED_SET:
        if(dir) edit_cycle(dir, LED_SET_COUNT);
        break;
    case S_BUZZER:
    case S_SOLO:
        if(dir) set.value = !set.value;
        break;
    case S_BUZZER_VOL:
        if(dir) edit_cycle(dir, 3);
        break;
    case S_SAVER:
        if(dir) {
            edit_cycle(dir, SAVER_COUNT);
            lock_load_art((uint8_t)set.value);
        }
        break;
    default:
        break;
    }

    if(b->ok) {
        edit_commit();
    } else if(b->back) {
        if(set.index == S_LED_BRIGHT) leds_set_brightness_preview(0xFF);
        set.editing = false;
    }
}

void settings_tick(const ButtonsEdge* b) {
    if(set.editing) {
        edit_tick(b);
        return;
    }
    if(b->up) set.index = (set.index == 0) ? (S_COUNT - 1) : (set.index - 1);
    if(b->down) set.index = (set.index + 1) % S_COUNT;
    if(b->ok) {
        edit_begin();
        return;
    }
    if(b->back) go_to(MODE_MENU, false);
}

static void edit_value_string(char* out, size_t len) {
    out[0] = 0;
    switch(set.index) {
    case S_BADGE_ROLE:
        strlcpy(out, ROLE_NAMES[set.value], len);
        break;
    case S_FREQUENCY:
        snprintf(out, len, "%ld.%02ld MHz", (long)(set.value / 100), (long)(set.value % 100));
        break;
    case S_UI_THEME:
        strlcpy(out, UI_THEME_NAMES[set.value], len);
        break;
    case S_LED_BRIGHT:
        snprintf(out, len, "%ld", (long)set.value);
        break;
    case S_LED_THEME:
        strlcpy(out, LED_THEME_NAMES[set.value], len);
        break;
    case S_LED_SET:
        strlcpy(out, LED_SET_NAMES[set.value], len);
        break;
    case S_BUZZER:
    case S_SOLO:
        strlcpy(out, set.value ? "ON" : "OFF", len);
        break;
    case S_BUZZER_VOL:
        strlcpy(out, VOL_NAMES[set.value], len);
        break;
    default:
        break;
    }
}

static const char* edit_hint(void) {
    switch(set.index) {
    case S_BADGE_ROLE:
        return "CHOOSE YOUR ROLE";
    case S_FREQUENCY:
        return "STEP: 0.05 MHZ";
    case S_UI_THEME:
        return "WHITE=LIGHT, OTHERS=DARK";
    case S_LED_BRIGHT:
        return "UP/DOWN SET (10)";
    case S_LED_THEME:
        return "PREFERRED LED THEME";
    case S_LED_SET:
        return "PREFERRED LED SET";
    case S_BUZZER:
        return "SILENT IF FLIPPER IS STEALTH";
    case S_BUZZER_VOL:
        return "PREFERRED BUZZER VOLUME";
    case S_SOLO:
        return "SIMULATE BADGES AND BEARERS";
    default:
        return "";
    }
}

static void draw_radio_info(Canvas* c) {
    ui_title(c, "RADIO INFO");
    char ln[34];
    snprintf(ln, sizeof(ln), "REGION: %s", furi_hal_region_get_name());
    ui_tiny(c, 2, 14, ln);
    snprintf(ln, sizeof(ln), "433.00 TX: %s", radio_freq_tx_allowed(433000000UL) ? "ALLOWED" : "BLOCKED");
    ui_tiny(c, 2, 22, ln);
    const uint32_t hz = radio_packet_freq_hz();
    snprintf(ln, sizeof(ln), "PACKET CHANNEL: %lu.%02lu", (unsigned long)(hz / 1000000UL), (unsigned long)((hz / 10000UL) % 100));
    ui_tiny(c, 2, 30, ln);
    snprintf(ln, sizeof(ln), "TX REFUSED: %lu", (unsigned long)radio_tx_blocked_count());
    ui_tiny(c, 2, 38, ln);
    snprintf(ln, sizeof(ln), "BADGE ID: %04X", (unsigned)self_short_id());
    ui_tiny(c, 2, 46, ln);
    ui_footer(c, "BACK RETURN");
}

static void draw_edit(Canvas* c) {
    switch(set.index) {
    case S_NICK:
        nick_draw(c, "NICKNAME", "HOLD OK SAVE  BACK CANCEL", "U/D EDIT  L/R MOVE");
        return;
    case S_RADIO_INFO:
        draw_radio_info(c);
        return;
    case S_FACTORY_RESET:
        ui_title(c, "FACTORY RST");
        ui_tiny(c, 2, 14, "THIS ERASES ALL PROGRESS!");
        canvas_draw_rframe(c, 8, 22, SCREEN_W - 16, 28, 2);
        ui_center(c, 36, FontPrimary, set.factory_ok_count ? "ONCE MORE" : "DOUBLE OK");
        ui_tiny_center(c, SCREEN_W / 2, 40, "TO CONFIRM");
        ui_footer(c, "OK TWICE  BACK CANCEL");
        return;
    case S_SAVER:
        lock_draw_art_preview(c, 0, 0);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 67, 12, "LOCK");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 67, 28, SAVER_NAMES[set.value]);
        ui_tiny(c, 67, 44, "U/D CHANGE");
        ui_tiny(c, 67, 51, "OK SAVE");
        ui_tiny(c, 67, 58, "BACK CANCEL");
        return;
    default:
        break;
    }

    ui_title(c, TITLES[set.index]);
    ui_tiny(c, 2, 14, edit_hint());

    char v[20];
    edit_value_string(v, sizeof(v));
    canvas_draw_rframe(c, 8, 22, SCREEN_W - 16, 26, 2);
    ui_center(c, 40, FontPrimary, v);
    if(set.index == S_LED_BRIGHT) ui_bar(c, 12, 43, SCREEN_W - 24, 4, set.value, 50);
    ui_footer(c, "U/D EDIT  OK SAVE  BACK CANCEL");
}

void settings_draw(Canvas* c) {
    if(set.editing) {
        draw_edit(c);
        return;
    }
    ui_title(c, "SETTINGS");
    ui_status_freq(c);

    const uint8_t top = ui_list_top(set.index, S_COUNT, UI_LIST_ROWS);
    for(uint8_t r = 0; r < UI_LIST_ROWS; r++) {
        const uint8_t idx = top + r;
        if(idx >= S_COUNT) break;
        char v[16];
        value_string(idx, v, sizeof(v));
        ui_list_row(c, r, TITLES[idx], v[0] ? v : NULL, idx == set.index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, S_COUNT, top);
    ui_footer(c, "OK EDIT  BACK RETURN");
}
