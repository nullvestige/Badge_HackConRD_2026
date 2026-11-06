#include "ui.h"
#include "prefs.h"

const char* const UI_THEME_NAMES[UI_THEME_COUNT] = {"MATRIX", "RED", "BLUE", "WHITE"};

#define G(a, b, c, d, e) (uint16_t)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e))

// 3x5 glyphs for ASCII 32..95. Each row is 3 bits, MSB on the left.
static const uint16_t TINY_FONT[64] = {
    G(0, 0, 0, 0, 0), // space
    G(2, 2, 2, 0, 2), // !
    G(5, 5, 0, 0, 0), // "
    G(5, 7, 5, 7, 5), // #
    G(3, 6, 2, 3, 6), // $
    G(5, 1, 2, 4, 5), // %
    G(2, 5, 2, 5, 3), // &
    G(2, 2, 0, 0, 0), // '
    G(1, 2, 2, 2, 1), // (
    G(4, 2, 2, 2, 4), // )
    G(0, 5, 2, 5, 0), // *
    G(0, 2, 7, 2, 0), // +
    G(0, 0, 0, 2, 4), // ,
    G(0, 0, 7, 0, 0), // -
    G(0, 0, 0, 0, 2), // .
    G(1, 1, 2, 4, 4), // /
    G(7, 5, 5, 5, 7), // 0
    G(2, 6, 2, 2, 7), // 1
    G(7, 1, 7, 4, 7), // 2
    G(7, 1, 7, 1, 7), // 3
    G(5, 5, 7, 1, 1), // 4
    G(7, 4, 7, 1, 7), // 5
    G(7, 4, 7, 5, 7), // 6
    G(7, 1, 2, 2, 2), // 7
    G(7, 5, 7, 5, 7), // 8
    G(7, 5, 7, 1, 7), // 9
    G(0, 2, 0, 2, 0), // :
    G(0, 2, 0, 2, 4), // ;
    G(1, 2, 4, 2, 1), // <
    G(0, 7, 0, 7, 0), // =
    G(4, 2, 1, 2, 4), // >
    G(7, 1, 3, 0, 2), // ?
    G(7, 5, 7, 4, 7), // @
    G(2, 5, 7, 5, 5), // A
    G(6, 5, 6, 5, 6), // B
    G(3, 4, 4, 4, 3), // C
    G(6, 5, 5, 5, 6), // D
    G(7, 4, 6, 4, 7), // E
    G(7, 4, 6, 4, 4), // F
    G(3, 4, 5, 5, 3), // G
    G(5, 5, 7, 5, 5), // H
    G(7, 2, 2, 2, 7), // I
    G(1, 1, 1, 5, 2), // J
    G(5, 5, 6, 5, 5), // K
    G(4, 4, 4, 4, 7), // L
    G(5, 7, 7, 5, 5), // M
    G(6, 5, 5, 5, 5), // N
    G(2, 5, 5, 5, 2), // O
    G(6, 5, 6, 4, 4), // P
    G(2, 5, 5, 6, 3), // Q
    G(6, 5, 6, 5, 5), // R
    G(3, 4, 2, 1, 6), // S
    G(7, 2, 2, 2, 2), // T
    G(5, 5, 5, 5, 7), // U
    G(5, 5, 5, 5, 2), // V
    G(5, 5, 7, 7, 5), // W
    G(5, 5, 2, 5, 5), // X
    G(5, 5, 2, 2, 2), // Y
    G(7, 1, 2, 4, 7), // Z
    G(6, 4, 4, 4, 6), // [
    G(4, 4, 2, 1, 1), // backslash
    G(3, 1, 1, 1, 3), // ]
    G(2, 5, 0, 0, 0), // ^
    G(0, 0, 0, 0, 7), // _
};

bool ui_is_dark(void) {
    return prefs.ui_theme != 3; // WHITE
}

void ui_tiny(Canvas* c, int x, int y, const char* s) {
    for(; *s; s++, x += 4) {
        char ch = *s;
        if(ch >= 'a' && ch <= 'z') ch -= 32;
        if(ch < 32 || ch > 95) continue;
        const uint16_t g = TINY_FONT[ch - 32];
        for(int row = 0; row < 5; row++) {
            const uint8_t bits = (g >> (12 - row * 3)) & 7;
            if(bits & 4) canvas_draw_dot(c, x, y + row);
            if(bits & 2) canvas_draw_dot(c, x + 1, y + row);
            if(bits & 1) canvas_draw_dot(c, x + 2, y + row);
        }
    }
}

int ui_tiny_width(const char* s) {
    return (int)strlen(s) * 4 - 1;
}

void ui_tiny_center(Canvas* c, int cx, int y, const char* s) {
    ui_tiny(c, cx - ui_tiny_width(s) / 2, y, s);
}

void ui_tiny_right(Canvas* c, int right_x, int y, const char* s) {
    ui_tiny(c, right_x - ui_tiny_width(s) + 1, y, s);
}

void ui_title(Canvas* c, const char* s) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 8, s);
    canvas_draw_line(c, 0, 10, SCREEN_W - 1, 10);
}

void ui_footer(Canvas* c, const char* s) {
    canvas_draw_line(c, 0, 56, SCREEN_W - 1, 56);
    ui_tiny(c, 1, UI_FOOTER_Y, s);
}

