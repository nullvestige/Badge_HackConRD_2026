// Friends over Sub-GHz. Same 32-byte frames and handshake as the badge, so a Flipper and a real
// badge can add each other:
//   [0] magic  [1] type  [2] nonce  [3] bro  [4..5] from (LE)  [6..7] to (LE)  [8] data
//   [9..16] nick  [17..22] zero  [23..31] unused
#include "features.h"
#include "prefs.h"
#include "radio.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

#define SCAN_HITS_MAX 32
#define SCAN_MS       6000

typedef struct {
    uint16_t id;
    uint8_t bro;
    char nick[9];
    int8_t rssi;
    uint32_t last_seen;
} ScanHit;

static const char* const ROLE_NAMES[5] = {"SCRPTK1D", "H4CK3R", "PHR3AK", "R3DT34M", "PWN3D"};

static struct {
    ScanHit hits[SCAN_HITS_MAX];
    uint8_t hits_count;
    uint8_t scan_index;
    bool collecting;
    uint32_t scan_end;

    uint8_t menu_index;
    uint8_t list_index;
    bool delete_confirm;

    uint32_t hello_next;

    // Incoming request
    bool seen_req;
    uint32_t req_prompt_start;
    uint16_t req_from;
    uint8_t req_nonce;
    uint8_t req_bro;
    char req_nick[9];
    uint16_t last_incoming_from;
    uint32_t incoming_ignore_until;
    ScreenMode mode_before_prompt;

    // Outgoing request
    uint16_t target_id;
    char target_nick[9];
    uint16_t out_to;
    uint8_t out_nonce;
    bool seen_ack, seen_nack;
    uint16_t ack_from, nack_from;
    uint32_t req_start, last_resend;
    uint8_t resend_stage;
} fr;

// --- Storage ----------------------------------------------------------------

static int friends_index_by_id(uint16_t sid) {
    for(uint8_t i = 0; i < prefs.friends_count; i++) {
        if(prefs.friends[i].short_id == sid) return i;
    }
    return -1;
}

bool friends_is_friend(uint16_t id) {
    return friends_index_by_id(id) >= 0;
}

static bool friends_add(uint16_t sid, const char* nick, uint8_t bro) {
    if(sid == self_short_id()) return false;
    if(friends_index_by_id(sid) >= 0) return false;
    if(prefs.friends_count >= FRIENDS_MAX) return false;

    FriendEntry* f = &prefs.friends[prefs.friends_count++];
    f->short_id = sid;
    f->role = bro & 0x07;
    memset(f->nick, 0, sizeof(f->nick));
    strlcpy(f->nick, nick, sizeof(f->nick));
    prefs_save();
    return true;
}

static void friends_delete_at(uint8_t idx) {
    if(idx >= prefs.friends_count) return;
    for(uint8_t i = idx + 1; i < prefs.friends_count; i++) prefs.friends[i - 1] = prefs.friends[i];
    prefs.friends_count--;
    prefs_save();
}

// --- Scan cache -------------------------------------------------------------

static void scan_start(void) {
    memset(fr.hits, 0, sizeof(fr.hits));
    fr.hits_count = 0;
    fr.scan_index = 0;
    fr.collecting = true;
    fr.scan_end = millis() + SCAN_MS;
    solo_scan_reset();
}

void friends_scan_seen(uint16_t id, uint8_t bro, int8_t rssi, const char* nick) {
    if(!fr.collecting) return;
    int idx = -1;
    for(uint8_t i = 0; i < fr.hits_count; i++) {
        if(fr.hits[i].id == id) idx = i;
    }
    if(idx < 0) {
        if(fr.hits_count >= SCAN_HITS_MAX) return;
        idx = fr.hits_count++;
        fr.hits[idx].id = id;
        const int fidx = friends_index_by_id(id);
        if(fidx >= 0) strlcpy(fr.hits[idx].nick, prefs.friends[fidx].nick, sizeof(fr.hits[idx].nick));
    }
    fr.hits[idx].bro = bro;
    fr.hits[idx].rssi = rssi;
    fr.hits[idx].last_seen = millis();
    if(nick && nick[0]) strlcpy(fr.hits[idx].nick, nick, sizeof(fr.hits[idx].nick));
}

// --- Packets ----------------------------------------------------------------

