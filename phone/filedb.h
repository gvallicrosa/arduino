#pragma once

static constexpr size_t FILES_MAX = 10;

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

class FileDB {
  String files[FILES_MAX];
  size_t files_read = 0;

  void gatherSoundFiles(File &dir) {
    Serial.println("gathered sound files");
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
          Serial.print("- ");
          Serial.println(entry.name());
          files[files_read] = String(entry.name());
          files_read++;
        }
      }
      entry.close();
    }
  }

  void printDirectory(SDLib::File &dir, int numTabs) {
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
  }

public:
  void gather() {
    SDLib::File root = SD.open("/");
    gatherSoundFiles(root);
    root.close();
  }

  bool hasFile(const String &filename) const {
    for (size_t i = 0; i < files_read; ++i) {
      if (filename == files[i]) {
        return true;
      }
    }
    return false;
  }

  void listAllFilesRecursive() {
    SDLib::File root = SD.open("/");
    printDirectory(root, 0);
    root.close();
  }
};