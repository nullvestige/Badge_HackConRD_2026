#include "leds.h"
#include "badge.h"
#include "prefs.h"
#include "sound.h"

#include <math.h>
#include <notification/notification_messages.h>

#define NUM_LEDS        12
#define LED_BRIGHT_MAX  50
#define LED_SHOW_MIN_MS 50
#define IDLE_DIM_MS     10000

typedef struct {
    uint8_t r, g, b;
} Rgb;

enum { THEME_NEON, THEME_RGB, THEME_SUNSET, THEME_ICE, THEME_PARTY, THEME_MATRIX };
enum { LEDSET_DRIP, LEDSET_COMET, LEDSET_SPARK, LEDSET_PULSE, LEDSET_ORBIT };

const char* const LED_THEME_NAMES[LED_THEME_COUNT] = {"NEON", "RGB", "SUNSET", "FROZEN", "PARTY", "MATRIX"};
const char* const LED_SET_NAMES[LED_SET_COUNT] = {"DRIP", "COMET", "SPARK", "PULSE", "ORBIT"};

static const Rgb THEMES[LED_THEME_COUNT][5] = {
    [THEME_NEON] = {{0, 255, 80}, {0, 180, 255}, {180, 0, 255}, {255, 80, 0}, {0, 255, 200}},
    [THEME_RGB] = {{255, 0, 0}, {0, 128, 0}, {0, 0, 255}, {255, 255, 0}, {255, 0, 255}},
    [THEME_SUNSET] = {{255, 40, 0}, {255, 120, 0}, {255, 0, 120}, {140, 0, 255}, {255, 200, 60}},
    [THEME_ICE] = {{0, 120, 255}, {0, 255, 200}, {120, 200, 255}, {0, 60, 160}, {180, 255, 255}},
    [THEME_PARTY] = {{255, 0, 60}, {0, 255, 120}, {0, 120, 255}, {255, 220, 0}, {180, 0, 255}},
    [THEME_MATRIX] = {{0, 255, 0}, {0, 200, 40}, {0, 140, 0}, {0, 255, 90}, {40, 255, 0}},
};

static struct {
    NotificationApp* notifications;
    Rgb ring[NUM_LEDS];
    uint32_t last_step_ms;
    uint8_t pos;
    uint8_t phase;
    uint32_t last_show_ms;
    Rgb shown;
    bool shown_valid;

    bool fox;
    int16_t fox_rssi;
    int16_t preview_brightness; // -1 == none

    Rgb flash;
    uint16_t flash_on_ms, flash_off_ms, flash_tone;
    uint8_t flash_left;
    bool flash_phase_on;
    uint32_t flash_next_ms;
} led;

static NotificationMessage msg_r = {.type = NotificationMessageTypeLedRed, .data.led.value = 0};
static NotificationMessage msg_g = {.type = NotificationMessageTypeLedGreen, .data.led.value = 0};
static NotificationMessage msg_b = {.type = NotificationMessageTypeLedBlue, .data.led.value = 0};
static const NotificationSequence seq_rgb = {&msg_r, &msg_g, &msg_b, &message_do_not_reset, NULL};

static Rgb hsv(uint8_t h) {
    // Full saturation and value, same hue wheel span as FastLED's CHSV for this use.
    uint8_t region = h / 43;
    uint8_t rem = (uint8_t)((h - region * 43) * 6);
    uint8_t q = 255 - rem;
    switch(region) {
    case 0:
        return (Rgb){255, rem, 0};
    case 1:
        return (Rgb){q, 255, 0};
    case 2:
        return (Rgb){0, 255, rem};
    case 3:
        return (Rgb){0, q, 255};
    case 4:
        return (Rgb){rem, 0, 255};
    default:
        return (Rgb){255, 0, q};
    }
}

static Rgb theme_color(uint8_t idx) {
    if(led.fox) {
        // -105 (blue: nothing) -> -85 (green: closer) -> -45 (red: found them)
        int16_t r = led.fox_rssi;
        if(r < -105) r = -105;
        if(r > -45) r = -45;
        uint8_t hue = (uint8_t)(160 - ((r + 105) * 160) / 60);
        return hsv(hue + idx * 5);
    }
    uint8_t theme = prefs.led_theme < LED_THEME_COUNT ? prefs.led_theme : THEME_MATRIX;
    return THEMES[theme][idx % 5];
}

static inline Rgb rgb_scale(Rgb c, uint8_t s) {
    return (Rgb){
        (uint8_t)((c.r * (uint16_t)s) >> 8),
        (uint8_t)((c.g * (uint16_t)s) >> 8),
        (uint8_t)((c.b * (uint16_t)s) >> 8)};
}

static inline uint8_t sat_add(uint8_t a, uint8_t b) {
    uint16_t v = (uint16_t)a + b;
    return v > 255 ? 255 : (uint8_t)v;
}

static inline void ring_add(uint8_t i, Rgb c) {
    led.ring[i].r = sat_add(led.ring[i].r, c.r);
    led.ring[i].g = sat_add(led.ring[i].g, c.g);
    led.ring[i].b = sat_add(led.ring[i].b, c.b);
}

static void ring_dim(uint8_t scale) {
    for(uint8_t i = 0; i < NUM_LEDS; i++) led.ring[i] = rgb_scale(led.ring[i], scale);
}

static bool step_due(uint16_t interval) {
    uint32_t now = millis();
    if(now - led.last_step_ms < interval) return false;
    led.last_step_ms = now;
    return true;
}

