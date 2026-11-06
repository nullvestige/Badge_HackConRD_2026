// THE KEY and THE BOX, the last two chapters of the CTF. Codes are stored the same way as on the
// badge (XOR-obfuscated pairs, FNV-1a hash), so reversing them stays part of the challenge.
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "radio.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

// --- THE KEY ----------------------------------------------------------------

#define KEY_XOR 0x5A
static const uint8_t KEY_UNLOCK_XOR[6] = {0x0F, 0x0A, 0x02, 0x0A, 0x18, 0x1B};
static const uint8_t KEY_PAIRS_XOR[6] = {0x6F, 0x6C, 0x63, 0x62, 0x6E, 0x69};

static const StoryPage KEY_INTRO[] = {
    {"CHAPTER III",
     "THE KEY",
     "A cipher was left behind to protect the BOX.",
     "OK NEXT  BACK SKIP",
     true,
     false},
    {"RULES", "ONE TRY", "Enter the six digits. Hold OK to submit.", "OK NEXT  BACK SKIP", false, false},
    {"NOTICE",
     "NO SHARING",
     "What unlocks here belongs to the one who earned it.",
     "OK CLOSE  BACK SKIP",
     false,
     false},
};

static const StoryPage KEY_ERROR_PAGES[] = {
    {"CHAPTER III",
     "DECODER ERROR!",
     "An error occurred while decoding the key completely.",
     "OK SHOW KEY",
     true,
     false},
    {"FLAG 3 DISCOVERED", "HCRD{K3Y-0P3N3D}", NULL, "OK CONTINUE", false, true},
};

typedef enum {
    KEY_STAGE_INPUT,
    KEY_STAGE_INVALID,
    KEY_STAGE_ALERT,
    KEY_STAGE_DECRYPT,
    KEY_STAGE_UNLOCKED,
    KEY_STAGE_ERROR,
} KeyStage;

static struct {
    char input[7];
    uint8_t pos;
    uint8_t pair_order[3];
    KeyStage stage;
    uint32_t stage_until;
    char scramble[7];
    uint32_t next_scramble;
} key;

char key_pair_char(uint8_t i) {
    return (char)(KEY_PAIRS_XOR[i] ^ KEY_XOR);
}

static void key_clear_input(void) {
    memset(key.input, '0', 6);
    key.input[6] = 0;
    key.pos = 0;
}

static void key_shuffle_pairs(void) {
    key.pair_order[0] = 0;
    key.pair_order[1] = 1;
    key.pair_order[2] = 2;
    for(int i = 2; i > 0; --i) {
        const int j = rand_u32() % (i + 1);
        const uint8_t tmp = key.pair_order[i];
        key.pair_order[i] = key.pair_order[j];
        key.pair_order[j] = tmp;
    }
}

static bool key_is_correct(void) {
    for(uint8_t screen = 0; screen < 6; screen++) {
        const uint8_t pair = key.pair_order[screen / 2];
        const uint8_t fixed = (uint8_t)(pair * 2 + screen % 2);
        if(key.input[screen] != key_pair_char(fixed)) return false;
    }
    return true;
}

void key_enter(bool skip_intro) {
    key.stage = KEY_STAGE_INPUT;
    if(!skip_intro) {
        story_start(KEY_INTRO, 3, NULL);
        if(!prefs.key_verified) {
            key_clear_input();
            key_shuffle_pairs();
        }
    }
}

static void key_after_story(void) {
    key.stage = KEY_STAGE_INPUT; // key_verified now selects the unlocked view
}

void key_tick(const ButtonsEdge* b) {
    if(!key_unlocked()) {
        go_to(MODE_MENU, false);
        return;
    }

    const uint32_t now = millis();
    switch(key.stage) {
    case KEY_STAGE_INVALID:
        if((int32_t)(now - key.stage_until) >= 0) key.stage = KEY_STAGE_INPUT;
        return;
    case KEY_STAGE_ALERT:
        if(!leds_flash_active()) {
            key.stage = KEY_STAGE_DECRYPT;
            key.stage_until = now + 4000;
        }
        return;
    case KEY_STAGE_DECRYPT:
        if((int32_t)(now - key.next_scramble) >= 0) {
            static const char SET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%*+-";
            for(uint8_t i = 0; i < 6; i++) key.scramble[i] = SET[rand8() % (sizeof(SET) - 1)];
            key.scramble[6] = 0;
            key.next_scramble = now + 55;
        }
        if((int32_t)(now - key.stage_until) >= 0) {
            sound_beep(20);
            key.stage = KEY_STAGE_UNLOCKED;
            key.stage_until = now + 1200;
        }
        return;
    case KEY_STAGE_UNLOCKED:
        if((int32_t)(now - key.stage_until) >= 0) {
            key.stage = KEY_STAGE_ERROR;
            leds_flash(255, 0, 0, 120, 120, 6, 1000);
        }
        return;
    case KEY_STAGE_ERROR:
        if(!leds_flash_active()) story_start(KEY_ERROR_PAGES, 2, key_after_story);
        return;
    case KEY_STAGE_INPUT:
    default:
        break;
    }

    if(prefs.key_verified) {
        if(b->back) go_to(MODE_MENU, false);
        return;
    }

    if(b->back) {
        key_clear_input();
        go_to(MODE_MENU, false);
        return;
    }
    if(b->left) key.pos = (key.pos == 0) ? 5 : (key.pos - 1);
    if(b->right) key.pos = (key.pos + 1) % 6;
    if(b->up) {
        char* ch = &key.input[key.pos];
        *ch = (*ch == '9') ? '0' : (char)(*ch + 1);
    }
    if(b->down) {
        char* ch = &key.input[key.pos];
        *ch = (*ch == '0') ? '9' : (char)(*ch - 1);
    }
    if(b->ok_short) key.pos = (key.pos + 1) % 6;

    if(hold_once(InputKeyOk, 900)) {
        if(key_is_correct()) {
            prefs.key_verified = 1;
            prefs_save();
            leds_quest_alert();
            key.stage = KEY_STAGE_ALERT;
        } else {
            sound_beep(10);
            key.stage = KEY_STAGE_INVALID;
            key.stage_until = now + 1200;
        }
    }
}

