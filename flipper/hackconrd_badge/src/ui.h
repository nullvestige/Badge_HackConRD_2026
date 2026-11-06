// Drawing helpers for the 128x64 screen. Replaces the badge's 240x240 ST7789 primitives.
#pragma once

#include "badge.h"

#define SCREEN_W 128
#define SCREEN_H 64

// Standard screen layout: title row, content area, footer row.
#define UI_CONTENT_Y  12
#define UI_FOOTER_Y   58
#define UI_LIST_ROW_H 11
#define UI_LIST_ROWS  4

#define UI_THEME_COUNT 4
extern const char* const UI_THEME_NAMES[UI_THEME_COUNT];

// MATRIX, RED and BLUE draw light-on-dark like the badge's terminal look; WHITE is a normal
// dark-on-light screen. The LCD has no color, so story pages flash the theme's color on the LED.
bool ui_is_dark(void);

void ui_title(Canvas* c, const char* s);
void ui_footer(Canvas* c, const char* s);
void ui_status_freq(Canvas* c);
void ui_center(Canvas* c, int y, Font font, const char* s);

// 3x5 pixel font (uppercase only), 4 px per character. (x, y) is the top-left corner.
void ui_tiny(Canvas* c, int x, int y, const char* s);
void ui_tiny_center(Canvas* c, int cx, int y, const char* s);
void ui_tiny_right(Canvas* c, int right_x, int y, const char* s);
int ui_tiny_width(const char* s);

// Word-wraps text in FontSecondary. Returns the y baseline after the last line.
int ui_wrap(Canvas* c, int x, int y, int max_w, int line_h, const char* text, bool centered);

// Scrolling list. `right` values are optional, pass NULL for plain rows.
uint8_t ui_list_top(uint8_t sel, uint8_t total, uint8_t visible);
void ui_list_row(Canvas* c, uint8_t row, const char* text, const char* right, bool selected);
void ui_scrollbar(Canvas* c, uint8_t visible, uint8_t total, uint8_t top);

void ui_bar(Canvas* c, int x, int y, int w, int h, int value, int max);

// Sprite from strings: '#' draws a pixel, anything else is transparent.
void ui_sprite(Canvas* c, int x, int y, const char* const* rows, int h);

// Draws 1-bit art where a set bit means "bright", correct in both UI themes.
void ui_art(Canvas* c, int x, int y, int w, int h, const uint8_t* bits);

// Modal box for dialogs over the current screen.
void ui_dialog_box(Canvas* c, int x, int y, int w, int h);
