// arduino nano v3
constexpr uint8_t PIN_SPK = 11;

constexpr uint8_t PIN_R1 = 10;
constexpr uint8_t PIN_R2 = 9;
constexpr uint8_t PIN_R3 = 8;
constexpr uint8_t PIN_R4 = 7;

constexpr uint8_t PIN_C1 = 3;
constexpr uint8_t PIN_C2 = 5;
constexpr uint8_t PIN_C3 = 4;

constexpr uint8_t BUTTON_INVALID = 255;
constexpr uint8_t BUTTON_ASTERISK = 10;
constexpr uint8_t BUTTON_HASHTAG = 11;

// common
constexpr uint8_t COLS[] = { PIN_C1, PIN_C2, PIN_C3 };
constexpr size_t COLS_SIZE = sizeof(COLS) / sizeof(COLS[0]);
constexpr uint8_t ROWS[] = { PIN_R1, PIN_R2, PIN_R3, PIN_R4 };
constexpr size_t ROWS_SIZE = sizeof(ROWS) / sizeof(ROWS[0]);
char buffer[100] = { 0 };

constexpr size_t VALUE[4][3] = {
  { 1, 2, 3 },                             // R1
  { 4, 5, 6 },                             // R2
  { 7, 8, 9 },                             // R3
  { BUTTON_ASTERISK, 0, BUTTON_HASHTAG },  // R4
};

class ButtonMatrix {
public:
  void read() {
    for (size_t c = 0; c < COLS_SIZE; ++c) {
      digitalWrite(COLS[c], HIGH);
      for (size_t r = 0; r < ROWS_SIZE; ++r) {
        if (digitalRead(ROWS[r]) == HIGH) {
          m_pressed_button = VALUE[r][c];
        }
      }
      digitalWrite(COLS[c], LOW);
    }
  }
  bool getPressedButton(size_t &button) const {
    button = m_pressed_button;
    return m_pressed_button != BUTTON_INVALID;
  }
  void reset() {
    m_pressed_button = BUTTON_INVALID;
  }
private:
  size_t m_pressed_button = BUTTON_INVALID;
};


void setup() {
  // serial
  Serial.begin(9600);
  while (!Serial) {
    delay(100);  // ms
  }
  Serial.println("ready");

  // declare pins
  // speaker
  pinMode(PIN_SPK, OUTPUT);
  // number pad columns with diodes
  pinMode(PIN_C1, OUTPUT);
  pinMode(PIN_C2, OUTPUT);
  pinMode(PIN_C3, OUTPUT);
  // number pad rows with pulldown resistors
  pinMode(PIN_R1, INPUT);
  pinMode(PIN_R2, INPUT);
  pinMode(PIN_R3, INPUT);
  pinMode(PIN_R4, INPUT);

  // init all outputs to low
  digitalWrite(PIN_SPK, LOW);
  for (size_t c = 0; c < COLS_SIZE; ++c) {
    digitalWrite(COLS[c], LOW);
  }
}

// notes
constexpr unsigned int NOTE_C3 = 131;
constexpr unsigned int NOTE_D3 = 147;
constexpr unsigned int NOTE_E3 = 165;
constexpr unsigned int NOTE_F3 = 175;
constexpr unsigned int NOTE_G3 = 196;
constexpr unsigned int NOTE_A3 = 220;
constexpr unsigned int NOTE_B3 = 247;

constexpr unsigned int NOTE_C4 = 262;
constexpr unsigned int NOTE_D4 = 294;
constexpr unsigned int NOTE_E4 = 330;
constexpr unsigned int NOTE_F4 = 349;
constexpr unsigned int NOTE_G4 = 392;
constexpr unsigned int NOTE_A4 = 440;
constexpr unsigned int NOTE_B4 = 494;

constexpr unsigned int NOTES[] = {
  NOTE_C3,
  NOTE_D3,
  NOTE_E3,
  NOTE_F3,
  NOTE_G3,
  NOTE_A3,
  NOTE_B3,
  NOTE_C4,
  NOTE_D4,
  NOTE_E4,
  NOTE_F4,
  NOTE_G4,
  NOTE_A4,
  NOTE_B4,
};

void loop() {
  ButtonMatrix bm;
  size_t last_button = BUTTON_INVALID;
  size_t button = 0;
  Serial.println("init");
  while (true) {
    bm.read();
    if (bm.getPressedButton(button)) {
      if (button != last_button) {
        snprintf(buffer, sizeof(buffer), "button: %d", button);
        Serial.println(buffer);
        last_button = button;
      }
      tone(PIN_SPK, NOTES[button], 200);
      last_button = button;
      bm.reset();
    } else {
      last_button = BUTTON_INVALID;
    }
  }
}
