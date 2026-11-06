// Solo mode: Flipper-only addition so people who were not at the event can finish the CTF.
// It simulates the crowd of badges and the RF lab's second badge locally, never on the air.
// Real radio traffic keeps working alongside it.
#include "features.h"
#include "prefs.h"
#include "radio.h"

#define VIRTUAL_BASE  0xFA00
#define POOL_SIZE     13
#define SCAN_OFFERS   6
#define BRO_ALIEN     0x80
#define LAB_PEER      0xC5
#define LAB_BEACON_MS 1200

// Entry 0 only shows up once the quest asks for him.
static const char* const POOL[POOL_SIZE] = {
    "CAESARS", "KR4K3N", "R00TKIT", "BYT3", "NULLPTR", "SEGV", "H3X",
    "D3BUG",   "X0R",    "ST4CK",   "H34P", "PAYL04D", "SH3LL",
};

static struct {
    uint8_t offer[SCAN_OFFERS];
    uint32_t offer_at[SCAN_OFFERS];
    uint8_t offer_count;
    uint8_t offer_done;

    uint16_t request_id;
    uint32_t request_at;

    uint32_t beacon_next;
    uint8_t beacon_ctr;
    uint32_t ack_at;
    uint8_t ack_ctr;
} solo;

bool solo_enabled(void) {
    return prefs.solo_mode != 0;
}

bool solo_is_virtual(uint16_t id) {
    return id >= VIRTUAL_BASE && id < VIRTUAL_BASE + POOL_SIZE;
}

// --- Friends --------------------------------------------------------------------

void solo_scan_reset(void) {
    solo.offer_count = 0;
    solo.offer_done = 0;
    if(!solo_enabled()) return;

    const uint32_t now = millis();
    const uint8_t start = rand8() % POOL_SIZE;
    for(uint8_t i = 0; i < POOL_SIZE && solo.offer_count < SCAN_OFFERS; i++) {
        const uint8_t n = (start + i) % POOL_SIZE;
        if(n == 0 && !prefs.qa_armed) continue;
        if(friends_is_friend(VIRTUAL_BASE + n)) continue;
        solo.offer[solo.offer_count] = n;
        solo.offer_at[solo.offer_count] = now + 300 + (uint32_t)solo.offer_count * 700 + (rand8() % 200);
        solo.offer_count++;
    }
}

void solo_scan_tick(void) {
    if(solo.offer_done >= solo.offer_count) return;
    if((int32_t)(millis() - solo.offer_at[solo.offer_done]) < 0) return;

    const uint8_t n = solo.offer[solo.offer_done++];
    const int8_t rssi = (int8_t)(-40 - (rand8() % 28));
    friends_scan_seen(VIRTUAL_BASE + n, (uint8_t)(n % 5) | BRO_ALIEN, rssi, POOL[n]);
}

void solo_request_start(uint16_t id) {
    solo.request_id = id;
    solo.request_at = millis() + 1200 + (rand8() * 5);
}

void solo_request_tick(void) {
    if(solo.request_id == 0) return;
    if((int32_t)(millis() - solo.request_at) < 0) return;
    friends_virtual_ack(solo.request_id);
    solo.request_id = 0;
}

// --- RF lab -----------------------------------------------------------------------

uint8_t solo_lab_peer(void) {
    return LAB_PEER;
}

static void lab_inject(uint8_t lesson, uint8_t type, uint8_t target, uint8_t ctr, uint8_t data) {
    RxFrame f;
    memset(&f, 0, sizeof(f));
    const uint8_t p[9] = {0xA6, lesson, type, LAB_PEER, target, ctr, 0, data, 0};
    memcpy(f.payload, p, sizeof(p));
    f.rssi_dbm = (int8_t)(-52 - (rand8() % 12));
    f.lqi = 10 + (rand8() % 20);
    f.crc_ok = true;
    f.ms = millis();
    f.is_virtual = true;
    radio_rx_push(&f);
}

void solo_lab_tick(bool active) {
    if(!solo_enabled() || !active) return;
    const uint32_t now = millis();
    if((int32_t)(now - solo.beacon_next) >= 0) {
        solo.beacon_next = now + LAB_BEACON_MS;
        lab_inject(0x26, 1 /* RFL_BEACON */, LAB_PEER, solo.beacon_ctr++, 0x48);
    }
    if(solo.ack_at != 0 && (int32_t)(now - solo.ack_at) >= 0) {
        solo.ack_at = 0;
        lab_inject(4, 5 /* RFL_REPLAY_ACK */, 0xFF, solo.ack_ctr++, 0);
    }
}

void solo_lab_replay(uint8_t target) {
    if(solo_enabled() && target == LAB_PEER) solo.ack_at = millis() + 500;
}
