// HackConRD 2026 badge - Flipper Zero port. App core: event loop, navigation and lock screen policy.
#include "badge.h"
#include "features.h"
#include "leds.h"
#include "prefs.h"
#include "radio.h"
#include "sound.h"
#include "story.h"
#include "ui.h"

#include <notification/notification_messages.h>

#define TICK_MS          20
#define DRAW_EVERY_TICKS 2

#define IDLE_SAVER_MS      (60UL * 1000UL)
#define IDLE_SAVER_GAME_MS (5UL * 60UL * 1000UL)

#define INPUT_KEYS 6

static struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool running;

    ScreenMode mode;
    ScreenMode mode_before_saver;
    bool saver_active;
    bool backlight_forced;
    uint32_t last_activity_ms;
    uint16_t short_id;

    bool pressed[INPUT_KEYS];
    bool consumed[INPUT_KEYS];
    uint32_t press_ms[INPUT_KEYS];
    ButtonsEdge edges;

    char toast_title[20];
    char toast_msg[24];
    uint32_t toast_until;
    bool toast_is_full;
} app;

// --- Input ------------------------------------------------------------------

bool is_pressed(InputKey key) {
    return app.pressed[key];
}

bool hold_once(InputKey key, uint16_t hold_ms) {
    if(!app.pressed[key] || app.consumed[key]) return false;
    if(millis() - app.press_ms[key] < hold_ms) return false;
    app.consumed[key] = true;
    return true;
}

// A key still down when the screen changes belongs to the old screen: its release must not count
// as a short press on the new one, and it must not start a hold there.
void input_consume_held(void) {
    for(uint8_t i = 0; i < INPUT_KEYS; i++) {
        if(app.pressed[i]) app.consumed[i] = true;
    }
}

static void input_handle(const InputEvent* ev) {
    if(ev->key >= INPUT_KEYS) return;
    if(ev->type == InputTypePress) {
        app.pressed[ev->key] = true;
        app.consumed[ev->key] = false;
        app.press_ms[ev->key] = millis();
        app.edges.any = true;
        switch(ev->key) {
        case InputKeyUp:
            app.edges.up = true;
            break;
        case InputKeyDown:
            app.edges.down = true;
            break;
        case InputKeyLeft:
            app.edges.left = true;
            break;
        case InputKeyRight:
            app.edges.right = true;
            break;
        case InputKeyOk:
            app.edges.ok = true;
            break;
        case InputKeyBack:
            app.edges.back = true;
            break;
        default:
            break;
        }
    } else if(ev->type == InputTypeRelease) {
        app.pressed[ev->key] = false;
        if(!app.consumed[ev->key]) {
            if(ev->key == InputKeyOk) app.edges.ok_short = true;
            if(ev->key == InputKeyBack) app.edges.back_short = true;
        }
    }
}

// --- Identity ---------------------------------------------------------------

uint16_t self_short_id(void) {
    return app.short_id;
}

static uint16_t calc_short_id(void) {
    // Same folding as calcSelfShortId(), over the device UID instead of the ESP32 MAC.
    const uint8_t* uid = furi_hal_version_uid();
    const size_t size = furi_hal_version_uid_size();
    uint16_t hash = 0;
    for(size_t i = 0; i + 1 < size; i += 2) {
        hash ^= (uint16_t)uid[i] | ((uint16_t)uid[i + 1] << 8);
    }
    if(hash == 0 || hash == 0xFFFF) hash = 0xBEEF;
    return hash;
}

// --- Idle / lock screen -----------------------------------------------------

void note_activity(void) {
    app.last_activity_ms = millis();
}

void reset_idle_timer(void) {
    note_activity();
}

uint32_t idle_elapsed_ms(void) {
    return millis() - app.last_activity_ms;
}

static bool mode_is_game(ScreenMode m) {
    return m == MODE_DINO || m == MODE_MAZE || m == MODE_HACKMON || m == MODE_STAR_INVADERS;
}

