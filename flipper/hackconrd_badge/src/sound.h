// Non-blocking buzzer: looping melodies plus one-shot effects that preempt them.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t freq;
    uint16_t dur;
} Note;

void sound_tick(void);
void sound_stop_all(void);

void sound_music_start(const Note* melody, uint16_t len, bool loop);
void sound_music_stop(void);

// One-shot sequence (must point to static data). Silences music until it finishes.
void sound_sfx(const Note* notes, uint8_t len);
// One-shot steady tone.
void sound_tone(uint16_t freq, uint16_t ms);
// The badge's beep(): 1 kHz.
void sound_beep(uint16_t ms);
