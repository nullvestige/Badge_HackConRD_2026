// The badge's 12 WS2812B ring, shown on the Flipper's single RGB LED: the ring still animates
// virtually with the same themes and effects, and the LED shows one pixel of it.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LED_THEME_COUNT 6
#define LED_SET_COUNT   5

extern const char* const LED_THEME_NAMES[LED_THEME_COUNT];
extern const char* const LED_SET_NAMES[LED_SET_COUNT];

void leds_init(void);
void leds_deinit(void);
void leds_tick(void);

// Fox hunt: color follows RSSI (blue=far, green=near, red=very near).
void leds_set_fox(bool enabled, int16_t rssi_dbm);
// Live brightness preview while editing the setting.
void leds_set_brightness_preview(uint8_t brightness);

// Non-blocking flash burst that overrides the theme, with an optional tone on each flash.
void leds_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t on_ms, uint16_t off_ms, uint8_t count, uint16_t tone_hz);
bool leds_flash_active(void);

// One flash in the UI theme's color, which the 1-bit screen can't show.
void leds_accent_flash(void);
// Port of questAlertEffect(): 4 white flashes with beeps.
void leds_quest_alert(void);
// Port of l5AccessEffect(): green or red bursts.
void leds_access_effect(bool granted);