static void frf_build(uint8_t out[32], uint8_t type, uint16_t to, uint8_t nonce) {
    memset(out, 0, 32);
    const uint16_t from = self_short_id();
    out[0] = FRF_MAGIC;
    out[1] = type;
    out[2] = nonce;
    out[3] = bro_encode(prefs.role);
    out[4] = (uint8_t)(from & 0xFF);
    out[5] = (uint8_t)(from >> 8);
    out[6] = (uint8_t)(to & 0xFF);
    out[7] = (uint8_t)(to >> 8);
    memcpy(&out[9], prefs.nick, 8);
}

static uint8_t frf_make_nonce(void) {
    const uint8_t n = rand8();
    return n == 0 ? 1 : n;
}

static void frf_send(uint8_t type, uint16_t to, uint8_t nonce) {
    uint8_t p[32];
    frf_build(p, type, to, nonce);
    radio_send_fixed32(p);
}

static void frf_send_req(uint16_t to) {
    if(fr.out_to != to || fr.out_nonce == 0) {
        fr.out_to = to;
        fr.out_nonce = frf_make_nonce();
    }
    frf_send(FRF_REQ, to, fr.out_nonce);
}

static void frf_send_burst(uint8_t type, uint16_t to, uint8_t nonce) {
    furi_delay_ms(20 + (rand_u32() % 60));
    frf_send(type, to, nonce);
    furi_delay_ms(70);
    frf_send(type, to, nonce);
    furi_delay_ms(70);
    frf_send(type, to, nonce);
}

static void frf_ensure_hello(void) {
    const uint32_t now = millis();
    if((int32_t)(now - fr.hello_next) < 0) return;
    fr.hello_next = now + 1500 + (rand_u32() % 500);
    frf_send(FRF_HELLO, FRF_BROADCAST, 0);
}

// Virtual peers answer through here instead of over the air.
void friends_virtual_ack(uint16_t from) {
    if(current_mode() != MODE_FRIENDS_REQUESTING || from != fr.target_id) return;
    fr.seen_ack = true;
    fr.ack_from = from;
}

void friends_background_tick(void) {
    const ScreenMode mode = current_mode();
    if(mode != MODE_FRIENDS_REQUESTING) frf_ensure_hello();

    radio_rx_service();
    solo_scan_tick();
    solo_request_tick();

    RxFrame f;
    while(radio_rx_pop(&f)) {
        const uint8_t* p = f.payload;
        if(p[0] != FRF_MAGIC) continue;

        const uint8_t type = p[1];
        const uint8_t a = p[2];
        const uint8_t bro = p[3];
        const uint16_t from = (uint16_t)p[4] | ((uint16_t)p[5] << 8);
        const uint16_t to = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
        char nick[9];
        memcpy(nick, &p[9], 8);
        nick[8] = 0;

        if(to != FRF_BROADCAST && to != self_short_id()) continue;
        if(from == 0 || from == self_short_id()) continue;

        if(type == FRF_HELLO) {
            if(f.rssi_dbm >= -70) friends_scan_seen(from, bro, f.rssi_dbm, nick); // proximity filter
        } else if(type == FRF_REQ) {
            const uint32_t now = millis();
            if(from == fr.last_incoming_from && (int32_t)(now - fr.incoming_ignore_until) < 0) continue;

            if(friends_index_by_id(from) >= 0) {
                frf_send(FRF_ACK, from, a);
                continue;
            }
            if(mode == MODE_FRIEND_REQ_PROMPT || mode == MODE_FRIENDS_REQUESTING || fr.seen_req) {
                frf_send(FRF_NACK, from, a);
                continue;
            }

            fr.seen_req = true;
            fr.req_prompt_start = now;
            fr.req_from = from;
            fr.req_nonce = a;
            fr.req_bro = bro;
            if(nick[0]) {
                strlcpy(fr.req_nick, nick, sizeof(fr.req_nick));
            } else {
                snprintf(fr.req_nick, sizeof(fr.req_nick), "%04X", (unsigned)from);
            }
            fr.last_incoming_from = from;
            fr.incoming_ignore_until = now + 4000;
        } else if(type == FRF_ACK || type == FRF_NACK) {
            if(mode != MODE_FRIENDS_REQUESTING) continue;
            if(from != fr.out_to || a != fr.out_nonce) continue;
            if(type == FRF_ACK) {
                fr.seen_ack = true;
                fr.ack_from = from;
            } else {
                fr.seen_nack = true;
                fr.nack_from = from;
            }
        }
    }

    if(fr.seen_req && mode != MODE_FRIEND_REQ_PROMPT && mode != MODE_FRIENDS_REQUESTING &&
       !story_active()) {
        fr.mode_before_prompt = mode;
        go_to(MODE_FRIEND_REQ_PROMPT, false);
    }
}

