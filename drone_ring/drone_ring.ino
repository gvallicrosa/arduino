#include <FastLED.h>           // FastLED by Daniel Garcia
#include <Adafruit_VL53L0X.h>  // For VL53LXX-V2
#include <Wire.h>              // required by Adafruit_VL53L0X
#include <TFT.h>               // arduino lcd library
#include <SPI.h>               // required by lcd library

// configuration
constexpr bool DEBUG = false;
constexpr uint16_t MAX_RANGE_MM = 300;
constexpr unsigned long DELAY_MS = 1000;

// leds
#define PIN_LEDS 4
#define NUM_LEDS 8
#define BRIGHTNESS 20
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// tft
#define CS 10  // arduino nano v3
#define DC 9
#define RST 8
TFT tft(CS, DC, RST);

// range sensors
const uint8_t DefaultAddress = VL53L0X_I2C_ADDR;

struct Sensor {
  Adafruit_VL53L0X driver;
  VL53L0X_RangingMeasurementData_t measure;
  const uint8_t xshut_pin;

  Sensor(const uint8_t &xshut)
    : xshut_pin(xshut) {}
};

Sensor sensors[] = {
  { 2 },
  // { 3 },
};

constexpr size_t N = sizeof(sensors) / sizeof(sensors[0]);  // entries in sensors[]

void setID(const size_t &index, const uint8_t &address) {
  // debug
  Serial.print("set sensor index ");
  Serial.print(index);
  Serial.print(" to address ");
  Serial.println(address, HEX);

  // all reset
  for (size_t i = 0; i < N; ++i) {
    digitalWrite(sensors[i].xshut_pin, LOW);
  }
  delay(10);

  // all unreset
  for (size_t i = 0; i < N; ++i) {
    digitalWrite(sensors[i].xshut_pin, HIGH);
  }
  delay(10);

  // deactivate all that are not == index
  for (size_t i = 0; i < N; ++i) {
    if (i != index) {
      digitalWrite(sensors[i].xshut_pin, LOW);
    }
  }

  // change address of the only one that is active
  if (!sensors[index].driver.begin(address)) {
    Serial.print("Failed to boot VL53L0X with index ");
    Serial.println(index);
    while (true)
      ;
  }
  delay(10);

  // all unreset
  for (size_t i = 0; i < N; ++i) {
    digitalWrite(sensors[i].xshut_pin, HIGH);
  }
  delay(10);
}

char buffer[40];

void write_time(const unsigned long &duration_ms, const bool &top) {
  const unsigned long ms = duration_ms % 1000;
  const unsigned long s = duration_ms / 1000;
  const unsigned long sec = s % 60;
  const unsigned long m = s / 60;
  // position
  const int16_t y = top ? 30 : 93;
  // erase
  tft.noStroke();
  tft.fill(0, 0, 0);  // black
  tft.rect(0, y, 160, 30);
  // print
  if (top) {
    tft.stroke(255, 255, 255);  // white
  } else {
    tft.stroke(0, 100, 200);  // blue
  }
  tft.setTextSize(3);  // 30 pixels height
  sprintf(buffer, "%02lu:%02lu.%03lu", m, sec, ms);
  tft.text(buffer, 0, y);
}

void set_leds(const CRGB &color) {
  for (size_t i = 0; i < NUM_LEDS; ++i) {
    leds[i] = color;
  }
  FastLED.show();
}

void setup() {
  // wait until serial port opens for native USB devices
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // leds
  delay(3000);  // power-up safety delay
  FastLED.addLeds<LED_TYPE, PIN_LEDS, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // tft
  tft.begin();
  tft.background(0, 0, 0);
  tft.stroke(255, 255, 255);

  // number of sensors
  // Serial.print("configured for ");
  // Serial.print(N);
  // Serial.println(" range sensors");

  // configure xshuts
  // Serial.println("configure xshuts");
  for (size_t i = 0; i < N; ++i) {
    pinMode(sensors[i].xshut_pin, OUTPUT);
  }

  // set different addresses (keep initial at 0x29 and change the others)
  // Serial.println("set addresses");
  for (size_t i = 0; i < N; ++i) {
    setID(i, DefaultAddress + static_cast<uint8_t>(i));
  }

  // ready
  tft.setTextSize(2);        // 20 pixels height
  tft.stroke(230, 220, 45);  // yellow
  tft.text("current", 0, 5);
  tft.text("best", 0, 68);
  write_time(0, true);
  write_time(0, false);
  Serial.println("ready");
  set_leds(CRGB::Yellow);
}

struct Status {
  bool started = false;
  unsigned long start_time = 0;
  unsigned long best_time = 1000 * 60 * 60;
} status;

bool check_range(const bool &debug = false) {
  // measure
  for (size_t i = 0; i < N; ++i) {
    sensors[i].driver.rangingTest(&(sensors[i].measure), false);  // pass in 'true' to get debug data printout!
    if ((0 < sensors[i].measure.RangeMilliMeter) && (sensors[i].measure.RangeMilliMeter < MAX_RANGE_MM)) {
      return true;
    }
  }
  // for (size_t i = 0; i < N; ++i) {
  //   Serial.print("range");
  //   Serial.print(i);
  //   Serial.print(":");
  //   Serial.print(sensors[i].measure.RangeMilliMeter);
  //   Serial.print(",");
  // }
  // Serial.println();
  return false;
}

void loop() {
  // scan
  // scan_i2c();

  // normal loop
  if (!status.started) {
    // not started => start on detection
    if (check_range(DEBUG)) {
      // start and save stamp
      status.started = true;
      status.start_time = millis();
      // turn leds green
      set_leds(CRGB::Green);
      // wait to avoid double detections
      delay(DELAY_MS);
    }

  } else {
    // show time
    const unsigned long duration = millis() - status.start_time;
    write_time(duration, true);
    // started  => stop on detection
    if (check_range(DEBUG)) {
      // stop
      status.started = false;
      // show on screen if better than best
      if (duration < status.best_time) {
        for (size_t i = 0; i < 10; ++i) {
          set_leds(CRGB::BlueViolet);
          delay(250);
          set_leds(CRGB::White);
          delay(250);
        }
        status.best_time = duration;
        write_time(duration, false);
      } else {
        // turn leds red
        set_leds(CRGB::Red);
        // wait to avoid double detections
        delay(250 * 2 * 10);
      }
      // turn leds yellow for ready
      set_leds(CRGB::Yellow);
    }
  }
}

void scan_i2c() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found");
  else
    Serial.println("done");

  delay(5000);  // wait 5 seconds for next scan
}