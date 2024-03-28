#include <Adafruit_GFX.h>      // lcd
#include <Adafruit_SSD1306.h>  // lcd
#include <MFRC522.h>           // rfid
#include <SPI.h>               // rfid
#include <Wire.h>              // lcd

// lcd
constexpr uint8_t LCD_ADDRESS = 0x3C;
constexpr uint8_t LCD_WIDTH = 128;
constexpr uint8_t LCD_HEIGHT = 64;
constexpr uint8_t LCD_RESET = -1;
Adafruit_SSD1306 display(LCD_WIDTH, LCD_HEIGHT, &Wire, LCD_RESET);

// rfid
constexpr uint8_t PIN_RST = 9;
constexpr uint8_t PIN_SS = 10;
const String UID_100("1340a01a");
const String UID_200("d3dcb71a");
const String UID_400("03a2bb1a");
const String UID_800("13ef9b1a");
MFRC522 mfrc522(PIN_SS, PIN_RST);


void setup()
{
  // serial
  Serial.begin(9600);  // Initialize serial communications with the PC
  while (!Serial)
  {
    delay(1);
  };  // Do nothing if no serial port is opened (added for Arduinos based on
      // ATMEGA32U4)
  Serial.println(F("serial ready"));

  // rfid reader
  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 module
  delay(4);
  Serial.print(F("version: "));
  mfrc522.PCD_DumpVersionToSerial();  // Show version of PCD - MFRC522 Card Reader

  // lcd
  if (!display.begin(SSD1306_SWITCHCAPVCC, LCD_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // don't proceed, loop forever
  }
  display.cp437(true);
  showInit();

  // done
  Serial.println(F("setup end"));
}

constexpr size_t bar_height = 10;

void drawBar(const size_t &bar_width)
{
  // full rectangle
  display.drawRect(0, 0, display.width(), bar_height, SSD1306_WHITE);
  // inside rectangle is 2 pixels less
  display.drawRect(2, 2, display.width() - 4, bar_height - 4, SSD1306_WHITE);
  // draw completed bar
  display.fillRect(2, 2, bar_width, bar_height - 4, SSD1306_WHITE);
}

void showInit()
{
  display.clearDisplay();
  display.setTextSize(4);                 // Normal 1:1 pixel scale
  display.setCursor(46, 3 * bar_height);  // Start at top-left corner
  display.setTextColor(SSD1306_WHITE);    // Draw white text
  display.write("0$");
  drawBar(0);
  display.display();
}

void showEnd()
{
  display.clearDisplay();
  display.setTextSize(2);                 // Normal 1:1 pixel scale
  display.setCursor(10, 2 * bar_height);  // Start at top-left corner
  display.setTextColor(SSD1306_WHITE);    // Draw white text
  display.write("1   5   9");
  display.setCursor(10, 4 * bar_height);  // Start at top-left corner
  display.write("  2   6");
  drawBar(124);
  display.display();
}

void showProgress(const size_t &old_value, const size_t &new_value, const char *amount)
{
  display.clearDisplay();
  display.setTextSize(4);                // Normal 1:1 pixel scale
  display.setCursor(2, 3 * bar_height);  // Start at top-left corner
  display.setTextColor(SSD1306_WHITE);   // Draw white text
  display.write(amount);
  for (size_t i = old_value; i < new_value; ++i)
  {
    drawBar(i);
    display.display();
    delay(1);
  }
  // after animation
  display.clearDisplay();
  drawBar(new_value);
  display.display();
}

enum class State
{
  Bet,
  Complete
};

struct Status
{
  // value goes from 0 to 100 + 200 + 400 + 800 = 1500 which is the same as 128 - 4 = 124
  bool chip_100 = false;  // 8
  bool chip_200 = false;  // 17
  bool chip_400 = false;  // 33
  bool chip_800 = false;  // 66
  size_t value = 0;
  String uid = "";
  State state = State::Bet;
} status;

void loop()
{
  switch (status.state)
  {
    case State::Bet:
    {
      // new card?
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
      {
        status.uid = dumpUID(mfrc522.uid);
        mfrc522.PICC_HaltA();  // end read
        Serial.println(status.uid);

        // which card?
        if (status.uid == UID_100)
        {
          if (status.chip_100)
          {
            Serial.println("100$ again");
          }
          else
          {
            Serial.println("100$");
            showProgress(status.value, status.value + 8, "+100$");
            status.value += 8;
            status.chip_100 = true;
          }
        }
        else if (status.uid == UID_200)
        {
          if (status.chip_200)
          {
            Serial.println("200$ again");
          }
          else
          {
            Serial.println("200$");
            showProgress(status.value, status.value + 17, "+200$");
            status.value += 17;
            status.chip_200 = true;
          }
        }
        else if (status.uid == UID_400)
        {
          if (status.chip_400)
          {
            Serial.println("400$ again");
          }
          else
          {
            Serial.println("400$");
            showProgress(status.value, status.value + 33, "+400$");
            status.value += 33;
            status.chip_400 = true;
          }
        }
        else if (status.uid == UID_800)
        {
          if (status.chip_800)
          {
            Serial.println("800$ again");
          }
          else
          {
            Serial.println("800$");
            showProgress(status.value, status.value + 66, "+800$");
            status.value += 66;
            status.chip_800 = true;
          }
        }
        else
        {
          Serial.println("invalid rfid");
        }

        // complete?
        if (status.chip_100 && status.chip_200 && status.chip_400 && status.chip_800)
        {
          Serial.println("State::Complete");
          status.state = State::Complete;
          // final draw
          showEnd();
        }
      }
      break;
    }
    case State::Complete:
    {
      break;
    }
  }
}

String dumpUID(const MFRC522::Uid &uid)
{
  char buffer_string[10 * 2 + 1] = { 0 };
  char *write_start = buffer_string;
  char *buffer_end = buffer_string + sizeof(buffer_string);
  for (int i = 0; i < uid.size; i++)
  {
    // need to add 2 chars and keep space for the string terminator \0
    if (write_start + 2 + 1 < buffer_end)
    {
      write_start += sprintf(write_start, "%02x", uid.uidByte[i]);
    }
  }
  return String(buffer_string);
}