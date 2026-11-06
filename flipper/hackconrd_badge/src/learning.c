// Learning RF lab: spectrum scan, packet injection, frames, capture and replay. Same 9-byte frames
// as the badge:
//   [0] magic  [1] lesson  [2] type  [3] from  [4] target  [5] counter  [6] stateful  [7] data  [8] 0
#include "features.h"
#include "leds.h"
#include "qr_data.h"
#include "radio.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

#define RFL_MAGIC 0xA6
#define RFL_BCAST 0xFF

enum { RFL_BEACON = 1, RFL_FRAME = 2, RFL_FRAME_TEST = 3, RFL_REPLAY = 4, RFL_REPLAY_ACK = 5 };

typedef enum { P_MENU, P_INTRO, P_QR, P_L1, P_L2, P_L3, P_L4, P_L5, P_PARTNER } Phase;

#define SCAN_BINS     20
#define SCAN_START_HZ 433720000UL
#define SCAN_STEP_HZ  20000UL

// The badge stays on whichever sweep bin it last touched when leaving lesson 1, anywhere in a
// 380 kHz span, and a receiver only catches signals close to its own frequency (50 kHz off already
// failed against a real badge). Listening on one fixed frequency took about 70 packets to hear one.
// So until a real frame arrives, the lab hops across the span, listening and transmitting on each
// channel in turn, then stays on the channel that worked.
#define LAB_CHANNELS 5
static const uint32_t LAB_CHANNEL_HZ[LAB_CHANNELS] = {
    433760000UL, 433840000UL, 433920000UL, 434000000UL, 434080000UL};
// Long enough to catch one frame of a badge's burst, which repeats about every 145 ms.
#define HOP_DWELL_MS    150
#define LOCK_TIMEOUT_MS 20000

#define CAP_MAX    4
#define CAP_WIN_MS 6000

#define L1_TX_REPEATS 4
#define L1_TX_GAP_MS  110

#define TXQ_MAX 6

typedef struct {
    uint8_t payload[9];
    uint8_t remaining;
    uint16_t gap_ms;
    uint32_t next_ms;
    bool active;
} TxReq;

static struct {
    Phase phase;
    uint8_t tx_counter;

    // Channel search
    bool locked;
    uint8_t channel;
    uint32_t hop_next;
    uint32_t last_real_rx;

    // Last decoded frame
    bool have_rx;
    uint8_t last_flag, last_type, last_from, last_target, last_ctr, last_lqi;
    int16_t last_rssi;

    // Lesson 1
    bool l1_tx_mode;
    bool l1_tx_flash;
    uint32_t l1_tx_flash_ms;
    int16_t scan_rssi[SCAN_BINS];
    uint8_t scan_pos;
    uint32_t next_scan;

    // Lesson 3
    char l3_msg[12];

    // Lesson 4 capture
    RxFrame cap[CAP_MAX];
    uint8_t cap_len, cap_sel;
    bool cap_armed, cap_frozen;
    uint32_t cap_arm_ms;

    // Lesson 5 / access UI
    char access1[20];
    char access2[20];
    uint32_t access_ms;
    uint8_t target_id;

    // TX scheduler
    TxReq txq[TXQ_MAX];
    uint8_t txq_head, txq_tail;
    uint32_t tx_cooldown;

    // CTFs Lab Partner
    uint32_t partner_next_beacon;
    uint32_t partner_ack_ms;
} lab;

static inline uint8_t self_id(void) {
    return (uint8_t)self_short_id();
}

// --- Channel search ------------------------------------------------------------

static void hop_to_next_channel(void) {
    lab.channel = (lab.channel + 1) % LAB_CHANNELS;
    radio_tune_hz(LAB_CHANNEL_HZ[lab.channel]);
    lab.hop_next = millis() + HOP_DWELL_MS;
}

static void search_start(void) {
    lab.locked = false;
    lab.channel = LAB_CHANNELS / 2;
    radio_tune_hz(LAB_CHANNEL_HZ[lab.channel]);
    radio_enter_rx();
    lab.hop_next = millis() + HOP_DWELL_MS;
}

static void search_tick(void) {
    // Lesson 5 stays put: a replay has to go out on the channel its target was heard on.
    if(lab.phase == P_L5) return;
    if(lab.phase < P_L1 || (lab.phase == P_L1 && !lab.l1_tx_mode)) return;

    const uint32_t now = millis();
    if(lab.locked) {
        if(now - lab.last_real_rx > LOCK_TIMEOUT_MS) lab.locked = false;
        return;
    }
    if((int32_t)(now - lab.hop_next) >= 0) {
        hop_to_next_channel();
        radio_enter_rx();
    }
}

