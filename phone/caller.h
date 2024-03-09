#pragma once
#include <TMRpcm.h>
#include "./filedb.h"

constexpr size_t PHONE_LEN = 3;

class Caller {
  char m_phone[PHONE_LEN + 1] = { 0 };
  size_t m_phone_index = 0;
  bool m_went_through_invalid = false;
  FileDB m_filedb;

public:
  void init() {
    // gather WAV files from SD
    m_filedb.gather();
  }

  void reset() {
    // called after a number has been dialled
    m_phone_index = 0;
    m_went_through_invalid = false;
  }

  bool addNumber(const size_t button) {
    // track that it goes to invalid after each button press
    if (button == bm::BUTTON_INVALID) {
      if (!m_went_through_invalid) {
        Serial.println(F("no key"));
        m_went_through_invalid = true;
      }
      return false;  // nothing else to do
    }
    // accept a new number after going to invalid
    if (m_went_through_invalid) {
      if (button <= 9) {
        m_went_through_invalid = false;
        m_phone[m_phone_index] = button;
        m_phone_index++;
        Serial.print("add number: ");
        Serial.println(button);
        if (m_phone_index == PHONE_LEN) {
          Serial.println("complete phone");
          return true;
        }
      } else {
        Serial.print("unused key: ");
        Serial.println(button);
      }
    }
    return false;
  }

  void call(TMRpcm &player) const {
    // construct filename
    char buffer[16] = { 0 };
    char *buffer_writter = buffer;
    for (size_t i = 0; i < PHONE_LEN; ++i) {
      buffer_writter += sprintf(buffer_writter, "%d", m_phone[i]);
    }
    buffer_writter += sprintf(buffer_writter, ".WAV");
    Serial.print(F("file to play: "));
    Serial.println(buffer);
    // check filedb
    const String number(buffer);
    if (m_filedb.hasFile(number)) {
      player.play(buffer);
      return;
    }
    // not found
    Serial.println(F("file does not exist"));
    player.play("NONE.WAV");  // not found
  }
};