// CTF roles run for a long time with nobody touching the Flipper, and the lock screen turns the
// radio off, which would end the role.
static bool mode_is_ctf_radio(ScreenMode m) {
    return m == MODE_CTF_BEARER || m == MODE_CTF_CROWD || m == MODE_CTF_BOX || m == MODE_CTF_LAB;
}

static void backlight_policy_apply(bool keep_on);

static void saver_enter(void) {
    if(app.saver_active) return;
    app.mode_before_saver = app.mode;
    app.saver_active = true;
    sound_stop_all();
    if(radio_is_on()) radio_power_down();
    backlight_policy_apply(false);
    lock_load_art(prefs.saver);
}

static void saver_exit(void) {
    if(!app.saver_active) return;
    app.saver_active = false;
    note_activity();
    go_to(app.mode_before_saver, true);
}

// Enter the lock screen right away, as the badge does on boot and on a held BACK in the main menu.
void lock_now(void) {
    saver_enter();
}

static void saver_policy_tick(void) {
    if(app.saver_active) return;
    if(app.mode == MODE_FIRST_BOOT || app.mode == MODE_BOOT) return;
    // The badge also sleeps here, which cuts a hunt short while walking without touching keys.
    if(app.mode == MODE_FOX_HUNT) return;
    if(mode_is_ctf_radio(app.mode)) return;
    const uint32_t limit = mode_is_game(app.mode) ? IDLE_SAVER_GAME_MS : IDLE_SAVER_MS;
    if(idle_elapsed_ms() > limit) saver_enter();
}

// --- Toasts -----------------------------------------------------------------

void toast_full(const char* title, const char* msg, uint16_t ms) {
    strlcpy(app.toast_title, title, sizeof(app.toast_title));
    strlcpy(app.toast_msg, msg, sizeof(app.toast_msg));
    app.toast_until = millis() + ms;
    app.toast_is_full = true;
}

void toast_small(const char* msg, uint16_t ms) {
    app.toast_title[0] = 0;
    strlcpy(app.toast_msg, msg, sizeof(app.toast_msg));
    app.toast_until = millis() + ms;
    app.toast_is_full = false;
}

static bool toast_active(void) {
    return app.toast_until != 0 && (int32_t)(millis() - app.toast_until) < 0;
}

// --- Navigation -------------------------------------------------------------

static bool rf_wanted_for_mode(ScreenMode m) {
    switch(m) {
    case MODE_FOX_HUNT:
    case MODE_LEARNING:
    case MODE_THE_BOX:
    case MODE_FRIENDS_MENU:
    case MODE_FRIENDS_SCAN:
    case MODE_FRIENDS_REQUESTING:
    case MODE_FRIENDS_LIST:
    case MODE_FRIEND_REQ_PROMPT:
    case MODE_CTF_BEARER:
    case MODE_CTF_CROWD:
    case MODE_CTF_BOX:
    case MODE_CTF_LAB:
        return true;
    default:
        return false;
    }
}

static bool frf_wanted_for_mode(ScreenMode m) {
    switch(m) {
    case MODE_FRIENDS_MENU:
    case MODE_FRIENDS_SCAN:
    case MODE_FRIENDS_REQUESTING:
    case MODE_FRIENDS_LIST:
    case MODE_FRIEND_REQ_PROMPT:
        return true;
    default:
        return false;
    }
}

static RadioProfile profile_for_mode(ScreenMode m) {
    if(m == MODE_FOX_HUNT || m == MODE_CTF_BEARER) return PROFILE_FOX_OOK_RSSI;
    if(m == MODE_LEARNING || m == MODE_CTF_LAB) return PROFILE_PACKET_GFSK;
    return PROFILE_FRIENDS_GFSK_32B; // Friends, THE BOX and the CTF roles that stand in for them
}