// One send per channel when nobody has been heard yet, so a burst covers the whole span.
static inline uint8_t tx_repeats(void) {
    return lab.locked ? L1_TX_REPEATS : LAB_CHANNELS;
}

// --- TX queue: repeats and gaps without blocking the UI ------------------------

static void txq_clear(void) {
    lab.txq_head = lab.txq_tail = 0;
}

static void tx_enqueue(const uint8_t p[9], uint8_t repeats, uint16_t gap_ms) {
    // Coalesce on (lesson, type, from, target): only the newest payload of a kind stays queued.
    for(uint8_t i = lab.txq_head; i != lab.txq_tail; i = (i + 1) % TXQ_MAX) {
        TxReq* q = &lab.txq[i];
        if(q->active && memcmp(&q->payload[1], &p[1], 4) == 0) {
            memcpy(q->payload, p, 9);
            q->gap_ms = gap_ms;
            if(q->remaining == 0) q->remaining = repeats;
            return;
        }
    }
    uint8_t next = (lab.txq_tail + 1) % TXQ_MAX;
    if(next == lab.txq_head) lab.txq_head = (lab.txq_head + 1) % TXQ_MAX;
    TxReq* q = &lab.txq[lab.txq_tail];
    memcpy(q->payload, p, 9);
    q->remaining = repeats;
    q->gap_ms = gap_ms;
    q->next_ms = millis();
    q->active = true;
    lab.txq_tail = next;
}

static void tx_request(uint8_t lesson, uint8_t type, uint8_t target, uint8_t data, uint8_t repeats, uint16_t gap_ms) {
    const uint8_t p[9] = {RFL_MAGIC, lesson, type, self_id(), target, lab.tx_counter++, 0, data, 0};
    tx_enqueue(p, repeats, gap_ms);
}

static void tx_tick(void) {
    const uint32_t now = millis();
    if((int32_t)(now - lab.tx_cooldown) < 0) return;
    if(lab.txq_head == lab.txq_tail) return;

    TxReq* q = &lab.txq[lab.txq_head];
    if(!q->active) {
        lab.txq_head = (lab.txq_head + 1) % TXQ_MAX;
        return;
    }
    if((int32_t)(now - q->next_ms) < 0) return;

    if(!lab.locked) hop_to_next_channel();
    radio_send_fixed9(q->payload);

    if(q->remaining > 0) q->remaining--;
    if(q->remaining == 0) {
        q->active = false;
        lab.txq_head = (lab.txq_head + 1) % TXQ_MAX;
        lab.tx_cooldown = now + 35;
    } else {
        q->next_ms = now + q->gap_ms;
        lab.tx_cooldown = now + 20;
    }
}

// --- RX ----------------------------------------------------------------------

static void set_access(const char* line1, const char* line2) {
    strlcpy(lab.access1, line1, sizeof(lab.access1));
    strlcpy(lab.access2, line2, sizeof(lab.access2));
    lab.access_ms = millis();
}

// A replay arrives as a burst of 4 frames, and it should be answered once.
#define PARTNER_ACK_GUARD_MS 1000

static void partner_on_frame(const RxFrame* f) {
    const uint8_t type = f->payload[2];
    if(type == RFL_REPLAY) {
        // Same check as the victim of lesson 4: only a replay of one of our own frames gets in.
        if(f->payload[3] != self_id()) return;
        const uint32_t now = millis();
        if(now - lab.partner_ack_ms > PARTNER_ACK_GUARD_MS) {
            set_access("ACCESS GRANTED", "BADGE REPLAYED ME");
            leds_access_effect(true);
            tx_request(4, RFL_REPLAY_ACK, RFL_BCAST, 0, 4, 80);
        }
        lab.partner_ack_ms = now;
    } else if(type != RFL_REPLAY_ACK) {
        // The newest frame the badge sent itself, ready to be replayed against it.
        lab.cap[0] = *f;
        lab.cap_len = 1;
    }
}