void ui_status_freq(Canvas* c) {
    char buf[12];
    snprintf(
        buf,
        sizeof(buf),
        "%lu.%02lu",
        (unsigned long)(prefs.fox_freq100 / 100),
        (unsigned long)(prefs.fox_freq100 % 100));
    ui_tiny_right(c, SCREEN_W - 2, 2, buf);
}

void ui_center(Canvas* c, int y, Font font, const char* s) {
    canvas_set_font(c, font);
    canvas_draw_str_aligned(c, SCREEN_W / 2, y, AlignCenter, AlignBottom, s);
}

int ui_wrap(Canvas* c, int x, int y, int max_w, int line_h, const char* text, bool centered) {
    canvas_set_font(c, FontSecondary);
    char line[48];
    size_t line_len = 0;
    const char* p = text;

    while(*p) {
        const char* word = p;
        while(*p && *p != ' ') p++;
        size_t word_len = p - word;
        while(*p == ' ') p++;

        char candidate[48];
        size_t cand_len = line_len;
        memcpy(candidate, line, line_len);
        if(cand_len && cand_len < sizeof(candidate) - 1) candidate[cand_len++] = ' ';
        size_t copy = word_len;
        if(cand_len + copy > sizeof(candidate) - 1) copy = sizeof(candidate) - 1 - cand_len;
        memcpy(candidate + cand_len, word, copy);
        cand_len += copy;
        candidate[cand_len] = 0;

        if(canvas_string_width(c, candidate) > max_w && line_len > 0) {
            line[line_len] = 0;
            if(centered) {
                canvas_draw_str_aligned(c, x + max_w / 2, y, AlignCenter, AlignBottom, line);
            } else {
                canvas_draw_str(c, x, y, line);
            }
            y += line_h;
            line_len = (word_len > sizeof(line) - 1) ? sizeof(line) - 1 : word_len;
            memcpy(line, word, line_len);
        } else {
            memcpy(line, candidate, cand_len);
            line_len = cand_len;
        }
    }
    if(line_len) {
        line[line_len] = 0;
        if(centered) {
            canvas_draw_str_aligned(c, x + max_w / 2, y, AlignCenter, AlignBottom, line);
        } else {
            canvas_draw_str(c, x, y, line);
        }
        y += line_h;
    }
    return y;
}

uint8_t ui_list_top(uint8_t sel, uint8_t total, uint8_t visible) {
    if(total <= visible || sel < visible / 2) return 0;
    int desired = (int)sel - visible / 2;
    int max_top = (int)total - visible;
    if(desired > max_top) desired = max_top;
    return desired < 0 ? 0 : (uint8_t)desired;
}

void ui_list_row(Canvas* c, uint8_t row, const char* text, const char* right, bool selected) {
    const int y = UI_CONTENT_Y + row * UI_LIST_ROW_H;
    canvas_set_font(c, FontSecondary);
    if(selected) {
        canvas_draw_rbox(c, 0, y, SCREEN_W - 4, UI_LIST_ROW_H, 2);
        canvas_set_color(c, ColorWhite);
    }
    canvas_draw_str(c, 3, y + 8, text);
    if(right) ui_tiny_right(c, SCREEN_W - 7, y + 3, right);
    canvas_set_color(c, ColorBlack);
}

void ui_scrollbar(Canvas* c, uint8_t visible, uint8_t total, uint8_t top) {
    const int x = SCREEN_W - 2;
    const int y = UI_CONTENT_Y;
    const int h = UI_LIST_ROW_H * UI_LIST_ROWS;
    canvas_draw_line(c, x, y, x, y + h - 1);
    if(total <= visible) return;
    int thumb = (h * visible) / total;
    if(thumb < 4) thumb = 4;
    const int max_top = total - visible;
    const int thumb_y = y + ((h - thumb) * top) / max_top;
    canvas_draw_box(c, x - 1, thumb_y, 3, thumb);
}

void ui_bar(Canvas* c, int x, int y, int w, int h, int value, int max) {
    canvas_draw_frame(c, x, y, w, h);
    if(max <= 0) return;
    if(value < 0) value = 0;
    if(value > max) value = max;
    const int fill = ((w - 2) * value) / max;
    if(fill > 0) canvas_draw_box(c, x + 1, y + 1, fill, h - 2);
}

void ui_sprite(Canvas* c, int x, int y, const char* const* rows, int h) {
    for(int r = 0; r < h; r++) {
        const char* row = rows[r];
        for(int col = 0; row[col]; col++) {
            if(row[col] == '#') canvas_draw_dot(c, x + col, y + r);
        }
    }
}

void ui_art(Canvas* c, int x, int y, int w, int h, const uint8_t* bits) {
    if(ui_is_dark()) {
        canvas_draw_xbm(c, x, y, w, h, bits);
    } else {
        canvas_draw_box(c, x, y, w, h);
        canvas_set_color(c, ColorWhite);
        canvas_draw_xbm(c, x, y, w, h, bits);
        canvas_set_color(c, ColorBlack);
    }
}

void ui_dialog_box(Canvas* c, int x, int y, int w, int h) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, x, y, w, h);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, x, y, w, h, 2);
}
