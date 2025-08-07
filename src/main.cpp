#include <Arduino.h>
#include "pitches.h"
#include "utils.h"

#define BUZZER_PIN 18


void playWinMelody() {
  Serial.println("Playing win melody...");
  playMelody(BUZZER_PIN, win_melody, win_melody_len);
}

void playLoseMelody() {
  Serial.println("Playing lose melody...");
  playMelody(BUZZER_PIN, lose_melody, lose_melody_len);
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("ESP32 Buzzer Melody Test");
  Serial.println("Playing win melody in 2 seconds...");
  delay(2000);

  playWinMelody();

  delay(3000);

  Serial.println("Playing lose melody...");
  playLoseMelody();
}

void loop() {
  // Test melodies every 10 seconds
  delay(10000);

  Serial.println("Playing win melody...");
  playWinMelody();

  delay(3000);

  Serial.println("Playing lose melody...");
  playLoseMelody();
  Serial.println("Song played. No further action in loop.");
  delay(10000); // Prevents continuous looping, just for demonstration
}