static void parse_frame(const RxFrame* f) {
    if(f->payload[0] != RFL_MAGIC) return;

    if(!f->is_virtual) {
        lab.locked = true;
        lab.last_real_rx = millis();
    }

    lab.last_flag = f->payload[1];
    lab.last_type = f->payload[2];
    lab.last_from = f->payload[3];
    lab.last_target = f->payload[4];
    lab.last_ctr = f->payload[5];
    lab.last_rssi = f->rssi_dbm;
    lab.last_lqi = f->lqi;
    lab.have_rx = true;

    strlcpy(lab.l3_msg, (f->payload[2] == RFL_REPLAY) ? "TX COMP" : "TX OK", sizeof(lab.l3_msg));

    if(lab.cap_armed && !lab.cap_frozen && lab.cap_len < CAP_MAX) {
        lab.cap[lab.cap_len++] = *f;
        if(lab.cap_sel >= lab.cap_len) lab.cap_sel = lab.cap_len - 1;
        if(lab.cap_len >= CAP_MAX) lab.cap_frozen = true;
    }

    // Victim side of the replay lesson: a replayed frame carrying our own ID opens the door.
    if(lab.phase == P_L4 && lab.last_type == RFL_REPLAY) {
        if(lab.last_from == self_id()) {
            set_access("ACCESS GRANTED", "");
            leds_access_effect(true);
            tx_request(4, RFL_REPLAY_ACK, RFL_BCAST, 0, 4, 80);
        } else {
            set_access("ACCESS DENIED", "");
            leds_access_effect(false);
        }
    }

    // Attacker side: the victim confirms the replay worked.
    if((lab.phase == P_L5 || lab.phase == P_PARTNER) && lab.last_type == RFL_REPLAY_ACK &&
       lab.access1[0] == 'W' && lab.last_from == lab.target_id) {
        set_access("ACCESS GRANTED", "CONGRATS!");
        leds_access_effect(true);
    }

    if(lab.phase == P_PARTNER) partner_on_frame(f);
}

// --- Lesson 1 sweep ------------------------------------------------------------

static void scan_init(void) {
    for(uint8_t i = 0; i < SCAN_BINS; i++) lab.scan_rssi[i] = -110;
    lab.scan_pos = 0;
}

static void scan_tick(void) {
    const uint32_t hz = SCAN_START_HZ + (uint32_t)lab.scan_pos * SCAN_STEP_HZ;
    radio_tune_hz(hz);
    radio_enter_rx();
    furi_delay_ms(3);
    lab.scan_rssi[lab.scan_pos] = radio_rssi_dbm();
    lab.scan_pos = (lab.scan_pos + 1) % SCAN_BINS;
}

// --- Navigation ----------------------------------------------------------------

static void lab_go(Phase p) {
    const Phase prev = lab.phase;
    lab.phase = p;
    sound_beep(10);

    // Leaving the sweep: a lock from TX mode is still good, otherwise start looking for peers.
    if(prev == P_L1 && p != P_L1 && !lab.locked) search_start();

    if(p == P_MENU || p == P_L4) {
        lab.cap_len = 0;
        lab.cap_armed = false;
    }
    if(p == P_L1) {
        lab.l1_tx_mode = false;
        lab.locked = false;
        scan_init();
    }
    if(p == P_L5) {
        if(!(prev == P_L4 && lab.cap_len > 0)) {
            lab.cap_len = 0;
            lab.cap_sel = 0;
            lab.cap_frozen = false;
        }
        lab.cap_armed = false; // lesson 5 never captures live
        lab.access1[0] = 0;
        lab.access2[0] = 0;
        lab.access_ms = 0;
    }
}

void learning_enter(bool skip_intro) {
    if(skip_intro) {
        // Back from the lock screen: the radio was re-tuned to the default channel.
        if(lab.phase >= P_L2 || (lab.phase == P_L1 && lab.l1_tx_mode)) search_start();
        return;
    }
    memset(&lab, 0, sizeof(lab));
    lab.phase = P_MENU;
    strlcpy(lab.l3_msg, "----", sizeof(lab.l3_msg));
    scan_init();
}

