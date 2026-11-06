// Boot splash and lock screen. Artwork lives on the SD card (app assets) and is read on demand,
// since 21 images would otherwise sit in RAM with the rest of the app.
#include "features.h"
#include "prefs.h"
#include "ui.h"

#include <storage/storage.h>

#define ART_SIZE      64
#define ART_BYTES     (ART_SIZE * ART_SIZE / 8)
#define SPONSOR_W     128
#define SPONSOR_H     48
#define SPONSOR_BYTES (SPONSOR_W * SPONSOR_H / 8)

extern void lock_now(void);

// Same order as tools/convert_assets.py.
const char* const SAVER_NAMES[SAVER_COUNT] = {
    "No Limit", "ID Card", "Nika",    "RickMorty", "Bulbasaur", "Sponsors", "Bender",
    "Hackerman", "Im In",  "Zoro",    "Gojo",      "Inosuke",   "Zenitsu",  "Levi",
    "Nanami",   "Homer",   "Kali",    "Arch",      "Parrot",    "Hacker",   "Matrix",
};

static const char* const ROLE_NAMES[5] = {"SCRPTK1D", "H4CK3R", "PHR3AK", "R3DT34M", "PWN3D"};

static uint8_t art[ART_BYTES];
static bool art_valid;
static int16_t art_index = -1;

static uint8_t sponsor_art[SPONSOR_BYTES];
static bool sponsor_valid;

static bool load_file(const char* path, uint8_t* out, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = storage_file_read(file, out, size) == size;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void lock_load_art(uint8_t index) {
    if(index >= SAVER_COUNT) index = 1;
    if(art_valid && art_index == index) return;
    char path[64];
    snprintf(path, sizeof(path), APP_ASSETS_PATH("locks/%02u.bin"), index);
    art_valid = load_file(path, art, sizeof(art));
    art_index = index;
}

void lock_draw_art_preview(Canvas* c, int x, int y) {
    if(art_valid) {
        ui_art(c, x, y, ART_SIZE, ART_SIZE, art);
    } else {
        canvas_draw_frame(c, x, y, ART_SIZE, ART_SIZE);
        ui_tiny_center(c, x + ART_SIZE / 2, y + 28, "NO ART");
    }
}

void lock_draw(Canvas* c) {
    lock_draw_art_preview(c, 0, 0);

    const int x = ART_SIZE + 3;
    ui_tiny(c, x, 2, "HACKCONRD26");
    canvas_draw_line(c, x, 9, SCREEN_W - 2, 9);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, x, 21, prefs.nick[0] ? prefs.nick : "AAAAAAA");

    uint8_t role = prefs.role;
    if(role > 4) role = 0;
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, x, 33, ROLE_NAMES[role]);

    char id[12];
    snprintf(id, sizeof(id), "ID-%04X", (unsigned)self_short_id());
    canvas_draw_str(c, x, 44, id);

    ui_tiny(c, x, 57, "PRESS ANY KEY");
}

// --- Boot sequence: logo, then each sponsor, then first boot or lock screen -------

#define BOOT_LOGO_MS    1400
#define BOOT_SPONSOR_MS 1000

static struct {
    uint8_t stage;
    uint32_t stage_until;
} boot;

static void boot_load_sponsor(uint8_t n) {
    char path[64];
    snprintf(path, sizeof(path), APP_ASSETS_PATH("sponsor%u.bin"), n);
    sponsor_valid = load_file(path, sponsor_art, sizeof(sponsor_art));
}

static void boot_next_stage(void) {
    boot.stage++;
    if(boot.stage <= 3) {
        boot_load_sponsor(boot.stage - 1);
        boot.stage_until = millis() + BOOT_SPONSOR_MS;
        return;
    }
    if(prefs_first_boot()) {
        go_to(MODE_FIRST_BOOT, true);
    } else {
        go_to(MODE_MENU, true);
        lock_now();
    }
}

void boot_enter(void) {
    boot.stage = 0;
    boot.stage_until = millis() + BOOT_LOGO_MS;
    art_valid = load_file(APP_ASSETS_PATH("logo.bin"), art, sizeof(art));
    art_index = -1;
}

void boot_tick(const ButtonsEdge* b) {
    if(b->any || (int32_t)(millis() - boot.stage_until) >= 0) boot_next_stage();
}

void boot_draw(Canvas* c) {
    if(boot.stage == 0) {
        lock_draw_art_preview(c, 0, 0);
        const int x = ART_SIZE + 2;
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, x, 26, "HackCon");
        canvas_draw_str(c, x, 38, "RD 2026");
        ui_tiny(c, x, 50, "BADGE V" BADGE_VERSION);
    } else {
        ui_tiny_center(c, SCREEN_W / 2, 2, "SPONSORED BY");
        // Sponsor art is dark logo on bright background, so it always draws as bright=set.
        if(sponsor_valid) ui_art(c, 0, 12, SPONSOR_W, SPONSOR_H, sponsor_art);
    }
}
