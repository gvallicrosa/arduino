#include <FastLED.h>  // FastLED by Daniel Garcia
#include <NewPing.h>  // NewPing by Tim Eckel https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home
#include <TFT.h>      // arduino lcd library
#include <SPI.h>      // required by lcd library

// tft pin definition for the nano v3
#define CS 10
#define DC 9
#define RST 8

TFT tft = TFT(CS, DC, RST);

#define SONAR_MAX_DISTANCE_CM 29
#define SONAR_1_TRIGGER 2
#define SONAR_1_ECHO 3
#define SONAR_2_TRIGGER 4
#define SONAR_2_ECHO 5
#define DELAY_MS 1000

NewPing sonar1(SONAR_1_TRIGGER, SONAR_1_ECHO, SONAR_MAX_DISTANCE_CM);
NewPing sonar2(SONAR_2_TRIGGER, SONAR_2_ECHO, SONAR_MAX_DISTANCE_CM);

char buffer[40];

struct Status {
  bool started = false;
  unsigned long start_time = 0;
} status;

void setup() {
  Serial.begin(9600);
  // tft
  tft.begin();
  tft.background(0, 0, 0);
  tft.stroke(255, 255, 255);
}

void write_time(const unsigned long &duration_ms) {
  const unsigned long ms = duration_ms % 1000;
  const unsigned long s = duration_ms / 1000;
  const unsigned long sec = s % 60;
  const unsigned long m = s / 60;
  // erase
  tft.noStroke();
  tft.fill(0, 0, 0);  // black
  tft.rect(0, 20, 160, 30);
  // print
  tft.stroke(255, 255, 255);  // white
  tft.setTextSize(3);         // 30 pixels height
  sprintf(buffer, "%02lu:%02lu.%03lu", m, sec, ms);
  tft.text(buffer, 0, 20);
}

bool check_sonar(const bool &debug = false);

void loop() {
  // debug loop
  // while (true) {
  //   check_sonar(true);
  // }

  // normal loop
  if (!status.started) {
    // not started => start on detection
    if (check_sonar()) {
      // start and save stamp
      status.started = true;
      status.start_time = millis();
      // turn leds green

      // wait to avoid double detections
      delay(DELAY_MS);
    }

  } else {
    // show time
    write_time(millis() - status.start_time);
    // started  => stop on detection
    if (check_sonar()) {
      // stop
      status.started = false;
      const unsigned long period_ms = millis() - status.start_time;
      // turn leds green

      // show on screen

      // wait to avoid double detections
      delay(DELAY_MS);
      // turn leds yellow for ready
    }
  }
}

bool check_sonar(const bool &debug) {
  delay(35);  // Milliseconds between sensor pings (29ms is about the min to avoid cross-sensor echo).
  const unsigned long cm1 = sonar1.ping_cm();
  delay(35);  // Milliseconds between sensor pings (29ms is about the min to avoid cross-sensor echo).
  const unsigned long cm2 = sonar2.ping_cm();
  if (debug) {
    Serial.print("Threshold:");
    Serial.print(SONAR_MAX_DISTANCE_CM);
    Serial.print(",Sonar1:");
    Serial.print(cm1);
    Serial.print(",Sonar2:");
    Serial.println(cm2);
  }
  if ((cm1 > 0) || (cm2 > 0)) {
    return true;
  }
  return false;
}
