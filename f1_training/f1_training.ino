#include <TFT.h>  // arduino lcd library
#include <SPI.h>  // required by lcd library

// pin definition for the mega 2560
#define cs 53
#define dc 48
#define rst 49
TFT TFTscreen = TFT(cs, dc, rst);
char score_buffer[4];
char max_score_buffer[4];

const int N = 5;
const int BUTTONS[] = { 9, 10, 11, 12, 13 };
const int LEDS[] = { 18, 17, 16, 15, 14 };
//const unsigned long GAME_TIME = 30000;  // 30 seconds
const unsigned long GAME_TIME = 10000;  // 10 seconds

char serial_buffer[50];

int current_led_index = 0;
bool game_started = false;
unsigned long time_started;
int count = 0;
int max_count = 0;

void write_current_score() {
  // erase
  TFTscreen.noStroke();
  TFTscreen.fill(0, 0, 0);
  TFTscreen.rect(0, 20, 160, 36);
  // print
  TFTscreen.stroke(255, 255, 255);
  TFTscreen.setTextSize(5);
  String(count).toCharArray(score_buffer, 4);
  TFTscreen.text(score_buffer, 0, 20);
}

void write_max_score() {
  // erase
  TFTscreen.noStroke();
  TFTscreen.fill(0, 0, 0);
  TFTscreen.rect(0, 80, 160, 36);
  // print
  TFTscreen.stroke(66, 135, 245);
  TFTscreen.setTextSize(5);
  String(max_count).toCharArray(max_score_buffer, 4);
  TFTscreen.text(max_score_buffer, 0, 80);
}

void setup() {
  // TODO: load max count
  // tft
  TFTscreen.begin();
  TFTscreen.background(0, 0, 0);
  TFTscreen.stroke(255, 255, 255);
  // title
  TFTscreen.setTextSize(2);
  TFTscreen.text("Score:\n ", 0, 0);
  // current score
  write_current_score();
  // max title
  TFTscreen.stroke(66, 135, 245);
  TFTscreen.setTextSize(2);
  TFTscreen.text("Maximum:\n ", 0, 60);
  // current max
  write_max_score();
  // setup leds and pins
  for (int i = 0; i < N; ++i) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
    pinMode(LEDS[i], OUTPUT);
  }
  // enable one led
  digitalWrite(LEDS[current_led_index], HIGH);
  // enable serial
  Serial.begin(9600);
}

bool change_led_on_button_press() {
  // read only current index
  const int value = digitalRead(BUTTONS[current_led_index]);

  // if pressed
  if (value == LOW) {
    // turn off
    digitalWrite(LEDS[current_led_index], LOW);
    // get new index different from current one
    int new_index = random(0, N);
    while (new_index == current_led_index) {
      new_index = random(0, N);
    }
    current_led_index = new_index;
    // turn on
    digitalWrite(LEDS[current_led_index], HIGH);
    // we had input
    return true;
  }
  // no input and no change
  return false;
}

void all_on_and_then_off() {
  for (int j = 0; j < 3; ++j) {
    // on
    for (int i = 0; i < N; ++i) {
      digitalWrite(LEDS[i], HIGH);
    }
    // wait
    delay(500);
    // off
    for (int i = 0; i < N; ++i) {
      digitalWrite(LEDS[i], LOW);
    }
    // wait
    delay(500);
  }
}

void loop() {
  if (game_started) {
    // check game end
    const unsigned long diff = millis() - time_started;
    if (diff >= GAME_TIME) {
      // show end
      sprintf(serial_buffer, "score %d\n", count);
      Serial.write(serial_buffer);
      all_on_and_then_off();
      // reset game
      game_started = false;
      current_led_index = 0;
      // wait a bit
      delay(3000);
      digitalWrite(LEDS[current_led_index], HIGH);
      // change scores
      if (count > max_count) {
        max_count = count;
        write_max_score();
      }
      count = 0;
      write_current_score();
    } else {
      // keep playing and counting
      if (change_led_on_button_press()) {
        ++count;
        write_current_score();
      }
    }
  } else {
    // read only current index
    if (change_led_on_button_press()) {
      Serial.write("start game\n");
      game_started = true;
      time_started = millis();  // turns around in 50 days :)
    }
  }
}
