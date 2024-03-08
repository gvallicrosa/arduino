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
File root;

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
  root = SD.open("/");
  Serial.println("dir:");
  printDirectory(root, 0);
  root.close();
  //caller.gatherSoundFiles(root);

  // wav player
  delay(500);
  player.speakerPin = PIN_SPK;  // PWM pin for audio output
  player.setVolume(6);          // 0 to 7 volume level for output
  player.quality(1);
  Serial.println("audio start");
  player.play("/INTRO.WAV");
  Serial.println("audio playing");
  while (player.isPlaying()) {
    delay(300);
  }
  Serial.println("audio end");
}


void printDirectory(SDLib::File& dir, int numTabs) {
  dir.rewindDirectory();
  while (true) {
    SDLib::File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }

    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
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
  } else {
    switch (state) {
      case State::Hang:
        // if picked up, change state
        if (!is_hang) {
          last_time = millis();  // delay intro play
          state = State::Pickup;
          Serial.println("state: pickup");
        }
        break;
      case State::Pickup:
        // wait 1s to start welcome play
        if (millis() - last_time > 1000) {
          player.play("INTRO.WAV");
          state = State::Welcome;
          Serial.println("state: welcome");
          Serial.println(player.isPlaying());
          delay(100);  // allow play to start
          Serial.println(player.isPlaying());
        }
        break;
      case State::Welcome:
        if (!player.isPlaying()) {
          state = State::Dialling;
          Serial.println("state: dialling");
        }
        break;
      case State::Dialling:
        if (caller.addNumber(bmatrix.read())) {
          caller.call(player);
          state = State::Response;
          Serial.println("state: response");
        }
        break;
      case State::Response:
        if (!player.isPlaying()) {
          state = State::WaitToHang;
          Serial.println("state: wait to hang");
        }
        break;
      case State::WaitToHang:
        break;
    }
  }
}
