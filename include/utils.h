#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "pitches.h"

// Declaration for the melody arrays
extern const int win_melody[];
extern const int win_melody_len;
extern const int lose_melody[];
extern const int lose_melody_len;

void playMelody(int buzzer_pin, const int melody[], int melody_length, int tempo_divisor = 25);

#endif // UTILS_H