static void games_leave(ScreenMode m) {
    switch(m) {
    case MODE_DINO:
        dino_exit();
        break;
    case MODE_MAZE:
        maze_exit();
        break;
    case MODE_HACKMON:
        hackermon_exit();
        break;
    case MODE_STAR_INVADERS:
        invaders_exit();
        break;
    default:
        break;
    }
}

// Games, hunts and CTF roles are watched for long stretches without a key press. The Bearer is the
// exception: it is hidden, and a lit screen would give it away.
static bool mode_keeps_backlight(ScreenMode m) {
    return mode_is_game(m) || m == MODE_FOX_HUNT || m == MODE_FOX_SIM || m == MODE_CTF_CROWD ||
           m == MODE_CTF_BOX || m == MODE_CTF_LAB;
}

static void backlight_policy_apply(bool keep_on) {
    if(keep_on == app.backlight_forced) return;
    app.backlight_forced = keep_on;
    notification_message(
        app.notifications,
        keep_on ? &sequence_display_backlight_enforce_on : &sequence_display_backlight_enforce_auto);
}

ScreenMode current_mode(void) {
    return app.mode;
}

void request_app_exit(void) {
    app.running = false;
}

void go_to(ScreenMode m, bool skip_intro) {
    const ScreenMode prev = app.mode;
    const bool prev_rf = rf_wanted_for_mode(prev);
    const bool next_rf = rf_wanted_for_mode(m);

    input_consume_held();
    if(prev != m) games_leave(prev);
    if((prev == MODE_FOX_HUNT || prev == MODE_FOX_SIM) && m != prev) fox_exit();
    if(prev == MODE_CTF_BEARER && m != prev) ctf_bearer_exit();

    app.mode = m;

    backlight_policy_apply(mode_keeps_backlight(m));

    if(next_rf) {
        // The radio is re-tuned on every RF mode change, so each module starts from a clean state.
        radio_power_up(profile_for_mode(m));
        if(!prev_rf && !skip_intro) toast_full("RADIO", "RX ON", 500);
    } else {
        if(radio_is_on()) radio_power_down();
        if(prev_rf && !skip_intro) toast_full("RADIO", "RX OFF", 500);
    }

    switch(m) {
    case MODE_BOOT:
        boot_enter();
        break;
    case MODE_FOX_HUNT:
        note_activity();
        fox_enter(skip_intro);
        break;
    case MODE_FOX_SIM:
        note_activity();
        fox_sim_enter(skip_intro);
        break;
    case MODE_FRIENDS_MENU:
    case MODE_FRIENDS_SCAN:
    case MODE_FRIENDS_REQUESTING:
    case MODE_FRIENDS_LIST:
    case MODE_FRIEND_REQ_PROMPT:
        friends_on_enter(m);
        break;
    case MODE_SETTINGS:
        settings_enter();
        break;
    case MODE_THE_KEY:
        note_activity();
        key_enter(skip_intro);
        break;
    case MODE_THE_BOX:
        note_activity();
        box_enter(skip_intro);
        break;
    case MODE_LEARNING:
        note_activity();
        learning_enter(skip_intro);
        break;
    case MODE_FIRST_BOOT:
        first_boot_enter();
        break;
    case MODE_DINO:
        note_activity();
        dino_enter(skip_intro);
        break;
    case MODE_MAZE:
        note_activity();
        maze_enter(skip_intro);
        break;
    case MODE_HACKMON:
        note_activity();
        hackermon_enter(skip_intro);
        break;
    case MODE_STAR_INVADERS:
        note_activity();
        invaders_enter(skip_intro);
        break;
    case MODE_CTF_BEARER:
        note_activity();
        ctf_bearer_enter(skip_intro);
        break;
    case MODE_CTF_CROWD:
        note_activity();
        ctf_crowd_enter(skip_intro);
        break;
    case MODE_CTF_BOX:
        note_activity();
        ctf_box_enter(skip_intro);
        break;
    case MODE_CTF_LAB:
        note_activity();
        lab_partner_enter(skip_intro);
        break;
    default:
        break;
    }
}

