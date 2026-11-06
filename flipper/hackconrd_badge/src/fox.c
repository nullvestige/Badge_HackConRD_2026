// Fox hunting. Real mode measures live RSSI exactly like the badge. Virtual mode (solo only) hides
// three simulated bearers on a grid, since the people who carried the transmitters were at the venue.
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

static const StoryPage FOX_INTRO[] = {
    {"CHAPTER I",
     "THE SIGNAL",
     "At the end of each signal, you will find a BEARER.",
     "OK NEXT  BACK SKIP",
     true,
     false},
    {"BEARERS",
     "NOT TARGETS",
     "They carry a piece of truth. Find them in the venue.",
     "OK START  BACK SKIP",
     false,
     false},
};

static struct {
    float smoothed;
    float peak;
    uint32_t peak_ms;
    int16_t last;
    uint32_t next_poll;
} fox;

static void fox_reset(void) {
    fox.smoothed = -120.0f;
    fox.peak = -120.0f;
    fox.peak_ms = 0;
    fox.last = -120;
    fox.next_poll = 0;
}

static void fox_feed(int16_t rssi) {
    fox.last = rssi;
    const float alpha = 0.25f;
    if(fox.smoothed < -119.0f) fox.smoothed = (float)rssi;
    fox.smoothed = alpha * (float)rssi + (1.0f - alpha) * fox.smoothed;
    if((float)rssi > fox.peak || (millis() - fox.peak_ms > 3000)) {
        fox.peak = (float)rssi;
        fox.peak_ms = millis();
    }
    leds_set_fox(true, rssi);
}

static int clamp_scale(int v) {
    if(v < -105) return -105;
    if(v > -45) return -45;
    return v;
}

static void fox_draw_meter(Canvas* c, int y) {
    const int r = clamp_scale((int)fox.smoothed);
    const int peak = clamp_scale((int)fox.peak);

    ui_bar(c, 2, y, SCREEN_W - 4, 9, r + 105, 60);
    const int px = 3 + ((SCREEN_W - 6) * (peak + 105)) / 60;
    canvas_set_color(c, ColorXOR);
    canvas_draw_line(c, px, y + 1, px, y + 7);
    canvas_set_color(c, ColorBlack);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d dBm", r);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, y + 21, buf);

    snprintf(buf, sizeof(buf), "PEAK:%d", peak);
    ui_tiny_right(c, SCREEN_W - 3, y + 11, buf);

    // The badge's blue/green/red bar colors, in words for the 1-bit screen.
    ui_tiny_right(c, SCREEN_W - 3, y + 17, r > -65 ? "HOT" : (r > -85 ? "WARM" : "COLD"));
}

void fox_exit(void) {
    leds_set_fox(false, 0);
}

// --- Real RF ----------------------------------------------------------------

void fox_enter(bool skip_intro) {
    fox_reset();
    if(!skip_intro) story_start(FOX_INTRO, 2, NULL);
}

void fox_tick(const ButtonsEdge* b) {
    if(b->back) {
        go_to(solo_enabled() ? MODE_HUNT_MENU : MODE_MENU, false);
        return;
    }
    const uint32_t now = millis();
    if((int32_t)(now - fox.next_poll) >= 0) {
        fox.next_poll = now + 60;
        int16_t best = -127;
        for(uint8_t i = 0; i < 4; i++) {
            const int16_t r = radio_rssi_sample();
            if(r > best) best = r;
        }
        fox_feed(best);
    }
}

void fox_draw(Canvas* c) {
    ui_title(c, "HUNTING");
    ui_status_freq(c);
    ui_tiny(c, 2, 13, "LOCATE THE TRANSMITTER");
    fox_draw_meter(c, 21);
    ui_footer(c, "BACK MENU");
}

// --- Virtual bearers (solo mode) ----------------------------------------------

#define SIM_GRID    32
#define SIM_BEARERS 3

static struct {
    int8_t px, py;
    int8_t bx, by;
    uint8_t current;
    uint32_t next_step;
    uint32_t next_sample;
    int16_t rssi;
} sim;