static void ring_step(void) {
    switch(prefs.led_set) {
    case LEDSET_COMET:
        if(!step_due(80)) return;
        ring_dim(200);
        ring_add(led.pos, rgb_scale(theme_color(0), 200));
        ring_add((led.pos + NUM_LEDS / 2) % NUM_LEDS, rgb_scale(theme_color(2), 170));
        led.pos = (led.pos + 1) % NUM_LEDS;
        break;
    case LEDSET_SPARK:
        if(!step_due(105)) return;
        ring_dim(220);
        ring_add(rand8() % NUM_LEDS, rgb_scale(theme_color(rand8() % 5), 200));
        break;
    case LEDSET_PULSE: {
        if(!step_due(85)) return;
        uint8_t v = (uint8_t)(127.5f * (1.0f + sinf((float)led.phase * 2.0f * (float)M_PI / 256.0f)));
        led.phase += 8;
        for(uint8_t i = 0; i < NUM_LEDS; i++) {
            uint8_t g = (i % 3 == 0) ? v : (v / 4);
            led.ring[i] = rgb_scale(theme_color(i), g);
        }
        break;
    }
    case LEDSET_ORBIT:
        if(!step_due(80)) return;
        ring_dim(205);
        ring_add(led.pos, rgb_scale(theme_color(1), 210));
        ring_add((led.pos + NUM_LEDS / 2) % NUM_LEDS, rgb_scale(theme_color(4), 170));
        if((led.pos & 0x03) == 0) {
            for(uint8_t i = 0; i < NUM_LEDS; i += 3) ring_add(i, rgb_scale(theme_color(3), 90));
        }
        led.pos = (led.pos + 1) % NUM_LEDS;
        break;
    case LEDSET_DRIP:
    default:
        if(!step_due(90)) return;
        ring_dim(210);
        ring_add(led.pos, rgb_scale(theme_color(0), 220));
        ring_add((led.pos + 3) % NUM_LEDS, rgb_scale(theme_color(0), 80));
        led.pos = (led.pos + 1) % NUM_LEDS;
        break;
    }
}

extern uint32_t idle_elapsed_ms(void);

static uint8_t effective_brightness(void) {
    if(led.preview_brightness >= 0) return (uint8_t)led.preview_brightness;
    uint8_t b = prefs.led_brightness;
    if(idle_elapsed_ms() > IDLE_DIM_MS) {
        if(b >= 30) return 20;
        if(b >= 20) return 10;
    }
    return b;
}

static void show(Rgb c, uint8_t brightness) {
    uint32_t now = millis();
    if(now - led.last_show_ms < LED_SHOW_MIN_MS) return;

    uint16_t scale = (uint16_t)brightness * 255 / LED_BRIGHT_MAX;
    Rgb out = {
        (uint8_t)((c.r * scale) / 255),
        (uint8_t)((c.g * scale) / 255),
        (uint8_t)((c.b * scale) / 255)};
    if(led.shown_valid && out.r == led.shown.r && out.g == led.shown.g && out.b == led.shown.b) return;

    led.last_show_ms = now;
    led.shown = out;
    led.shown_valid = true;
    msg_r.data.led.value = out.r;
    msg_g.data.led.value = out.g;
    msg_b.data.led.value = out.b;
    notification_message(led.notifications, &seq_rgb);
}

void leds_init(void) {
    memset(&led, 0, sizeof(led));
    led.preview_brightness = -1;
    led.notifications = furi_record_open(RECORD_NOTIFICATION);
}

void leds_deinit(void) {
    notification_message(led.notifications, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);
    led.notifications = NULL;
}

void leds_tick(void) {
    if(led.flash_left > 0) {
        uint32_t now = millis();
        if((int32_t)(now - led.flash_next_ms) >= 0) {
            if(led.flash_phase_on) {
                led.flash_phase_on = false;
                led.flash_next_ms = now + led.flash_off_ms;
                led.flash_left--;
            } else {
                led.flash_phase_on = true;
                led.flash_next_ms = now + led.flash_on_ms;
                if(led.flash_tone) sound_tone(led.flash_tone, led.flash_on_ms);
            }
        }
        led.last_show_ms = 0;
        show(led.flash_phase_on ? led.flash : (Rgb){0, 0, 0}, LED_BRIGHT_MAX);
        if(led.flash_left == 0) led.shown_valid = false;
        return;
    }

    ring_step();
    show(led.ring[0], effective_brightness());
}

void leds_set_fox(bool enabled, int16_t rssi_dbm) {
    led.fox = enabled;
    led.fox_rssi = rssi_dbm;
}

void leds_set_brightness_preview(uint8_t brightness) {
    led.preview_brightness = (brightness == 0xFF) ? -1 : brightness;
}

void leds_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t on_ms, uint16_t off_ms, uint8_t count, uint16_t tone_hz) {
    led.flash = (Rgb){r, g, b};
    led.flash_on_ms = on_ms;
    led.flash_off_ms = off_ms;
    led.flash_tone = tone_hz;
    led.flash_left = count;
    led.flash_phase_on = false;
    led.flash_next_ms = millis();
}

bool leds_flash_active(void) {
    return led.flash_left > 0;
}

void leds_accent_flash(void) {
    static const Rgb ACCENT[4] = {{0, 255, 0}, {255, 0, 0}, {0, 80, 255}, {255, 255, 255}};
    const Rgb a = ACCENT[prefs.ui_theme % 4];
    leds_flash(a.r, a.g, a.b, 120, 40, 1, 0);
}

void leds_quest_alert(void) {
    leds_flash(255, 255, 255, 80, 80, 4, 1000);
}

void leds_access_effect(bool granted) {
    if(granted) {
        leds_flash(0, 255, 0, 60, 60, 3, 1200);
    } else {
        leds_flash(255, 0, 0, 120, 60, 3, 400);
    }
}
