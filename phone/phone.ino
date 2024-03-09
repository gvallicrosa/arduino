// arduino nano v3
constexpr uint8_t PIN_SS = 10;

constexpr uint8_t PIN_SPK = 9;

constexpr uint8_t PIN_R1 = 8;
constexpr uint8_t PIN_R2 = 7;
constexpr uint8_t PIN_R3 = 6;
constexpr uint8_t PIN_R4 = 5;

constexpr uint8_t PIN_C1 = 2;
constexpr uint8_t PIN_C2 = 3;
constexpr uint8_t PIN_C3 = 4;

constexpr uint8_t PIN_HANG = A0;

// include
#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>
#include "./button_matrix.h"
#include "./caller.h"

// common
TMRpcm player;
bm::ButtonMatrix bmatrix;
Caller caller;

void setup() {
  // pins
  bmatrix.setup();
  pinMode(PIN_HANG, INPUT_PULLUP);  // hang phone

  // serial
  Serial.begin(9600);
  while (!Serial) {
    delay(100);  // ms
  }
  Serial.println("init");

  // sd
  delay(500);
  if (!SD.begin(PIN_SS)) {
    Serial.println("SD init failed!");
    while (true) {};
  }

  // gather sound files
  caller.init();

  // wav player
  delay(500);
  player.speakerPin = PIN_SPK;  // PWM pin for audio output
  player.setVolume(6);          // 0 to 7 volume level for output
  player.quality(1);
}


enum class State {
  Hang,
  Pickup,
  Welcome,
  Dialling,
  Response,
  WaitToHang,
};

State state = State::Hang;
unsigned long last_time = 0;

void loop() {
  const bool is_hang = (digitalRead(PIN_HANG) == LOW);  // pullup
  if ((state != State::Hang) && is_hang) {
    // global change from any state to hang
    player.stopPlayback();
    caller.reset();
    state = State::Hang;
    Serial.println("state: hang");
    return;  // nothing else to do
  }

  // normal state machine
  switch (state) {
    case State::Hang:
      // wait for pickup
      if (!is_hang) {
        last_time = millis();  // delay intro play
        state = State::Pickup;
        Serial.println("state: pickup");
      }
      break;
    case State::Pickup:
      // wait 1s to start welcome message play
      if (millis() - last_time > 1000) {
        player.play("INTRO.WAV");
        state = State::Welcome;
        Serial.println("state: welcome");
      }
      break;
    case State::Welcome:
      // wait for welcome message play to finish
      if (!player.isPlaying()) {
        player.disable();  // allow tone to play in same speaker pin
        delay(300);        // wait a bit to take effect
        state = State::Dialling;
        Serial.println("state: dialling");
      }
      break;
    case State::Dialling:
      // read button presses until a full phone number is dialled
      if (caller.addNumber(bmatrix.read())) {
        caller.call(player);
        state = State::Response;
        Serial.println("state: response");
      }
      break;
    case State::Response:
      // wait for response from phone to finish
      if (!player.isPlaying()) {
        state = State::WaitToHang;
        Serial.println("state: wait to hang");
      }
      break;
    case State::WaitToHang:
      // wait for phone to be hanged
      break;
  }
}
