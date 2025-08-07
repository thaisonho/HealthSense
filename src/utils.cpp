#include "utils.h"
#include <Arduino.h>

// Win melody
const int win_melody[] = {
    NOTE_FS5, REST,    REST,    REST,    REST,     REST,     NOTE_D5,  REST,
    REST,     REST,    REST,    REST,    REST,     NOTE_D5,  NOTE_E5,  NOTE_F5,
    REST,     REST,    NOTE_E5, REST,    REST,     NOTE_D5,  REST,     NOTE_CS5,
    REST,     REST,    NOTE_D5, REST,    REST,     NOTE_E5,  REST,     NOTE_FS5,
    REST,     REST,    REST,    REST,    REST,     NOTE_B5,  REST,     REST,
    REST,     REST,    REST,    NOTE_B4, REST,     NOTE_CS5, REST,     NOTE_D5,
    REST,     REST,    NOTE_E5, REST,    REST,     NOTE_D5,  REST,     NOTE_CS5,
    REST,     REST,    NOTE_A5, REST,    REST,     NOTE_G5,  REST,     NOTE_FS5,
    REST,     REST,    REST,    REST,    REST,     NOTE_D5,  REST,     REST,
    REST,     REST,    REST,    REST,    NOTE_D5,  NOTE_E5,  NOTE_F5,  REST,
    REST,     NOTE_E5, REST,    REST,    NOTE_D5,  REST,     NOTE_CS5, REST,
    REST,     NOTE_D5, REST,    REST,    NOTE_E5,  REST,     NOTE_FS5, REST,
    REST,     REST,    REST,    REST,    NOTE_B5,  REST,     REST,     REST,
    REST,     REST,    NOTE_B5, REST,    NOTE_CS6, REST,     NOTE_D6,  REST,
    REST,     NOTE_G6, REST,    REST,    NOTE_FS6, REST,     NOTE_F6,  REST,
    REST,     NOTE_D6, REST,    REST,    NOTE_AS5, REST,     NOTE_B5
};

// Lose melody
const int lose_melody[] = {
    NOTE_A4,  REST,     REST,     NOTE_B4,  REST,     REST,     NOTE_D5,
    REST,     REST,     NOTE_B4,  REST,     REST,     NOTE_FS5, REST,
    REST,     REST,     REST,     NOTE_FS5, REST,     REST,     REST,
    REST,     NOTE_E5,  REST,     REST,     NOTE_A4,  REST,     REST,
    NOTE_B4,  REST,     REST,     NOTE_D5,  REST,     REST,     NOTE_B4,
    NOTE_E5,  REST,     REST,     REST,     REST,     NOTE_E5,  REST,
    REST,     REST,     REST,     NOTE_D5,  REST,     REST,     REST,
    REST,     NOTE_CS5, REST,     NOTE_B4,  REST,     REST,     REST,
    REST,     NOTE_A4,  REST,     REST,     NOTE_B4,  REST,     REST,
    NOTE_D5,  REST,     REST,     NOTE_B4,  NOTE_D5,  REST,     REST,
    NOTE_E5,  REST,     REST,     NOTE_CS5, REST,     REST,     REST,
    REST,     NOTE_B4,  REST,     REST,     NOTE_A4,  REST,     REST,
    NOTE_A4,  REST,     REST,     NOTE_A4,  REST,     REST,     NOTE_E5,
    REST,     REST,     NOTE_D5,  REST,     REST,     NOTE_A4,  REST,
    REST,     NOTE_B4,  REST,     REST,     NOTE_D5,  REST,     REST,
    NOTE_B4,  NOTE_FS5, REST,     REST,     REST,     REST,     NOTE_FS5,
    REST,     REST,     REST,     REST,     NOTE_E5,  REST,     REST,
    NOTE_A4,  NOTE_B4,  NOTE_D5,  NOTE_B4,  NOTE_A5,  NOTE_CS5, NOTE_D5,
    REST,     REST,     REST,     REST,     NOTE_CS5, NOTE_B4,  NOTE_A4,
    NOTE_B4,  NOTE_D5,  NOTE_B4
};

const int win_melody_len = sizeof(win_melody) / sizeof(win_melody[0]);
const int lose_melody_len = sizeof(lose_melody) / sizeof(lose_melody[0]);

void playMelody(int buzzer_pin, const int melody[], int melody_length, int tempo_divisor) {
    const int base_note_len_ms = 1000 / tempo_divisor;
    const int time_per_note_ms = (int)(base_note_len_ms * 1.3);

    for (int i = 0; i < melody_length; i++) {
        int note = melody[i];

        if (note != REST) {
            tone(buzzer_pin, note, base_note_len_ms);
        }

        delay(time_per_note_ms);
        noTone(buzzer_pin);
    }
}