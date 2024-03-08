#pragma once
#include <TMRpcm.h>

int endsWith(const char *str, const char *suffix) {
  if (!str || !suffix) {
    return 0;
  }
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr) {
    return 0;
  }
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

constexpr size_t PHONE_LEN = 3;
constexpr int PHONE_NUMBER_MIN_TIME_MS = 300;

class Caller {
  // unsigned long m_time = 0;
  // static constexpr size_t FILES_MAX = 40;
  // String files[FILES_MAX];
  // size_t files_read = 0;

  char m_phone[PHONE_LEN + 1] = { 0 };
  size_t m_phone_index = 0;
  bool m_went_through_invalid = false;


public:
  void reset() {
    // m_time = millis();
    m_phone_index = 0;
    m_went_through_invalid = false;
  }

  // void gatherSoundFiles(File dir) {
  //   Serial.println("gathered sound files");
  //   while (files_read < FILES_MAX) {
  //     SDLib::File entry = dir.openNextFile();
  //     if (!entry) {
  //       // no more files
  //       break;
  //     }
  //     Serial.println(entry.name());
  //     // only audio files on the root
  //     if (entry.isDirectory()) {
  //       gatherSoundFiles(entry);
  //     } else {
  //       if (endsWith(entry.name(), ".WAV")) {
  //         Serial.print("- ");
  //         Serial.println(entry.name());
  //         files[files_read] = String(entry.name());
  //         files_read++;
  //       }
  //     }
  //     entry.close();
  //   }
  // }

  bool addNumber(const size_t button) {
    if (button == bm::BUTTON_INVALID) {
      if (!m_went_through_invalid) {
        Serial.println(F("no key"));
        m_went_through_invalid = true;
      }
      return false;  // nothing else to do
    }
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
    char buffer[16] = { 0 };
    char *writter = buffer;
    for (size_t i = 0; i < PHONE_LEN; ++i) {
      writter += sprintf(writter, "%d", m_phone[i]);
    }
    writter += sprintf(writter, ".WAV");
    Serial.print(F("file to play: "));
    Serial.println(buffer);
    // const String number(buffer);
    // for (size_t i = 0; i < files_read; ++i) {
    //   if (number == files[i]) {
    //     player.play(number.c_str());
    //     return;
    //   }
    // }
    Serial.println(F("file does not exist"));
    player.play("NONE.WAV");  // not found
  }
};