// --- Screens ----------------------------------------------------------------

void friends_on_enter(ScreenMode mode) {
    if(mode == MODE_FRIENDS_REQUESTING) {
        fr.req_start = millis();
        fr.last_resend = millis();
        fr.resend_stage = 0;
    } else if(mode == MODE_FRIENDS_LIST) {
        fr.delete_confirm = false;
        if(fr.list_index >= prefs.friends_count) fr.list_index = 0;
    }
}

static void clear_incoming(void) {
    fr.seen_req = false;
    fr.req_from = 0;
    fr.req_bro = 0;
    fr.req_prompt_start = 0;
    memset(fr.req_nick, 0, sizeof(fr.req_nick));
}

static void tick_prompt(const ButtonsEdge* b) {
    if(fr.req_prompt_start != 0 && (uint32_t)(millis() - fr.req_prompt_start) > 10500) {
        fr.last_incoming_from = fr.req_from;
        fr.incoming_ignore_until = millis() + 4000;
        clear_incoming();
        toast_small("TIMEOUT", 500);
        go_to(fr.mode_before_prompt, true);
        return;
    }

    if(b->ok) {
        const uint16_t from = fr.req_from;
        const uint8_t bro = fr.req_bro;
        const uint8_t nonce = fr.req_nonce;
        const bool was_armed = prefs.qa_armed;
        const bool added = friends_add(from, fr.req_nick[0] ? fr.req_nick : "FRIEND", bro);

        frf_send_burst(FRF_ACK, from, nonce);

        fr.last_incoming_from = from;
        fr.incoming_ignore_until = millis() + 4000;
        clear_incoming();

        bool story_started = false;
        quest_after_friend_added(from, bro, added, was_armed, &story_started);

        go_to(MODE_FRIENDS_MENU, true);
        if(!story_started) {
            if(added) {
                sound_beep(12);
                toast_small("ADDED", 500);
            } else {
                toast_small(prefs.friends_count >= FRIENDS_MAX ? "BOX FULL" : "EXISTS", 500);
            }
        }
        return;
    }

    if(b->back) {
        const uint16_t from = fr.req_from;
        frf_send_burst(FRF_NACK, from, fr.req_nonce);
        fr.last_incoming_from = from;
        fr.incoming_ignore_until = millis() + 4000;
        clear_incoming();
        toast_small("REJECTED", 500);
        go_to(fr.mode_before_prompt, true);
    }
}

static void tick_menu(const ButtonsEdge* b) {
    if(b->up || b->down) fr.menu_index ^= 1;
    if(b->ok) {
        if(fr.menu_index == 0) {
            scan_start();
            go_to(MODE_FRIENDS_SCAN, true);
        } else {
            fr.list_index = 0;
            go_to(MODE_FRIENDS_LIST, true);
        }
        return;
    }
    if(b->back) go_to(MODE_MENU, false);
}

static void scan_move(const ButtonsEdge* b) {
    if(fr.hits_count == 0) return;
    if(b->up) fr.scan_index = (fr.scan_index == 0) ? (fr.hits_count - 1) : (fr.scan_index - 1);
    if(b->down) fr.scan_index = (fr.scan_index + 1) % fr.hits_count;
}

static void tick_scan(const ButtonsEdge* b) {
    if(fr.collecting) {
        if(b->back) {
            fr.collecting = false;
            go_to(MODE_FRIENDS_MENU, true);
            return;
        }
        if((int32_t)(millis() - fr.scan_end) >= 0) {
            fr.collecting = false;
            fr.scan_index = 0;
        }
        scan_move(b);
        return;
    }

    if(hold_once(InputKeyBack, 600)) {
        scan_start();
        return;
    }
    if(b->back_short) {
        go_to(MODE_FRIENDS_MENU, true);
        return;
    }

    scan_move(b);

    if(b->ok && fr.hits_count > 0) {
        const ScanHit* h = &fr.hits[fr.scan_index];
        if(h->id == self_short_id()) {
            sound_beep(10);
        } else if(friends_index_by_id(h->id) >= 0) {
            sound_beep(10);
            toast_small("ADDED", 400);
        } else {
            fr.target_id = h->id;
            strlcpy(fr.target_nick, h->nick, sizeof(fr.target_nick));
            fr.seen_ack = false;
            fr.seen_nack = false;
            fr.ack_from = 0;
            fr.nack_from = 0;
            fr.out_to = 0;
            fr.out_nonce = 0;
            if(solo_is_virtual(h->id)) {
                solo_request_start(h->id);
            } else {
                frf_send_req(fr.target_id);
            }
            go_to(MODE_FRIENDS_REQUESTING, true);
        }
    }
}