static void key_decode_unlock(char out[7]) {
    for(uint8_t i = 0; i < 6; i++) out[i] = (char)(KEY_UNLOCK_XOR[i] ^ KEY_XOR);
    out[6] = 0;
}

void key_draw(Canvas* c) {
    ui_title(c, "THE KEY");
    canvas_draw_rframe(c, 8, 20, SCREEN_W - 16, 32, 2);

    switch(key.stage) {
    case KEY_STAGE_INVALID:
        ui_center(c, 41, FontPrimary, "INVALID");
        ui_footer(c, "TRY ANOTHER ORDER");
        return;
    case KEY_STAGE_ALERT:
    case KEY_STAGE_DECRYPT:
        ui_center(c, 41, FontPrimary, key.stage == KEY_STAGE_DECRYPT ? key.scramble : "");
        ui_footer(c, "DECRYPTING...");
        return;
    case KEY_STAGE_UNLOCKED:
        ui_center(c, 41, FontPrimary, "UNLOCKED");
        ui_footer(c, "DECRYPTING...");
        return;
    case KEY_STAGE_ERROR:
        ui_center(c, 41, FontPrimary, "ERROR!");
        ui_footer(c, "DECODER FAULT");
        return;
    case KEY_STAGE_INPUT:
    default:
        break;
    }

    if(prefs.key_verified) {
        char unlock[7];
        key_decode_unlock(unlock);
        ui_tiny(c, 2, 13, "UNLOCKED");
        ui_center(c, 41, FontPrimary, unlock);
        ui_footer(c, "BACK EXIT");
        return;
    }

    ui_tiny(c, 2, 13, "ENTER CODE");
    canvas_set_font(c, FontBigNumbers);
    const int step = 17;
    const int base = (SCREEN_W - 6 * step) / 2 + 2;
    for(uint8_t i = 0; i < 6; i++) {
        char s[2] = {key.input[i], 0};
        canvas_draw_str(c, base + i * step, 45, s);
    }
    canvas_draw_box(c, base + key.pos * step, 47, 12, 2);
    ui_footer(c, "U/D EDIT L/R MOVE HOLD OK SEND");
}

// --- THE BOX ----------------------------------------------------------------

static const StoryPage BOX_INTRO[] = {
    {"CHAPTER IV",
     "THE BOX",
     "A sealed container was found behind the venue walls.",
     "OK NEXT  BACK SKIP",
     true,
     false},
    {"WARNING",
     "DO NOT OPEN",
     "It reacts to nearby signals. Bring the right KEY.",
     "OK CLOSE  BACK SKIP",
     false,
     false},
};

static const StoryPage BOX_NICE_TRY[] = {
    {"ERROR", "NICE TRY", "Nobody reads a book from the last page.", "OK CLOSE", false, false},
};

static const StoryPage BOX_FLAG[] = {
    {"FLAG 4 DISCOVERED", "HCRD{B0X-H4CK3D}", NULL, "OK CONTINUE", false, true},
};

typedef enum {
    BOX_STAGE_INPUT,
    BOX_STAGE_LOCKED_FLASH,
    BOX_STAGE_ERROR,
    BOX_STAGE_TX_PENDING,
    BOX_STAGE_GRANTED,
    BOX_STAGE_DENIED,
} BoxStage;

static struct {
    char code[7];
    uint8_t pos;
    BoxStage stage;
    uint32_t stage_until;
    bool tx_refused;
} box;

void box_enter(bool skip_intro) {
    box.stage = BOX_STAGE_INPUT;
    if(!skip_intro) {
        strlcpy(box.code, "AAAAAA", sizeof(box.code));
        box.pos = 0;
        story_start(BOX_INTRO, 2, NULL);
    }
}

