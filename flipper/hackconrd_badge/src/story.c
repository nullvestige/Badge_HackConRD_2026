#include "story.h"
#include "leds.h"
#include "sound.h"
#include "ui.h"

static struct {
    const StoryPage* pages;
    uint8_t count;
    uint8_t idx;
    bool active;
    void (*done)(void);
} st;

static void story_show_current(void) {
    if(st.pages[st.idx].beep) {
        sound_beep(18);
        if(!leds_flash_active()) leds_accent_flash();
    }
}

void story_start(const StoryPage* pages, uint8_t count, void (*done)(void)) {
    st.pages = pages;
    st.count = count;
    st.idx = 0;
    st.active = true;
    st.done = done;
    story_show_current();
}

bool story_active(void) {
    return st.active;
}

static void story_finish(void) {
    st.active = false;
    if(st.done) {
        void (*done)(void) = st.done;
        st.done = NULL;
        done();
    }
}

void story_tick(const ButtonsEdge* b) {
    if(b->back) {
        story_finish();
        return;
    }
    if(b->ok) {
        st.idx++;
        if(st.idx >= st.count) {
            story_finish();
        } else {
            story_show_current();
        }
    }
}

void story_draw(Canvas* c) {
    const StoryPage* p = &st.pages[st.idx];
    ui_title(c, p->title);

    if(p->is_flag) {
        canvas_draw_rframe(c, 2, 20, SCREEN_W - 4, 20, 2);
        ui_center(c, 34, FontSecondary, p->big);
    } else {
        if(p->big && p->big[0]) ui_center(c, 22, FontPrimary, p->big);
        if(p->text && p->text[0]) ui_wrap(c, 2, 33, SCREEN_W - 4, 9, p->text, true);
    }
    ui_footer(c, p->footer ? p->footer : "OK CLOSE");
}