static void tick_requesting(void) {
    static const uint16_t RESEND_MS[] = {650, 900, 1200, 1600};
    const bool is_virtual = solo_is_virtual(fr.target_id);

    if(!is_virtual && fr.target_id != 0 &&
       (uint32_t)(millis() - fr.last_resend) > RESEND_MS[fr.resend_stage]) {
        fr.last_resend = millis();
        frf_send_req(fr.target_id);
        if(fr.resend_stage < 3) fr.resend_stage++;
    }

    if(fr.seen_ack && fr.ack_from == fr.target_id) {
        const char* nick = fr.target_nick[0] ? fr.target_nick : "FRIEND";
        uint8_t bro = ROLE_SCRPTK1D;
        const bool was_armed = prefs.qa_armed;

        for(uint8_t i = 0; i < fr.hits_count; i++) {
            if(fr.hits[i].id == fr.target_id) {
                if(fr.hits[i].nick[0]) nick = fr.hits[i].nick;
                bro = fr.hits[i].bro;
                break;
            }
        }

        const uint16_t added_id = fr.target_id;
        const bool added = friends_add(added_id, nick, bro);

        fr.out_to = 0;
        fr.out_nonce = 0;
        fr.seen_ack = false;
        fr.ack_from = 0;
        fr.target_id = 0;

        bool story_started = false;
        quest_after_friend_added(added_id, bro, added, was_armed, &story_started);

        go_to(MODE_FRIENDS_MENU, true);
        if(!story_started) {
            if(added) {
                sound_beep(18);
                toast_small("ADDED", 500);
            } else {
                toast_small(prefs.friends_count >= FRIENDS_MAX ? "BOX FULL" : "EXISTS", 500);
            }
        }
        return;
    }

    if(fr.seen_nack && fr.nack_from == fr.target_id) {
        fr.seen_nack = false;
        fr.nack_from = 0;
        fr.target_id = 0;
        sound_beep(10);
        toast_small("REJECTED", 500);
        go_to(MODE_FRIENDS_MENU, true);
        return;
    }

    // Like the badge, this waits for ACK, NACK or the timeout; BACK is ignored on purpose.
    if((uint32_t)(millis() - fr.req_start) > 10000) {
        fr.target_id = 0;
        toast_small("TIMEOUT", 500);
        go_to(MODE_FRIENDS_MENU, true);
    }
}

static void tick_list(const ButtonsEdge* b) {
    if(fr.delete_confirm) {
        if(b->ok) {
            friends_delete_at(fr.list_index);
            if(fr.list_index >= prefs.friends_count && prefs.friends_count > 0) {
                fr.list_index = prefs.friends_count - 1;
            }
            sound_beep(12);
            fr.delete_confirm = false;
        } else if(b->back) {
            fr.delete_confirm = false;
        }
        return;
    }

    if(b->back) {
        go_to(MODE_FRIENDS_MENU, true);
        return;
    }
    if(prefs.friends_count == 0) return;

    if(b->up) fr.list_index = (fr.list_index == 0) ? (prefs.friends_count - 1) : (fr.list_index - 1);
    if(b->down) fr.list_index = (fr.list_index + 1) % prefs.friends_count;
    if(b->ok) fr.delete_confirm = true;
}

void friends_tick(ScreenMode mode, const ButtonsEdge* b) {
    switch(mode) {
    case MODE_FRIENDS_MENU:
        tick_menu(b);
        break;
    case MODE_FRIENDS_SCAN:
        tick_scan(b);
        break;
    case MODE_FRIENDS_REQUESTING:
        tick_requesting();
        break;
    case MODE_FRIENDS_LIST:
        tick_list(b);
        break;
    case MODE_FRIEND_REQ_PROMPT:
        tick_prompt(b);
        break;
    default:
        break;
    }
}

// --- Drawing ----------------------------------------------------------------