static uint32_t fnv1a(const uint8_t* data, size_t len) {
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

bool box_code_is_correct(const char* code) {
    return fnv1a((const uint8_t*)code, 6) == 0x0F044E3B;
}

static void box_transmit(void) {
    // Plain-text code for the physical BOX at the venue, same frame as the badge.
    uint8_t payload[32];
    memset(payload, 0, sizeof(payload));
    memcpy(payload, box.code, 6);
    memcpy(payload + 6, prefs.nick, 8);
    const uint16_t id = self_short_id();
    payload[14] = (uint8_t)(id & 0xFF);
    payload[15] = (uint8_t)(id >> 8);

    box.tx_refused = false;
    for(uint8_t i = 0; i < 5; i++) {
        if(!radio_send_fixed32(payload)) box.tx_refused = true;
        furi_delay_ms(50);
    }
}

void box_tick(const ButtonsEdge* b) {
    const uint32_t now = millis();

    switch(box.stage) {
    case BOX_STAGE_LOCKED_FLASH:
        if(!leds_flash_active()) {
            box.stage = BOX_STAGE_ERROR;
            box.stage_until = now + 800;
        }
        return;
    case BOX_STAGE_ERROR:
        if((int32_t)(now - box.stage_until) >= 0) {
            box.stage = BOX_STAGE_INPUT;
            story_start(BOX_NICE_TRY, 1, NULL);
        }
        return;
    case BOX_STAGE_TX_PENDING:
        // One tick late on purpose, so "TRANSMITTING..." reaches the screen before the blocking TX.
        if((int32_t)(now - box.stage_until) < 0) return;
        box_transmit();
        if(box_code_is_correct(box.code)) {
            box.stage = BOX_STAGE_GRANTED;
            box.stage_until = millis() + 1500;
            leds_access_effect(true);
        } else {
            box.stage = BOX_STAGE_DENIED;
            box.stage_until = millis() + 1200;
            leds_access_effect(false);
        }
        return;
    case BOX_STAGE_GRANTED:
        if((int32_t)(now - box.stage_until) >= 0) {
            box.stage = BOX_STAGE_INPUT;
            box.pos = 0;
            story_start(BOX_FLAG, 1, NULL);
        }
        return;
    case BOX_STAGE_DENIED:
        if((int32_t)(now - box.stage_until) >= 0) {
            box.stage = BOX_STAGE_INPUT;
            box.pos = 0;
        }
        return;
    case BOX_STAGE_INPUT:
    default:
        break;
    }

    if(b->back) {
        strlcpy(box.code, "AAAAAA", sizeof(box.code));
        box.pos = 0;
        go_to(MODE_MENU, false);
        return;
    }
    if(b->left) box.pos = (box.pos == 0) ? 5 : (box.pos - 1);
    if(b->right) box.pos = (box.pos + 1) % 6;
    if(b->up) {
        char* ch = &box.code[box.pos];
        *ch = (*ch >= 'A' && *ch < 'Z') ? (char)(*ch + 1) : 'A';
    }
    if(b->down) {
        char* ch = &box.code[box.pos];
        *ch = (*ch > 'A' && *ch <= 'Z') ? (char)(*ch - 1) : 'Z';
    }
    if(b->ok_short) box.pos = (box.pos + 1) % 6;

    if(hold_once(InputKeyOk, 800)) {
        if(!prefs.key_verified) {
            leds_flash(255, 80, 0, 80, 80, 4, 400);
            box.stage = BOX_STAGE_LOCKED_FLASH;
        } else {
            box.stage = BOX_STAGE_TX_PENDING;
            box.stage_until = now + 60;
        }
    }
}

void box_draw(Canvas* c) {
    ui_title(c, "THE BOX");

    switch(box.stage) {
    case BOX_STAGE_LOCKED_FLASH:
    case BOX_STAGE_ERROR:
        ui_center(c, 38, FontPrimary, "ERROR!");
        return;
    case BOX_STAGE_TX_PENDING:
        ui_center(c, 38, FontPrimary, "TRANSMITTING...");
        return;
    case BOX_STAGE_GRANTED:
        ui_center(c, 32, FontPrimary, "TRANSMITTING...");
        ui_center(c, 46, FontPrimary, "ACCESS GRANTED");
        if(box.tx_refused) ui_tiny_center(c, SCREEN_W / 2, 50, "TX REFUSED IN THIS REGION");
        return;
    case BOX_STAGE_DENIED:
        ui_center(c, 32, FontPrimary, "TRANSMITTING...");
        ui_center(c, 46, FontPrimary, "ACCESS DENIED");
        return;
    case BOX_STAGE_INPUT:
    default:
        break;
    }

    canvas_draw_rframe(c, 8, 18, SCREEN_W - 16, 32, 2);
    ui_tiny(c, 2, 12, "ENTER THE CODE");
    canvas_set_font(c, FontPrimary);
    const int step = 16;
    const int base = (SCREEN_W - 6 * step) / 2 + 5;
    for(uint8_t i = 0; i < 6; i++) {
        char s[2] = {box.code[i], 0};
        canvas_draw_str(c, base + i * step, 38, s);
    }
    canvas_draw_box(c, base - 1 + box.pos * step, 41, 9, 2);
    ui_footer(c, "U/D EDIT L/R MOVE HOLD OK SEND");
}
