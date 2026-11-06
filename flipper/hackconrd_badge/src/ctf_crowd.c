// The Crowd: one Flipper on the air as many badges, so a single real badge can reach the friend counts
// of its quest. It answers the badge's requests with the same frames and timing as a real badge.
//
// Size: stock firmware arms the quest at 5 friends, then needs 5 more. Builds from before 2026-03-19
// need 7 more, and the earliest ones arm at 7. Friends added before the quest arms do not count toward
// it, so the worst case is 14 new friends.
//
// Every member carries the alien flag, as every badge does in all versions of the firmware's source
// (BADGE_IS_ALIEN defaults to 1). The badge never checks for the name CAESARS, it only counts aliens.
#include "features.h"
#include "leds.h"
#include "radio.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

#define CROWD_SIZE    16
#define CROWD_BASE_ID 0xC500

// A badge in its Friends screens says HELLO at least every 2 s, which is what wakes the crowd, so it
// stays quiet when no badge is listening. A badge scans for 6 s, and with about 80 ms per frame this
// gap fits all 16 in one scan with some room for lost frames.
#define HELLO_GAP_MS 200
#define AWAKE_MS     8000
#define PAUSE_MS     1000

// RSSI above this means someone is transmitting. Same threshold as the badge's listen-before-talk.
#define BUSY_DBM       (-80)
#define BUSY_MAX_TICKS 15

static const char* const CROWD_NICKS[CROWD_SIZE] = {
    "CAESARS",
    "KR4K3N",
    "R00TKIT",
    "BYT3",
    "NULLPTR",
    "SEGV",
    "H3X",
    "D3BUG",
    "X0R",
    "ST4CK",
    "H34P",
    "PAYL04D",
    "SH3LL",
    "GL1TCH",
    "PH4NT0M",
    "C0BALT",
};

static const StoryPage CROWD_INTRO[] = {
    {"THE CROWD",
     "16 BADGES",
     "On your badge: Friends, Add Friends. Everyone here accepts.",
     "OK START  BACK SKIP",
     true,
     false},
};

static struct {
    bool tx_allowed;
    bool heard;
    uint32_t last_heard;
    char badge_nick[9];
    int8_t badge_rssi;

    uint16_t added_for; // the badge the count below belongs to
    uint16_t added; // bitmask of members this session, so the others get more air time
    uint8_t added_count;
    const char* last_added;

    uint8_t next;
    uint32_t next_hello;
    uint8_t busy_ticks;
} crowd;

static bool is_member(uint16_t id) {
    return id >= CROWD_BASE_ID && id < CROWD_BASE_ID + CROWD_SIZE;
}

static void send_as_member(uint8_t member, uint8_t type, uint16_t to, uint8_t nonce) {
    const uint16_t from = CROWD_BASE_ID + member;
    uint8_t p[32];
    memset(p, 0, sizeof(p));
    p[0] = FRF_MAGIC;
    p[1] = type;
    p[2] = nonce;
    p[3] = bro_encode(member % 5);
    p[4] = (uint8_t)(from & 0xFF);
    p[5] = (uint8_t)(from >> 8);
    p[6] = (uint8_t)(to & 0xFF);
    p[7] = (uint8_t)(to >> 8);
    memcpy(&p[9], CROWD_NICKS[member], strlen(CROWD_NICKS[member]));
    radio_send_fixed32(p);
}

static void note_badge(const RxFrame* f) {
    crowd.heard = true;
    crowd.last_heard = millis();
    crowd.badge_rssi = f->rssi_dbm;
    memcpy(crowd.badge_nick, &f->payload[9], 8);
    crowd.badge_nick[8] = 0;
}

static void accept_request(uint8_t member, uint16_t from, uint8_t nonce) {
    // Same burst as the badge's frfSendAckBurst(). The badge resends a request with the same nonce
    // until an ACK gets through, so answering again is right, but it only counts once.
    furi_delay_ms(20 + (rand_u32() % 60));
    send_as_member(member, FRF_ACK, from, nonce);
    furi_delay_ms(70);
    send_as_member(member, FRF_ACK, from, nonce);
    furi_delay_ms(70);
    send_as_member(member, FRF_ACK, from, nonce);

    // A different badge starts its own count.
    if(from != crowd.added_for) {
        crowd.added_for = from;
        crowd.added = 0;
        crowd.added_count = 0;
        crowd.last_added = NULL;
    }
    if(!(crowd.added & (1 << member))) {
        crowd.added |= (1 << member);
        crowd.added_count++;
        crowd.last_added = CROWD_NICKS[member];
        sound_beep(12);
        leds_accent_flash();
    }
    crowd.next_hello = millis() + PAUSE_MS;
}

