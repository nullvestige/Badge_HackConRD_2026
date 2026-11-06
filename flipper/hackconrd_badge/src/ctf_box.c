// THE BOX: the sealed container of the venue, played by the Flipper. It only listens. A badge sends its
// code in plain text and decides on its own whether it was right, showing FLAG 4 without waiting for an
// answer, so nothing here can affect the badge.
//
// The firmware of the real box is not public. What happens when it opens is a design of this port.
//
// Frame, sent 5 times at 433.00 MHz on the Friends profile:
//   [0..5] code  [6..13] nick  [14..15] badge ID (LE)  [16..31] zero
#include "features.h"
#include "leds.h"
#include "radio.h"
#include "story.h"
#include "ui.h"

// The badge's burst of 5 lasts well under a second.
#define SAME_BURST_MS 1200

static const StoryPage BOX_ROLE_INTRO[] = {
    {"THE BOX",
     "SEALED",
     "I listen for codes sent from THE BOX on your badge.",
     "OK START  BACK SKIP",
     true,
     false},
};

static struct {
    uint16_t attempts;
    bool have_attempt;
    char code[7];
    char nick[9];
    uint16_t id;
    int8_t rssi;
    uint32_t last_ms;
    bool open;
} sealed;

// A Friends frame on the same channel starts with its 0xF6 magic, and the badge's editor can only
// produce A to Z, so this tells the two apart.
static bool is_code_frame(const RxFrame* f) {
    if(!f->crc_ok) return false;
    for(uint8_t i = 0; i < 6; i++) {
        if(f->payload[i] < 'A' || f->payload[i] > 'Z') return false;
    }
    return true;
}

static void on_frame(const RxFrame* f) {
    if(sealed.open || !is_code_frame(f)) return;

    const uint8_t* p = f->payload;
    const uint16_t id = (uint16_t)p[14] | ((uint16_t)p[15] << 8);
    const uint32_t now = millis();

    const bool repeat = sealed.have_attempt && id == sealed.id && memcmp(p, sealed.code, 6) == 0 &&
                        (now - sealed.last_ms) < SAME_BURST_MS;
    sealed.last_ms = now;
    if(repeat) return;

    memcpy(sealed.code, p, 6);
    sealed.code[6] = 0;
    memcpy(sealed.nick, &p[6], 8);
    sealed.nick[8] = 0;
    sealed.id = id;
    sealed.rssi = f->rssi_dbm;
    sealed.have_attempt = true;
    sealed.attempts++;

    sealed.open = box_code_is_correct(sealed.code);
    leds_access_effect(sealed.open);
}

void ctf_box_enter(bool skip_intro) {
    // Listening is allowed everywhere, so this needs no regional fallback.
    radio_tune_hz(RADIO_BADGE_FREQ_HZ);
    radio_enter_rx();
    if(skip_intro) return;
    memset(&sealed, 0, sizeof(sealed));
    story_start(BOX_ROLE_INTRO, 1, NULL);
}

void ctf_box_tick(const ButtonsEdge* b) {
    if(b->back) {
        go_to(MODE_CTF_MENU, false);
        return;
    }
    if(sealed.open && b->ok) {
        sealed.open = false;
        sealed.have_attempt = false; // otherwise the right code would show up as the last denied one
    }

    radio_rx_service();
    RxFrame f;
    while(radio_rx_pop(&f)) on_frame(&f);
}

#define ART_X 6
#define ART_Y 33
#define ART_W 34
#define ART_H 21

static void draw_closed_box(Canvas* c) {
    canvas_draw_frame(c, ART_X, ART_Y, ART_W, ART_H);
    canvas_draw_frame(c, ART_X - 2, ART_Y - 8, ART_W + 4, 9);
    canvas_draw_box(c, ART_X + ART_W / 2 - 3, ART_Y - 2, 6, 7);
}

static void draw_open_box(Canvas* c) {
    canvas_draw_frame(c, ART_X, ART_Y, ART_W, ART_H);
    // The lid swung back on its hinge.
    canvas_draw_line(c, ART_X, ART_Y - 1, ART_X - 4, ART_Y - 15);
    canvas_draw_line(c, ART_X + 3, ART_Y - 1, ART_X - 1, ART_Y - 15);
    canvas_draw_line(c, ART_X - 4, ART_Y - 15, ART_X - 1, ART_Y - 15);
    // Light coming out, flickering.
    const int mid = ART_X + ART_W / 2 + 2;
    if((millis() / 200) % 2) {
        canvas_draw_line(c, mid, ART_Y - 3, mid, ART_Y - 15);
        canvas_draw_line(c, mid - 7, ART_Y - 3, mid - 11, ART_Y - 12);
        canvas_draw_line(c, mid + 7, ART_Y - 3, mid + 11, ART_Y - 12);
    } else {
        canvas_draw_line(c, mid - 3, ART_Y - 3, mid - 5, ART_Y - 14);
        canvas_draw_line(c, mid + 3, ART_Y - 3, mid + 5, ART_Y - 14);
    }
}

void ctf_box_draw(Canvas* c) {
    char ln[20];
    const int x = 46;
    const char* nick = sealed.nick[0] ? sealed.nick : "UNKNOWN";

    ui_title(c, "THE BOX");

    if(sealed.open) {
        ui_tiny_right(c, SCREEN_W - 2, 2, "ACCESS GRANTED");
        draw_open_box(c);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, x, 22, "OPENED");
        snprintf(ln, sizeof(ln), "BY %s", nick);
        ui_tiny(c, x, 26, ln);
        snprintf(ln, sizeof(ln), "ID %04X", sealed.id);
        ui_tiny(c, x, 33, ln);
        ui_tiny(c, x, 42, "FLAG 4 IS ON");
        ui_tiny(c, x, 48, "YOUR BADGE");
        ui_footer(c, "OK SEAL AGAIN  BACK MENU");
        return;
    }

    ui_tiny_right(c, SCREEN_W - 2, 2, "RX 433.00");
    draw_closed_box(c);

    snprintf(ln, sizeof(ln), "ATTEMPTS: %u", sealed.attempts);
    ui_tiny(c, x, 14, ln);
    if(sealed.have_attempt) {
        ui_tiny(c, x, 23, "DENIED:");
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, x, 38, sealed.code);
        snprintf(ln, sizeof(ln), "BY %s", nick);
        ui_tiny(c, x, 42, ln);
        snprintf(ln, sizeof(ln), "ID %04X %dDBM", sealed.id, sealed.rssi);
        ui_tiny(c, x, 49, ln);
    } else {
        ui_tiny(c, x, 25, "SEND THE CODE");
        ui_tiny(c, x, 32, "FROM THE BOX ON");
        ui_tiny(c, x, 39, "YOUR BADGE");
    }
    ui_footer(c, "BACK MENU");
}
