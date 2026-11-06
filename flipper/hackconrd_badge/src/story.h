// Story pages of the CTF narrative. Modal overlay: OK advances, BACK skips the rest,
// matching storyFoxPage() on the badge.
#pragma once

#include "badge.h"

typedef struct {
    const char* title;
    const char* big;
    const char* text;
    const char* footer;
    bool beep;
    bool is_flag; // `big` holds the flag string, shown as a flag page
} StoryPage;

// Pages must be static. `done` runs when the story ends, whether finished or skipped.
void story_start(const StoryPage* pages, uint8_t count, void (*done)(void));
bool story_active(void);
void story_tick(const ButtonsEdge* b);
void story_draw(Canvas* c);