void learning_tick(const ButtonsEdge* b) {
    radio_rx_service();
    RxFrame f;
    while(radio_rx_pop(&f)) parse_frame(&f);
    tx_tick();
    search_tick();
    solo_lab_tick(lab.phase >= P_L2);

    const uint32_t now = millis();

    switch(lab.phase) {
    case P_MENU:
        if(b->ok) lab_go(P_INTRO);
        if(b->back) go_to(MODE_MENU, false);
        return;
    case P_INTRO:
        if(b->ok) lab_go(P_QR);
        if(b->back) lab_go(P_MENU);
        return;
    case P_QR:
        if(b->ok) lab_go(P_L1);
        if(b->back) lab_go(P_INTRO);
        return;
    default:
        break;
    }

    // Lesson navigation
    if(b->ok) {
        if(lab.phase == P_L4 && lab.cap_len == 0) {
            sound_beep(50);
            toast_small("CAPTURE REQ'D", 600);
            return;
        }
        lab_go(lab.phase == P_L5 ? P_MENU : (Phase)(lab.phase + 1));
        return;
    }
    if(b->back) {
        lab_go(lab.phase == P_L1 ? P_MENU : (Phase)(lab.phase - 1));
        return;
    }

    switch(lab.phase) {
    case P_L1:
        if(lab.l1_tx_flash && now - lab.l1_tx_flash_ms > 250) lab.l1_tx_flash = false;
        if(hold_once(InputKeyDown, 700)) {
            lab.l1_tx_mode = !lab.l1_tx_mode;
            if(lab.l1_tx_mode) {
                search_start();
            } else {
                lab.locked = false;
                scan_init();
            }
            sound_beep(12);
            return;
        }
        if(b->up) {
            if(lab.l1_tx_mode) {
                lab.l1_tx_flash = true;
                lab.l1_tx_flash_ms = now;
                tx_request(0x26, RFL_BEACON, self_id(), 0x48, tx_repeats(), L1_TX_GAP_MS);
                sound_beep(10);
            } else {
                sound_beep(6);
            }
        }
        if(!lab.l1_tx_mode && (int32_t)(now - lab.next_scan) >= 0) {
            lab.next_scan = now + 60;
            scan_tick();
        }
        break;

    case P_L2:
        if(b->down) {
            lab.cap_len = 0;
            lab.cap_sel = 0;
            lab.cap_armed = false;
            lab.access1[0] = 0;
            lab.access2[0] = 0;
            lab.have_rx = false;
            lab.last_flag = lab.last_type = lab.last_from = lab.last_target = lab.last_ctr = 0;
            lab.last_rssi = -127;
            lab.last_lqi = 0;
            txq_clear();
            sound_beep(8);
        }
        break;

    case P_L3:
        if(b->down) {
            strlcpy(lab.l3_msg, "----", sizeof(lab.l3_msg));
            sound_beep(8);
        }
        break;

    case P_L4:
        if(lab.cap_armed && now - lab.cap_arm_ms > CAP_WIN_MS) lab.cap_armed = false;
        if(hold_once(InputKeyDown, 700)) {
            lab.cap_len = 0;
            lab.cap_sel = 0;
            lab.cap_frozen = false;
            lab.cap_armed = false;
            txq_clear();
            sound_beep(8);
            return;
        }
        if(b->down) {
            if(lab.cap_len > 0) lab.cap_sel = (lab.cap_sel + 1) % lab.cap_len;
            sound_beep(lab.cap_len > 0 ? 6 : 4);
            return;
        }
        if(b->up) {
            lab.cap_len = 0;
            lab.cap_sel = 0;
            lab.cap_frozen = false;
            lab.cap_armed = true;
            lab.cap_arm_ms = now;
            sound_beep(10);
        }
        break;

    case P_L5:
        if(lab.access1[0] == 'W' && now - lab.access_ms > 2500) {
            set_access("ACCESS DENIED", "");
            leds_access_effect(false);
        }
        if(b->down) {
            lab.cap_len = 0;
            lab.cap_sel = 0;
            lab.cap_frozen = false;
            lab.cap_armed = false;
            lab.access1[0] = 0;
            lab.access2[0] = 0;
            txq_clear();
            sound_beep(8);
            return;
        }
        if(b->up) {
            if(lab.cap_len == 0) {
                set_access("NO CAPTURE", "");
                sound_beep(12);
                return;
            }
            const uint8_t idx = (lab.cap_sel < lab.cap_len) ? lab.cap_sel : 0;
            uint8_t p[9];
            memcpy(p, lab.cap[idx].payload, 9);
            p[2] = RFL_REPLAY;
            tx_enqueue(p, tx_repeats(), L1_TX_GAP_MS);

            const uint8_t from = p[3];
            if(from != self_id()) {
                lab.target_id = from;
                set_access("WAITING...", "");
                solo_lab_replay(from);
            } else {
                set_access("ACCESS DENIED", "");
                leds_access_effect(false);
            }
        }
        break;

    default:
        break;
    }
}