static void on_frame(const RxFrame* f) {
    const uint8_t* p = f->payload;
    if(!f->crc_ok || p[0] != FRF_MAGIC) return;

    const uint16_t from = (uint16_t)p[4] | ((uint16_t)p[5] << 8);
    const uint16_t to = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
    // Another Flipper's crowd must not keep this one awake, and the other way around.
    if(from == 0 || is_member(from)) return;

    if(p[1] == FRF_HELLO) {
        note_badge(f);
    } else if(p[1] == FRF_REQ && is_member(to)) {
        note_badge(f);
        if(crowd.tx_allowed) accept_request((uint8_t)(to - CROWD_BASE_ID), from, p[2]);
    }
}

// Returns CROWD_SIZE once everyone has been added.
static uint8_t next_member(void) {
    for(uint8_t i = 0; i < CROWD_SIZE; i++) {
        const uint8_t m = (crowd.next + i) % CROWD_SIZE;
        if(!(crowd.added & (1 << m))) {
            crowd.next = (m + 1) % CROWD_SIZE;
            return m;
        }
    }
    return CROWD_SIZE;
}

static bool badge_listening(void) {
    return crowd.heard && (millis() - crowd.last_heard) < AWAKE_MS;
}

void ctf_crowd_enter(bool skip_intro) {
    radio_tune_hz(RADIO_BADGE_FREQ_HZ);
    radio_enter_rx();
    if(skip_intro) return;
    memset(&crowd, 0, sizeof(crowd));
    crowd.tx_allowed = radio_freq_tx_allowed(RADIO_BADGE_FREQ_HZ);
    story_start(CROWD_INTRO, 1, NULL);
}

void ctf_crowd_tick(const ButtonsEdge* b) {
    if(b->back) {
        go_to(MODE_CTF_MENU, false);
        return;
    }

    radio_rx_service();
    RxFrame f;
    while(radio_rx_pop(&f)) on_frame(&f);

    if(!crowd.tx_allowed || !badge_listening()) return;
    if((int32_t)(millis() - crowd.next_hello) < 0) return;

    // radio_send_fixed32() flushes RX before listening, which would lose a request already on the
    // air. Wait it out here, within limits, since a noisy channel must not silence the crowd.
    if(radio_rssi_dbm() >= BUSY_DBM && crowd.busy_ticks < BUSY_MAX_TICKS) {
        crowd.busy_ticks++;
        return;
    }
    crowd.busy_ticks = 0;

    const uint8_t member = next_member();
    if(member < CROWD_SIZE) send_as_member(member, FRF_HELLO, FRF_BROADCAST, 0);
    crowd.next_hello = millis() + HELLO_GAP_MS;
}

void ctf_crowd_draw(Canvas* c) {
    char ln[34];
    ui_title(c, "THE CROWD");

    if(!crowd.tx_allowed) {
        ui_tiny_right(c, SCREEN_W - 2, 2, "433.00");
        ui_center(c, 30, FontPrimary, "TX BLOCKED");
        ui_tiny_center(c, SCREEN_W / 2, 34, "THIS FIRMWARE REFUSES 433.00");
        ui_tiny_center(c, SCREEN_W / 2, 41, "BADGES ONLY USE THAT CHANNEL");
        ui_footer(c, "BACK MENU");
        return;
    }

    const bool listening = badge_listening();
    ui_tiny_right(c, SCREEN_W - 2, 2, listening ? "ON AIR 433.00" : "RX 433.00");

    if(listening) {
        snprintf(ln, sizeof(ln), "BADGE: %s %dDBM", crowd.badge_nick[0] ? crowd.badge_nick : "?", crowd.badge_rssi);
    } else {
        strlcpy(ln, "WAITING FOR A BADGE", sizeof(ln));
    }
    ui_tiny(c, 2, 13, ln);

    snprintf(ln, sizeof(ln), "ADDED: %u", crowd.added_count);
    ui_center(c, 31, FontPrimary, ln);
    if(crowd.last_added) {
        snprintf(ln, sizeof(ln), "LAST: %s", crowd.last_added);
        ui_tiny_center(c, SCREEN_W / 2, 34, ln);
    }

    ui_tiny_center(c, SCREEN_W / 2, 43, "ON THE BADGE: FRIENDS,");
    ui_tiny_center(c, SCREEN_W / 2, 49, "ADD FRIENDS, PICK A NAME");
    ui_footer(c, "BACK MENU");
}
