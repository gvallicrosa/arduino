// #include <cstdint>
#pragma once

namespace bm {

constexpr uint8_t COLS[] = { PIN_C1, PIN_C2, PIN_C3 };
constexpr size_t COLS_SIZE = sizeof(COLS) / sizeof(COLS[0]);
constexpr uint8_t ROWS[] = { PIN_R1, PIN_R2, PIN_R3, PIN_R4 };
constexpr size_t ROWS_SIZE = sizeof(ROWS) / sizeof(ROWS[0]);

constexpr uint8_t BUTTON_INVALID = 255;
constexpr uint8_t BUTTON_ASTERISK = 10;
constexpr uint8_t BUTTON_HASHTAG = 11;

constexpr size_t VALUE[ROWS_SIZE * COLS_SIZE] = {
  1, 2, 3,                             // R1
  4, 5, 6,                             // R2
  7, 8, 9,                             // R3
  BUTTON_ASTERISK, 0, BUTTON_HASHTAG,  // R4
};

size_t toIndex(size_t row, size_t col) {
  return row * COLS_SIZE + col;
}


struct ButtonMatrix {
  void setup() const {
    // number pad columns with diodes
    pinMode(PIN_C1, OUTPUT);
    pinMode(PIN_C2, OUTPUT);
    pinMode(PIN_C3, OUTPUT);
    for (size_t c = 0; c < COLS_SIZE; ++c) {
      digitalWrite(COLS[c], LOW);
    }
    // number pad rows with pulldown resistors
    pinMode(PIN_R1, INPUT);
    pinMode(PIN_R2, INPUT);
    pinMode(PIN_R3, INPUT);
    pinMode(PIN_R4, INPUT);
  }

  size_t read() const {
    size_t pressed_button = BUTTON_INVALID;
    for (size_t c = 0; c < COLS_SIZE; ++c) {
      digitalWrite(COLS[c], HIGH);
      for (size_t r = 0; r < ROWS_SIZE; ++r) {
        if (digitalRead(ROWS[r]) == HIGH) {
          pressed_button = VALUE[toIndex(r, c)];
        }
      }
      digitalWrite(COLS[c], LOW);
      delay(1);  // necessary to avoid multi-read
    }
    return pressed_button;
  }
};
}  // namespace bm