// --- Drawing -------------------------------------------------------------------

#define PANEL_Y 20

static void draw_channel_status(Canvas* c) {
    const uint32_t hz = LAB_CHANNEL_HZ[lab.channel];
    char buf[16];
    snprintf(
        buf,
        sizeof(buf),
        "%s %lu.%02lu",
        lab.locked ? "LOCK" : "SEARCH",
        (unsigned long)(hz / 1000000UL),
        (unsigned long)((hz / 10000UL) % 100));
    ui_tiny_right(c, SCREEN_W - 2, 2, buf);
}

static void draw_l1(Canvas* c) {
    if(lab.l1_tx_mode) {
        ui_center(c, 40, FontPrimary, lab.l1_tx_flash ? ">> PACKET SENT <<" : "PRESS UP");
        return;
    }
    int16_t peak = -200;
    for(uint8_t i = 0; i < SCAN_BINS; i++) {
        if(lab.scan_rssi[i] > peak) peak = lab.scan_rssi[i];
    }
    const int base = PANEL_Y + 27;
    for(uint8_t i = 0; i < SCAN_BINS; i++) {
        int dbm = lab.scan_rssi[i];
        if(dbm < -110) dbm = -110;
        if(dbm > -40) dbm = -40;
        int h = ((dbm + 110) * 25) / 70 + 1;
        canvas_draw_box(c, 4 + i * 6, base - h, 4, h);
    }
    char r[32];
    snprintf(r, sizeof(r), "RX  PEAK: %d DBM", (int)peak);
    ui_tiny(c, 4, base + 2, r);
}

static void draw_l2(Canvas* c) {
    char ln[36];
    if(lab.have_rx) {
        snprintf(ln, sizeof(ln), "FROM:%02X CTR:%u TYPE:%u", lab.last_from, lab.last_ctr, lab.last_type);
        ui_tiny(c, 2, PANEL_Y + 2, ln);
        snprintf(ln, sizeof(ln), "RSSI:%d DBM LQI:%u", (int)lab.last_rssi, lab.last_lqi);
        ui_tiny(c, 2, PANEL_Y + 10, ln);
        snprintf(ln, sizeof(ln), "PROTO:%02X (FIELD 1)", lab.last_flag);
        ui_tiny(c, 2, PANEL_Y + 18, ln);
        snprintf(ln, sizeof(ln), "TARGET:%02X", lab.last_target);
        ui_tiny(c, 2, PANEL_Y + 26, ln);
    } else {
        ui_tiny(c, 2, PANEL_Y + 6, "RX: (NONE YET)");
        ui_tiny(c, 2, PANEL_Y + 14, "WAITING FOR PACKET...");
    }
}

static void draw_l4(Canvas* c) {
    char st[36];
    uint32_t left = 0;
    if(lab.cap_armed) {
        const uint32_t el = millis() - lab.cap_arm_ms;
        left = (el >= CAP_WIN_MS) ? 0 : (CAP_WIN_MS - el);
    }
    snprintf(
        st,
        sizeof(st),
        "ARMED:%s %s %luS",
        lab.cap_armed ? "YES" : "NO",
        lab.cap_frozen ? "FROZEN" : "LIVE",
        (unsigned long)(left / 1000));
    ui_tiny(c, 2, PANEL_Y, st);

    if(lab.access1[0] && millis() - lab.access_ms < 4000) {
        ui_center(c, PANEL_Y + 24, FontPrimary, lab.access1);
        return;
    }

    if(lab.have_rx) {
        snprintf(st, sizeof(st), "LAST: FROM:%02X CTR:%u", lab.last_from, lab.last_ctr);
    } else {
        strlcpy(st, "LAST: (NONE)", sizeof(st));
    }
    ui_tiny(c, 2, PANEL_Y + 7, st);

    const int rows = 3;
    uint8_t start = 0;
    if(lab.cap_len > rows && lab.cap_sel >= rows) start = lab.cap_sel - (rows - 1);
    for(int k = 0; k < rows; k++) {
        const uint8_t i = start + k;
        if(i >= lab.cap_len) break;
        const uint8_t* p = lab.cap[i].payload;
        snprintf(st, sizeof(st), "%c#%u FROM:%02X CTR:%u", (i == lab.cap_sel) ? '>' : ' ', i + 1, p[3], p[5]);
        ui_tiny(c, 2, PANEL_Y + 15 + k * 7, st);
    }
}