static void draw_scan(Canvas* c) {
    if(fr.collecting) {
        static const char* const DOTS[4] = {"SEARCHING", "SEARCHING.", "SEARCHING..", "SEARCHING..."};
        ui_title(c, DOTS[(millis() / 200) % 4]);
    } else {
        ui_title(c, "ADD FRIENDS");
    }

    const uint8_t total = fr.hits_count;
    const uint8_t top = ui_list_top(fr.scan_index, total, UI_LIST_ROWS);
    for(uint8_t r = 0; r < UI_LIST_ROWS; r++) {
        const uint8_t idx = top + r;
        if(idx >= total) break;
        const ScanHit* h = &fr.hits[idx];
        char name[12];
        char right[12];
        if(h->nick[0]) {
            strlcpy(name, h->nick, sizeof(name));
        } else {
            snprintf(name, sizeof(name), "ID %04X", (unsigned)h->id);
        }
        if(friends_index_by_id(h->id) >= 0) {
            strlcpy(right, "ADDED", sizeof(right));
        } else {
            snprintf(right, sizeof(right), "%dDB", (int)h->rssi);
        }
        ui_list_row(c, r, name, right, idx == fr.scan_index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, total, top);

    if(fr.collecting) {
        const uint32_t left = (int32_t)(fr.scan_end - millis()) > 0 ? fr.scan_end - millis() : 0;
        char buf[24];
        snprintf(buf, sizeof(buf), "BACK CANCEL  %luS", (unsigned long)(left / 1000));
        ui_footer(c, buf);
    } else {
        ui_footer(c, total ? "OK ADD  HOLD BACK RESCAN" : "NONE. HOLD BACK RESCAN");
    }
}

static void draw_list(Canvas* c) {
    ui_title(c, "FRIENDS LIST");
    char count[10];
    snprintf(count, sizeof(count), "%u/%u", prefs.friends_count, FRIENDS_MAX);
    ui_tiny_right(c, SCREEN_W - 2, 2, count);

    const uint8_t total = prefs.friends_count;
    if(total == 0) {
        ui_center(c, 34, FontSecondary, "No friends yet");
    }
    const uint8_t top = ui_list_top(fr.list_index, total, UI_LIST_ROWS);
    for(uint8_t r = 0; r < UI_LIST_ROWS; r++) {
        const uint8_t idx = top + r;
        if(idx >= total) break;
        const FriendEntry* f = &prefs.friends[idx];
        ui_list_row(
            c,
            r,
            f->nick[0] ? f->nick : "UNKNOWN",
            ROLE_NAMES[f->role > 4 ? 4 : f->role],
            idx == fr.list_index);
    }
    ui_scrollbar(c, UI_LIST_ROWS, total, top);
    ui_footer(c, total ? "OK DELETE  BACK MENU" : "BACK MENU");

    if(fr.delete_confirm) {
        ui_dialog_box(c, 16, 14, SCREEN_W - 32, 38);
        ui_tiny_center(c, SCREEN_W / 2, 18, "DELETE FRIEND?");
        ui_center(c, 36, FontPrimary, prefs.friends[fr.list_index].nick);
        ui_tiny_center(c, SCREEN_W / 2, 43, "OK YES  BACK NO");
    }
}

void friends_draw(ScreenMode mode, Canvas* c) {
    switch(mode) {
    case MODE_FRIENDS_MENU:
        ui_title(c, "FRIENDS");
        ui_list_row(c, 0, "Add Friends", NULL, fr.menu_index == 0);
        ui_list_row(c, 1, "Friends List", NULL, fr.menu_index == 1);
        ui_footer(c, "OK OPEN  BACK MENU");
        break;
    case MODE_FRIENDS_SCAN:
        draw_scan(c);
        break;
    case MODE_FRIENDS_REQUESTING:
        ui_title(c, "REQUEST");
        ui_tiny(c, 2, 14, "WAITING FOR ACCEPT...");
        canvas_draw_rframe(c, 8, 24, SCREEN_W - 16, 22, 2);
        ui_center(c, 39, FontPrimary, fr.target_nick[0] ? fr.target_nick : "FRIEND");
        ui_footer(c, "PLEASE WAIT...");
        break;
    case MODE_FRIENDS_LIST:
        draw_list(c);
        break;
    case MODE_FRIEND_REQ_PROMPT:
        ui_title(c, "FRIEND REQ");
        ui_tiny(c, 2, 14, "INCOMING REQUEST:");
        canvas_draw_rframe(c, 8, 24, SCREEN_W - 16, 22, 2);
        ui_center(c, 39, FontPrimary, fr.req_nick);
        ui_footer(c, "OK ACCEPT  BACK REJECT");
        break;
    default:
        break;
    }
}