// --- Tick -------------------------------------------------------------------

static void mode_tick(const ButtonsEdge* b) {
    switch(app.mode) {
    case MODE_BOOT:
        boot_tick(b);
        break;
    case MODE_MENU:
        menu_tick(b);
        break;
    case MODE_HUNT_MENU:
        hunt_menu_tick(b);
        break;
    case MODE_FOX_HUNT:
        fox_tick(b);
        break;
    case MODE_FOX_SIM:
        fox_sim_tick(b);
        break;
    case MODE_FRIENDS_MENU:
    case MODE_FRIENDS_SCAN:
    case MODE_FRIENDS_REQUESTING:
    case MODE_FRIENDS_LIST:
    case MODE_FRIEND_REQ_PROMPT:
        friends_tick(app.mode, b);
        break;
    case MODE_THE_BOX:
        box_tick(b);
        break;
    case MODE_LEARNING:
        learning_tick(b);
        break;
    case MODE_SETTINGS:
        settings_tick(b);
        break;
    case MODE_THE_KEY:
        key_tick(b);
        break;
    case MODE_FIRST_BOOT:
        first_boot_tick(b);
        break;
    case MODE_GAMES_MENU:
        games_menu_tick(b);
        break;
    case MODE_DINO:
        dino_tick(b);
        break;
    case MODE_MAZE:
        maze_tick(b);
        break;
    case MODE_HACKMON:
        hackermon_tick(b);
        break;
    case MODE_STAR_INVADERS:
        invaders_tick(b);
        break;
    case MODE_CTF_MENU:
        ctf_menu_tick(b);
        break;
    case MODE_CTF_BEARER:
        ctf_bearer_tick(b);
        break;
    case MODE_CTF_CROWD:
        ctf_crowd_tick(b);
        break;
    case MODE_CTF_KEY:
        ctf_key_tick(b);
        break;
    case MODE_CTF_BOX:
        ctf_box_tick(b);
        break;
    case MODE_CTF_LAB:
        lab_partner_tick(b);
        break;
    }
}

static void badge_tick(void) {
    ButtonsEdge b = app.edges;
    memset(&app.edges, 0, sizeof(app.edges));

    if(b.any) note_activity();

    sound_tick();
    leds_tick();
    saver_policy_tick();

    if(app.saver_active) {
        if(b.any) saver_exit();
        return;
    }

    if(frf_wanted_for_mode(app.mode)) friends_background_tick();

    if(toast_active() && app.toast_is_full) {
        input_consume_held();
        return;
    }
    if(story_active()) {
        story_tick(&b);
        input_consume_held();
        return;
    }

    mode_tick(&b);
}

// --- Drawing ----------------------------------------------------------------

static void mode_draw(Canvas* c) {
    switch(app.mode) {
    case MODE_BOOT:
        boot_draw(c);
        break;
    case MODE_MENU:
        menu_draw(c);
        break;
    case MODE_HUNT_MENU:
        hunt_menu_draw(c);
        break;
    case MODE_FOX_HUNT:
        fox_draw(c);
        break;
    case MODE_FOX_SIM:
        fox_sim_draw(c);
        break;
    case MODE_FRIENDS_MENU:
    case MODE_FRIENDS_SCAN:
    case MODE_FRIENDS_REQUESTING:
    case MODE_FRIENDS_LIST:
    case MODE_FRIEND_REQ_PROMPT:
        friends_draw(app.mode, c);
        break;
    case MODE_THE_BOX:
        box_draw(c);
        break;
    case MODE_LEARNING:
        learning_draw(c);
        break;
    case MODE_SETTINGS:
        settings_draw(c);
        break;
    case MODE_THE_KEY:
        key_draw(c);
        break;
    case MODE_FIRST_BOOT:
        first_boot_draw(c);
        break;
    case MODE_GAMES_MENU:
        games_menu_draw(c);
        break;
    case MODE_DINO:
        dino_draw(c);
        break;
    case MODE_MAZE:
        maze_draw(c);
        break;
    case MODE_HACKMON:
        hackermon_draw(c);
        break;
    case MODE_STAR_INVADERS:
        invaders_draw(c);
        break;
    case MODE_CTF_MENU:
        ctf_menu_draw(c);
        break;
    case MODE_CTF_BEARER:
        ctf_bearer_draw(c);
        break;
    case MODE_CTF_CROWD:
        ctf_crowd_draw(c);
        break;
    case MODE_CTF_KEY:
        ctf_key_draw(c);
        break;
    case MODE_CTF_BOX:
        ctf_box_draw(c);
        break;
    case MODE_CTF_LAB:
        lab_partner_draw(c);
        break;
    }
}