static char sim_title[16];
static char sim_big[16];
static StoryPage sim_page;

static uint8_t sim_found_count(void) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < SIM_BEARERS; i++) {
        if(prefs.bearers_found & (1 << i)) n++;
    }
    return n;
}

static void sim_place_bearer(void) {
    // Next bearer not found yet; once all are found, keep hunting bearer 0 for fun.
    sim.current = 0;
    for(uint8_t i = 0; i < SIM_BEARERS; i++) {
        if(!(prefs.bearers_found & (1 << i))) {
            sim.current = i;
            break;
        }
    }
    do {
        sim.bx = rand8() % SIM_GRID;
        sim.by = rand8() % SIM_GRID;
    } while(abs(sim.bx - sim.px) + abs(sim.by - sim.py) < 16);
}

void fox_sim_enter(bool skip_intro) {
    fox_reset();
    if(!skip_intro) {
        sim.px = rand8() % SIM_GRID;
        sim.py = rand8() % SIM_GRID;
        sim_place_bearer();
        story_start(FOX_INTRO, 2, NULL);
    }
}

static int sim_distance(void) {
    const int dx = sim.px - sim.bx;
    const int dy = sim.py - sim.by;
    // Integer approximation of the euclidean distance, good enough for a signal gradient.
    const int ax = abs(dx);
    const int ay = abs(dy);
    return (ax > ay) ? ax + ay / 2 : ay + ax / 2;
}

void fox_sim_tick(const ButtonsEdge* b) {
    if(b->back) {
        go_to(MODE_HUNT_MENU, false);
        return;
    }
    const uint32_t now = millis();

    if((int32_t)(now - sim.next_step) >= 0) {
        int8_t dx = 0;
        int8_t dy = 0;
        if(is_pressed(InputKeyUp)) dy = -1;
        if(is_pressed(InputKeyDown)) dy = 1;
        if(is_pressed(InputKeyLeft)) dx = -1;
        if(is_pressed(InputKeyRight)) dx = 1;
        if(dx || dy) {
            int8_t nx = sim.px + dx;
            int8_t ny = sim.py + dy;
            if(nx >= 0 && nx < SIM_GRID) sim.px = nx;
            if(ny >= 0 && ny < SIM_GRID) sim.py = ny;
            sim.next_step = now + 140;
        }
    }

    if((int32_t)(now - sim.next_sample) >= 0) {
        sim.next_sample = now + 90;
        const int noise = (int)(rand8() % 5) - 2;
        // Slope chosen so the signal is already readable from almost anywhere on the grid.
        sim.rssi = (int16_t)(-47 - sim_distance() * 2 + noise);
        fox_feed(sim.rssi);
    }

    if(b->ok && sim_distance() <= 1) {
        prefs.bearers_found |= (1 << sim.current);
        prefs_save();

        snprintf(sim_title, sizeof(sim_title), "BEARER %u/3", sim.current + 1);
        snprintf(
            sim_big,
            sizeof(sim_big),
            "TRUTH: %c%c",
            key_pair_char(sim.current * 2),
            key_pair_char(sim.current * 2 + 1));
        sim_page.title = sim_title;
        sim_page.big = sim_big;
        sim_page.text = sim_found_count() >= SIM_BEARERS ?
                            "All truths found. The order is yours to discover." :
                            "A piece of the code. Remember it and keep hunting.";
        sim_page.footer = "OK CLOSE";
        sim_page.beep = true;
        sim_page.is_flag = false;
        leds_quest_alert();
        story_start(&sim_page, 1, NULL);
        sim_place_bearer();
    }
}

void fox_sim_draw(Canvas* c) {
    ui_title(c, "HUNTING");
    char buf[20];
    snprintf(buf, sizeof(buf), "FOUND %u/3", sim_found_count());
    ui_tiny_right(c, SCREEN_W - 2, 2, buf);
    ui_tiny(c, 2, 13, "VIRTUAL SIGNAL: WALK WITH D-PAD");
    fox_draw_meter(c, 21);
    ui_footer(c, sim_distance() <= 1 ? "OK TALK TO BEARER" : "BACK MENU");
}
