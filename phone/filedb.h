#pragma once

static constexpr size_t FILES_MAX = 15;

int endsWith(const char *str, const char *suffix) {
  if (!str || !suffix) {
    return 0;
  }
  const size_t lenstr = strlen(str);
  const size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr) {
    return 0;
  }
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

class FileDB {
  String files[FILES_MAX];
  size_t files_read = 0;

  void gatherSoundFiles(File &dir) {
    Serial.println(F("gathered sound files"));
    dir.rewindDirectory();
    while (files_read < FILES_MAX) {
      SDLib::File entry = dir.openNextFile();
      if (!entry) {
        // no more files
        break;
      }
      // only audio files on the root
      if (entry.isDirectory()) {
        // all files should be in root
      } else {
        if (endsWith(entry.name(), ".WAV")) {
          files[files_read] = String(entry.name());
          files_read++;
        }
      }
      entry.close();
    }
  }

  /*void printDirectory(SDLib::File &dir, int numTabs) {
    dir.rewindDirectory();
    while (true) {
      SDLib::File entry = dir.openNextFile();
      if (!entry) {
        // no more files
        break;
      }

      for (uint8_t i = 0; i < numTabs; i++) {
        Serial.print("  ");
      }
      Serial.print(entry.name());

      if (entry.isDirectory()) {
        Serial.println("/");
        printDirectory(entry, numTabs + 1);
      } else {
        // files have sizes, directories do not
        Serial.print("    ");
        Serial.println(entry.size(), DEC);
      }
      entry.close();
    }
  }*/

public:
  void gather() {
    SDLib::File root = SD.open(F("/"));
    gatherSoundFiles(root);
    root.close();
  }

  bool hasFile(const String &filename) const {
    for (size_t i = 0; i < files_read; ++i) {
      // Serial.print(filename);
      // Serial.print(" = ");
      // Serial.print(files[i]);
      // Serial.print(" ?");
      if (filename == files[i]) {
      // Serial.println(" yes");
        return true;
      }
      // Serial.println(" no");
    }
    return false;
  }

  /*void listAllFilesRecursive() {
    SDLib::File root = SD.open("/");
    printDirectory(root, 0);
    root.close();
  }*/

  void showFiles() const {
    Serial.print(F("files: "));
    Serial.println(files_read);
    for (size_t i = 0; i < files_read; ++i) {
      Serial.print(i);
      Serial.print(" ");
      Serial.println(files[i]);
    }
  }
};