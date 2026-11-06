// The hidden "alien" quest that links Friends to THE KEY. Flags and thresholds match the badge.
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "story.h"

#define BRO_FLAG_ALIEN 0x80

// The firmware builds with BADGE_IS_ALIEN=1 by default, so every badge counts for the quest.
// Keeping it set lets a Flipper count toward a real badge's quest too.
uint8_t bro_encode(uint8_t role) {
    return role | BRO_FLAG_ALIEN;
}

static const StoryPage QUEST_UNLOCKED[] = {
    {"CHAPTER II",
     "THE SECRET",
     "A researcher forged a KEY, then vanished without a trace.",
     "OK NEXT  BACK SKIP",
     true,
     false},
    {"CAESARS",
     "FIND HIM",
     "He blends in with the crowd. Add him to your Friends list.",
     "OK NEXT  BACK SKIP",
     false,
     false},
    {"REMEMBER",
     "NO HINTS",
     "He will not explain anything. Keep what you gain.",
     "OK NEXT  BACK SKIP",
     false,
     false},
    {"FLAG 1 DISCOVERED", "HCRD{B4DGE-S3RC3T}", NULL, "OK CLOSE", false, true},
};

static const StoryPage QUEST_COMPLETED[] = {
    {"FOUND",
     "CAESARS",
     "You found Caesars. He handed you the KEY.",
     "OK NEXT  BACK SKIP",
     true,
     false},
    {"UNLOCKED",
     "KEY ACQUIRED",
     "A new module is now available in the Main Menu.",
     "OK NEXT  BACK SKIP",
     false,
     false},
    {"FLAG 2 DISCOVERED", "HCRD{CA3S4RS-F0UND}", NULL, "OK CLOSE", false, true},
};

static void quest_arm_if_needed(void) {
    if(prefs.qa_done || prefs.qa_armed) return;
    if(prefs.friends_count < QUEST_ALIEN_TRIGGER) return;
    prefs.qa_armed = 1;
    prefs.qa_left = QUEST_ALIEN_NEED;
    prefs.qa_seen_count = 0;
    memset(prefs.qa_seen, 0, sizeof(prefs.qa_seen));
    prefs_save();
}

static void go_main_menu(void) {
    go_to(MODE_MENU, false);
}

// Returns true when this friend completed the quest.
static bool quest_on_alien_added(uint16_t sid) {
    if(!prefs.qa_armed || prefs.qa_done || prefs.qa_left == 0) return false;

    for(uint8_t i = 0; i < prefs.qa_seen_count; i++) {
        if(prefs.qa_seen[i] == sid) return false;
    }
    if(prefs.qa_seen_count < QUEST_ALIEN_NEED) prefs.qa_seen[prefs.qa_seen_count++] = sid;
    if(prefs.qa_left > 0) prefs.qa_left--;

    if(prefs.qa_left == 0) {
        note_activity();
        prefs.qa_done = 1;
        prefs.qa_armed = 0;
        prefs_save();
        return true;
    }
    prefs_save();
    return false;
}

void quest_after_friend_added(uint16_t sid, uint8_t bro, bool added, bool was_armed, bool* story_started) {
    bool completed = false;
    if(added && was_armed && (bro & BRO_FLAG_ALIEN)) completed = quest_on_alien_added(sid);

    quest_arm_if_needed();

    if(!was_armed && prefs.qa_armed) {
        note_activity();
        leds_quest_alert();
        story_start(QUEST_UNLOCKED, 4, NULL);
        *story_started = true;
    }
    if(completed) {
        leds_quest_alert();
        story_start(QUEST_COMPLETED, 3, go_main_menu);
        *story_started = true;
    }
}