static void draw_l5(Canvas* c) {
    char src[36];
    if(lab.cap_len > 0) {
        const uint8_t idx = (lab.cap_sel < lab.cap_len) ? lab.cap_sel : 0;
        const uint8_t* p = lab.cap[idx].payload;
        snprintf(src, sizeof(src), "REPLAY #%u FROM:%02X CTR:%u", idx + 1, p[3], p[5]);
    } else {
        strlcpy(src, "REPLAY SRC: (NONE)", sizeof(src));
    }
    ui_tiny(c, 2, PANEL_Y + 1, src);

    if(lab.access1[0]) {
        ui_center(c, PANEL_Y + 21, FontPrimary, lab.access1);
        if(lab.access2[0]) ui_tiny_center(c, SCREEN_W / 2, PANEL_Y + 25, lab.access2);
    }
}

static void draw_qr(Canvas* c) {
    // Scanners need dark modules on a bright field whatever the UI theme is.
    const int scale = 2;
    const int size = QR_SIZE * scale;
    const int ox = SCREEN_W - size - 5;
    const int oy = (SCREEN_H - size) / 2;
    const bool dark = ui_is_dark();

    if(dark) canvas_draw_box(c, ox - 4, oy - 4, size + 8, size + 8);
    canvas_set_color(c, dark ? ColorWhite : ColorBlack);
    for(int y = 0; y < QR_SIZE; y++) {
        for(int x = 0; x < QR_SIZE; x++) {
            if(qr_bits[y * QR_ROW_BYTES + x / 8] & (1 << (x % 8))) {
                canvas_draw_box(c, ox + x * scale, oy + y * scale, scale, scale);
            }
        }
    }
    canvas_set_color(c, ColorBlack);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 9, "Guide");
    ui_tiny(c, 1, 16, "SCAN FOR THE");
    ui_tiny(c, 1, 23, "WEB GUIDE");
    ui_tiny(c, 1, 46, "OK NEXT");
    ui_tiny(c, 1, 53, "BACK PREV");
}

void learning_draw(Canvas* c) {
    switch(lab.phase) {
    case P_MENU:
        ui_title(c, "Learning");
        canvas_draw_rbox(c, 14, 24, SCREEN_W - 28, 18, 3);
        canvas_set_color(c, ColorWhite);
        ui_center(c, 37, FontPrimary, "Start lesson");
        canvas_set_color(c, ColorBlack);
        ui_footer(c, "OK START  BACK MAIN");
        return;
    case P_INTRO:
        ui_title(c, "Learning");
        ui_wrap(c, 4, 25, SCREEN_W - 8, 10, "This is a guided learning. To complete it, open the following web guide.", true);
        ui_footer(c, "OK NEXT  BACK MENU");
        return;
    case P_QR:
        draw_qr(c);
        return;
    default:
        break;
    }

    switch(lab.phase) {
    case P_L1:
        ui_title(c, lab.l1_tx_mode ? "PACKET INJECTION" : "SPECTRUM SCAN");
        ui_tiny(c, 2, 13, lab.l1_tx_mode ? "TX MODE (LIVE TRANSMIT)" : "RX MODE (SCAN + LISTEN)");
        draw_l1(c);
        ui_footer(c, "HOLD DN MODE OK NEXT BACK MENU");
        break;
    case P_L2:
        ui_title(c, "FRAMES");
        draw_channel_status(c);
        draw_l2(c);
        ui_footer(c, "DN CLEAR  OK NEXT  BACK PREV");
        break;
    case P_L3:
        ui_title(c, "INTEGRITY");
        ui_center(c, 40, FontPrimary, lab.l3_msg);
        ui_footer(c, "DN CLEAR  OK NEXT  BACK PREV");
        break;
    case P_L4:
        ui_title(c, "CAPTURE");
        ui_tiny_right(c, SCREEN_W - 2, 2, "VICTIM MODE");
        draw_l4(c);
        ui_footer(c, "UP ARM DN SEL HOLD DN CLR OK >");
        break;
    case P_L5:
        ui_title(c, "REPLAY");
        ui_tiny_right(c, SCREEN_W - 2, 2, "TARGETED");
        draw_l5(c);
        ui_footer(c, "UP REPLAY DN CLR OK MENU");
        break;
    default:
        break;
    }
}

