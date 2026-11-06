#include "sound.h"
#include "badge.h"
#include "prefs.h"

static struct {
    const Note* melody;
    uint16_t melody_len;
    uint16_t melody_idx;
    bool melody_loop;
    bool melody_playing;
    uint32_t melody_next_ms;

    const Note* sfx;
    uint8_t sfx_len;
    uint8_t sfx_idx;
    uint32_t sfx_next_ms;
    bool sfx_playing;

    Note single;
    bool owned;
} s;

static bool sound_allowed(void) {
    // Stealth mode is the Flipper's system-wide silent switch; direct speaker use must honor it.
    return prefs.buzzer_on && !furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
}

static float sound_volume(void) {
    switch(prefs.buzzer_volume) {
    case 0:
        return 0.1f;
    case 2:
        return 1.0f;
    default:
        return 0.35f;
    }
}

static void speaker_off(void) {
    if(s.owned) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        s.owned = false;
    }
}

static void speaker_tone(uint16_t freq) {
    if(freq == 0 || !sound_allowed()) {
        if(s.owned) furi_hal_speaker_stop();
        return;
    }
    if(!s.owned) {
        if(!furi_hal_speaker_acquire(5)) return;
        s.owned = true;
    }
    furi_hal_speaker_start(freq, sound_volume());
}

void sound_tick(void) {
    const uint32_t now = millis();

    if(s.sfx_playing) {
        if((int32_t)(now - s.sfx_next_ms) < 0) return;
        if(s.sfx_idx >= s.sfx_len) {
            s.sfx_playing = false;
            speaker_tone(0);
            if(!s.melody_playing) speaker_off();
            return;
        }
        speaker_tone(s.sfx[s.sfx_idx].freq);
        s.sfx_next_ms = now + s.sfx[s.sfx_idx].dur;
        s.sfx_idx++;
        return;
    }

    if(!s.melody_playing) return;
    if((int32_t)(now - s.melody_next_ms) < 0) return;
    if(s.melody_idx >= s.melody_len) {
        if(!s.melody_loop) {
            sound_music_stop();
            return;
        }
        s.melody_idx = 0;
    }
    speaker_tone(s.melody[s.melody_idx].freq);
    s.melody_next_ms = now + s.melody[s.melody_idx].dur;
    s.melody_idx++;
}

void sound_stop_all(void) {
    s.melody_playing = false;
    s.sfx_playing = false;
    speaker_off();
}

void sound_music_start(const Note* melody, uint16_t len, bool loop) {
    s.melody = melody;
    s.melody_len = len;
    s.melody_idx = 0;
    s.melody_loop = loop;
    s.melody_next_ms = millis();
    s.melody_playing = true;
}

void sound_music_stop(void) {
    s.melody_playing = false;
    if(!s.sfx_playing) speaker_off();
}

void sound_sfx(const Note* notes, uint8_t len) {
    s.sfx = notes;
    s.sfx_len = len;
    s.sfx_idx = 0;
    s.sfx_next_ms = millis();
    s.sfx_playing = true;
}

void sound_tone(uint16_t freq, uint16_t ms) {
    s.single.freq = freq;
    s.single.dur = ms;
    sound_sfx(&s.single, 1);
}

void sound_beep(uint16_t ms) {
    sound_tone(1000, ms);
}
