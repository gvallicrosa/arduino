#include <SPI.h>               // rfid
#include <MFRC522.h>           // rfid
#include <Wire.h>              // lcd
#include <Adafruit_GFX.h>      // lcd
#include <Adafruit_SSD1306.h>  // lcd
#include "./image.h"           // lcd

// lcd
constexpr uint8_t LCD_ADDRESS = 0x3C;
constexpr uint8_t LCD_WIDTH = 128;
constexpr uint8_t LCD_HEIGHT = 32;
constexpr uint8_t LCD_RESET = -1;
Adafruit_SSD1306 display(LCD_WIDTH, LCD_HEIGHT, &Wire, LCD_RESET);

// rfid
constexpr uint8_t PIN_RST = 9;
constexpr uint8_t PIN_SS = 10;
const String UID_EYE("f3651cf7");
const String UID_CARD("3478125b");
// f3651cf7 keychain
// 3478125b card
MFRC522 mfrc522(PIN_SS, PIN_RST);

// TODO: invalid image

void showLocked() {
  display.clearDisplay();
  display.drawBitmap(
    display.width() - 1 - IMAGE_EYE_WIDTH,
    (display.height() - IMAGE_EYE_HEIGHT) / 2,
    IMAGE_EYE_DATA, IMAGE_EYE_WIDTH, IMAGE_EYE_HEIGHT,
    SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.write(" scan to");
  display.setCursor(0, 20);
  display.write("  enter");
  display.display();
}

void showUnlocked() {
  display.clearDisplay();
  display.drawBitmap(
    display.width() - 1 - IMAGE_LOCK_WIDTH,
    (display.height() - IMAGE_LOCK_HEIGHT) / 2,
    IMAGE_LOCK_DATA, IMAGE_LOCK_WIDTH, IMAGE_LOCK_HEIGHT,
    SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  display.write("access granted");
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.write("  4264");
  display.display();
}

void showInvalid() {
  display.clearDisplay();
  display.drawBitmap(
    display.width() - 1 - IMAGE_INVALID_WIDTH,
    (display.height() - IMAGE_INVALID_HEIGHT) / 2,
    IMAGE_INVALID_DATA, IMAGE_INVALID_WIDTH, IMAGE_INVALID_HEIGHT,
    SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.write(" DENIED");
  display.display();
}

void setup() {
  // serial
  Serial.begin(9600);             // Initialize serial communications with the PC
  while (!Serial) { delay(1); };  // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  Serial.println(F("serial ready"));

  // rfid reader
  SPI.begin();  // Init SPI bus
  // mfrc522.PCD_Init(PIN_SS, PIN_RST);  // Init MFRC522 module
  Serial.print(F("antenna strength: "));
  Serial.println(mfrc522.PCD_GetAntennaGain());
  Serial.print(F("version: "));
  mfrc522.PCD_DumpVersionToSerial();  // Show version of PCD - MFRC522 Card Reader

  // lcd
  if (!display.begin(SSD1306_SWITCHCAPVCC, LCD_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // don't proceed, loop forever
  }
  display.cp437(true);
  showLocked();

  // done
  Serial.println(F("setup end"));
}


enum class State {
  Locked,
  Unlocked,
  Invalid,
};

constexpr unsigned long TIME_THRESHOLD_MS = 5000;
struct Status {
  State state = State::Locked;
  unsigned long last_time_ms = 0;
  String uid = "";
} status;

void loop() {
  switch (status.state) {
    case State::Locked:
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        status.uid = dumpUID(mfrc522.uid);
        Serial.println(status.uid);
        status.last_time_ms = millis();
        if (status.uid == UID_EYE) {
          Serial.println("valid rfid");
          status.state = State::Unlocked;
          showUnlocked();
        } else {
          Serial.println("invalid rfid");
          status.state = State::Invalid;
          showInvalid();
        }
      }
      break;
    case State::Unlocked:
      [[fallthrough]];
    case State::Invalid:
      if ((millis() - status.last_time_ms) > TIME_THRESHOLD_MS) {
        Serial.println("back to locked");
        status.state = State::Locked;
        showLocked();
      }
      break;
  }
  // mfrc522.PICC_HaltA();
  // mfrc522.PCD_StopCrypto1();
}

String dumpUID(const MFRC522::Uid& uid) {
  char buffer_string[10 * 2 + 1] = { 0 };
  char* write_start = buffer_string;
  char* buffer_end = buffer_string + sizeof(buffer_string);
  for (int i = 0; i < uid.size; i++) {
    // need to add 2 chars and keep space for the string terminator \0
    if (write_start + 2 + 1 < buffer_end) {
      write_start += sprintf(write_start, "%02x", uid.uidByte[i]);
    }
  }
  return String(buffer_string);
}