// --- CTFs: Lab Partner ------------------------------------------------------------
// Lessons 2 to 5 on a badge need a second device. At the event that was another badge, or a station
// the firmware only mentions in passing as "masterService". This plays that part without any menus:
// it keeps sending frames for the badge to read and capture, accepts the badge's replay of them, and
// can replay the badge's own frames back at it while it sits in lesson 4's victim mode.
//
// The badge's channel is unknown until it transmits, and a replay burst is too short to sweep through.
// Sending one packet from the badge's injection screen links the two for the rest of the session, as
// long as the badge does not go back to its spectrum sweep.

#define PARTNER_BEACON_MS 600 // L1_TXRATE_MS in the badge's firmware
#define PARTNER_WAIT_MS   2500

static const StoryPage PARTNER_INTRO[] = {
    {"LAB PARTNER",
     "SECOND BADGE",
     "Your badge lab needs a peer for lessons 2 to 5. That is me.",
     "OK START  BACK SKIP",
     true,
     false},
};

void lab_partner_enter(bool skip_intro) {
    if(!skip_intro) {
        memset(&lab, 0, sizeof(lab));
        story_start(PARTNER_INTRO, 1, NULL);
    }
    lab.phase = P_PARTNER;
    search_start();
}

static void partner_replay_badge(void) {
    if(lab.cap_len == 0) {
        sound_beep(12);
        toast_small("NOTHING HEARD", 800);
        return;
    }
    uint8_t p[9];
    memcpy(p, lab.cap[0].payload, 9);
    p[2] = RFL_REPLAY;
    tx_enqueue(p, L1_TX_REPEATS, L1_TX_GAP_MS);
    lab.target_id = p[3];
    set_access("WAITING...", "");
}

void lab_partner_tick(const ButtonsEdge* b) {
    radio_rx_service();
    RxFrame f;
    while(radio_rx_pop(&f)) parse_frame(&f);
    tx_tick();

    if(b->back) {
        go_to(MODE_CTF_MENU, false);
        return;
    }

    const uint32_t now = millis();
    if(lab.access1[0] == 'W' && now - lab.access_ms > PARTNER_WAIT_MS) {
        set_access("NO ANSWER", "BADGE IN LESSON 4?");
        leds_access_effect(false);
    }
    if(b->ok) partner_replay_badge();
    if(b->down) {
        // For when the badge went back to its sweep and moved to another channel.
        lab.have_rx = false;
        lab.cap_len = 0;
        lab.access1[0] = 0;
        lab.access2[0] = 0;
        txq_clear();
        search_start();
        sound_beep(8);
    }

    // Unlocked, each send also hops, so the beacons sweep the whole span the badge may be sitting on.
    if((int32_t)(now - lab.partner_next_beacon) >= 0) {
        lab.partner_next_beacon = now + (lab.locked ? PARTNER_BEACON_MS : HOP_DWELL_MS);
        tx_request(0x26, RFL_BEACON, self_id(), 0x48, 1, 0);
    }
}

void lab_partner_draw(Canvas* c) {
    char ln[36];
    ui_title(c, "LAB PARTNER");
    draw_channel_status(c);

    if(lab.have_rx) {
        snprintf(ln, sizeof(ln), "HEARD %02X CTR:%u TYPE:%u", lab.last_from, lab.last_ctr, lab.last_type);
        ui_tiny(c, 2, 13, ln);
        snprintf(ln, sizeof(ln), "RSSI:%d DBM", (int)lab.last_rssi);
        ui_tiny(c, 2, 20, ln);
    } else {
        ui_tiny(c, 2, 13, "NO BADGE HEARD YET");
    }
    snprintf(ln, sizeof(ln), "MY ID:%02X  SENT:%u", self_id(), lab.tx_counter);
    ui_tiny(c, 2, 27, ln);

    if(lab.access1[0]) {
        ui_center(c, 45, FontPrimary, lab.access1);
        if(lab.access2[0]) ui_tiny_center(c, SCREEN_W / 2, 48, lab.access2);
    } else {
        ui_tiny_center(c, SCREEN_W / 2, 37, "BADGE LAB: HOLD DOWN, PRESS UP");
        ui_tiny_center(c, SCREEN_W / 2, 44, "THEN CAPTURE AND REPLAY ME");
    }
    ui_footer(c, "OK REPLAY DN SEARCH BACK MENU");
}