static void draw_callback(Canvas* c, void* ctx) {
    UNUSED(ctx);
    furi_check(furi_mutex_acquire(app.mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    if(app.saver_active) {
        lock_draw(c);
    } else if(toast_active() && app.toast_is_full) {
        ui_title(c, app.toast_title);
        ui_center(c, 40, FontPrimary, app.toast_msg);
    } else {
        if(story_active()) {
            story_draw(c);
        } else {
            mode_draw(c);
        }
        if(toast_active()) {
            ui_dialog_box(c, 14, 22, SCREEN_W - 28, 16);
            ui_center(c, 34, FontPrimary, app.toast_msg);
        }
    }

    furi_mutex_release(app.mutex);

    if(ui_is_dark()) {
        canvas_set_color(c, ColorXOR);
        canvas_draw_box(c, 0, 0, SCREEN_W, SCREEN_H);
        canvas_set_color(c, ColorBlack);
    }
}

static void input_callback(InputEvent* ev, void* ctx) {
    UNUSED(ctx);
    furi_message_queue_put(app.queue, ev, 0);
}

// --- Entry point ------------------------------------------------------------

// Lets the system close the app cleanly, for example `loader close` or a companion app.
static bool signal_callback(uint32_t signal, void* arg, void* ctx) {
    UNUSED(arg);
    UNUSED(ctx);
    if(signal != FuriSignalExit) return false;
    app.running = false;
    return true;
}

int32_t hackconrd_badge_app(void* p) {
    UNUSED(p);
    memset(&app, 0, sizeof(app));
    furi_thread_set_signal_callback(furi_thread_get_current(), signal_callback, NULL);

    app.queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app.short_id = calc_short_id();
    app.running = true;
    app.mode = MODE_BOOT;

    prefs_load();
    leds_init();
    note_activity();

    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, draw_callback, NULL);
    view_port_input_callback_set(app.view_port, input_callback, NULL);
    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    app.notifications = furi_record_open(RECORD_NOTIFICATION);

    boot_enter();

    uint32_t next_tick = furi_get_tick();
    uint8_t draw_div = 0;

    while(app.running) {
        const uint32_t now = furi_get_tick();
        const int32_t wait = (int32_t)(next_tick - now);
        InputEvent ev;
        if(furi_message_queue_get(app.queue, &ev, wait > 0 ? (uint32_t)wait : 0) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(app.mutex, FuriWaitForever) == FuriStatusOk);
            input_handle(&ev);
            furi_mutex_release(app.mutex);
            continue;
        }

        next_tick = furi_get_tick() + TICK_MS;

        furi_check(furi_mutex_acquire(app.mutex, FuriWaitForever) == FuriStatusOk);
        badge_tick();
        furi_mutex_release(app.mutex);

        if(++draw_div >= DRAW_EVERY_TICKS) {
            draw_div = 0;
            view_port_update(app.view_port);
        }
    }

    games_leave(app.mode);
    sound_stop_all();
    if(radio_is_on()) radio_power_down();
    backlight_policy_apply(false);
    leds_deinit();

    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_message_queue_free(app.queue);
    furi_mutex_free(app.mutex);

    return 0;
}
