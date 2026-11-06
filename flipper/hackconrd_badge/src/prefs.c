#include "prefs.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <string.h>

#define PREFS_PATH    APP_DATA_PATH("badge.sav")
#define PREFS_MAGIC   0x26
#define PREFS_VERSION 2

BadgePrefs prefs;

static void prefs_defaults(void) {
    memset(&prefs, 0, sizeof(prefs));
    prefs.role = ROLE_SCRPTK1D;
    prefs.led_brightness = 50;
    prefs.led_theme = 5; // THEME_MATRIX
    prefs.led_set = 0; // LEDSET_DRIP
    prefs.ui_theme = 0; // UI_MATRIX
    prefs.buzzer_on = 1;
    prefs.buzzer_volume = 1;
    prefs.saver = 1; // ID Card
    prefs.solo_mode = 1;
    prefs.fox_freq100 = 43390;
    prefs.ctf_bearer_power = 2;
}

void prefs_load(void) {
    prefs_defaults();
    BadgePrefs loaded;
    if(saved_struct_load(PREFS_PATH, &loaded, sizeof(loaded), PREFS_MAGIC, PREFS_VERSION)) {
        prefs = loaded;
        // Clamp anything a corrupted or hand-edited file could put out of range.
        prefs.nick[sizeof(prefs.nick) - 1] = 0;
        if(prefs.role > ROLE_PWN3D) prefs.role = ROLE_SCRPTK1D;
        if(prefs.led_brightness > 50) prefs.led_brightness = 50;
        if(prefs.fox_freq100 < 43000 || prefs.fox_freq100 > 44000) prefs.fox_freq100 = 43390;
        if(prefs.friends_count > FRIENDS_MAX) prefs.friends_count = FRIENDS_MAX;
        if(prefs.qa_seen_count > QUEST_ALIEN_NEED) prefs.qa_seen_count = QUEST_ALIEN_NEED;
        if(prefs.buzzer_volume > 2) prefs.buzzer_volume = 1;
        if(prefs.ctf_bearer_power >= CTF_BEARER_POWERS) prefs.ctf_bearer_power = 2;
    }
}

void prefs_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/hackconrd_badge"));
    furi_record_close(RECORD_STORAGE);
    saved_struct_save(PREFS_PATH, &prefs, sizeof(prefs), PREFS_MAGIC, PREFS_VERSION);
}

void prefs_factory_reset(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, PREFS_PATH);
    furi_record_close(RECORD_STORAGE);
    prefs_defaults();
}

bool prefs_first_boot(void) {
    return prefs.nick[0] == 0;
}
