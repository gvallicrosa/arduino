#include <TFT.h>      // arduino lcd library
#include <SPI.h>      // required by lcd library
#include <toneAC.h>   // music play library
#include "pitches.h"  // note frequencies
#include "melodies.h"

// pin definition for the mega 2560
#define cs 53
#define dc 48
#define rst 49
TFT tft = TFT(cs, dc, rst);
char score_buffer[4];

const int BUTTONS[] = { 19, 18, 17, 16, 15, 14, 31, 30, 29, 28, 27, 26, 32, 34, 36, 38, 40, 42 };
const int LEDS[] = { 5, 6, 7, 8, 9, 10, 25, 24, 23, 22, 21, 20, 33, 35, 37, 39, 41, 43 };
constexpr int M = sizeof(BUTTONS) / sizeof(int);
constexpr int N = sizeof(LEDS) / sizeof(int);
static_assert(M == N);

// const unsigned long GAME_TIME = 30000;  // 30 seconds
const unsigned long GAME_TIME = 15000;  // 15 seconds

// play a melody given an array of notes and its length
void play(Note const *values, const size_t &size) {
  for (size_t i = 0; i < size; ++i) {
    const auto &[note, duration] = *values;
    const unsigned long note_duration_ms = 1000 / duration;
    const unsigned long pause_ms = note_duration_ms * 1.30;
    toneAC(note, 20, note_duration_ms, false);
    delay(pause_ms);
    values++;
  }
}

// able to play a melody without stopping the execution process
struct BackgroundPlayer {
  // from constructor
  Note const *const original_pointer;  // to be able to rewind
  Note const *pointer;
  const size_t size;

  // computed
  unsigned long total_time_ms;
  unsigned long next_play_time;
  size_t index = 0;  // current playing note

  // state
  bool playing = false;
  bool wait_for_start = true;

  BackgroundPlayer(Note const *values, const size_t &size_)
    : original_pointer(values),
      pointer(values),
      size(size_) {
    // count total time
    total_time_ms = 0;
    for (size_t i = 0; i < size; ++i) {
      const auto &[note, duration] = *pointer;
      const unsigned long note_duration_ms = 1000 / duration;
      const unsigned long pause_ms = note_duration_ms * 1.30;
      total_time_ms += note_duration_ms + pause_ms;
      ++pointer;
    }
    // reset to first note
    reset();
  }

  void startPlayIfTimeOk(const unsigned long &current_time_ms, const unsigned long &end_time_ms) {
    if (wait_for_start) {
      if (current_time_ms >= end_time_ms - total_time_ms) {
        playing = true;
        wait_for_start = false;
        next_play_time = current_time_ms;
      }
    }
  }

  void play(const unsigned long &current_time_ms) {
    if (playing && (current_time_ms > next_play_time)) {
      if (index >= size) {
        playing = false;
      } else {
        const auto &[note, duration] = *pointer;
        const unsigned long note_duration_ms = 1000 / duration;
        const unsigned long pause_ms = note_duration_ms * 1.30;
        next_play_time += note_duration_ms + pause_ms;
        toneAC(note, 20, note_duration_ms, true);  // play in the background

        // next
        pointer++;
        index++;
      }
    }
  }

  void reset() {
    wait_for_start = true;
    playing = false;
    pointer = original_pointer;
    index = 0;
  }
} p_countdown(m_countdown, m_countdown_size);

void write_current_score(const int &value) {
  // erase
  tft.noStroke();
  tft.fill(0, 0, 0);
  tft.rect(0, 20, 160, 36);
  // print
  tft.stroke(255, 255, 255);
  tft.setTextSize(5);
  String(value).toCharArray(score_buffer, 4);
  tft.text(score_buffer, 0, 20);
}

void write_max_score(const int &value) {
  // erase
  tft.noStroke();
  tft.fill(0, 0, 0);
  tft.rect(0, 80, 160, 36);
  // print
  tft.stroke(66, 135, 245);
  tft.setTextSize(5);
  String(value).toCharArray(score_buffer, 4);
  tft.text(score_buffer, 0, 80);
}

struct GameState {
  int current_led_index = 0;
  bool started = false;
  unsigned long end_time_ms;
  int count = 0;
  int max_count = 0;

  void reset() {
    current_led_index = 0;
    started = false;
    count = 0;
  }
} game_state;

void test_leds_and_buttons() {
  for (size_t i = 0; i < N; ++i) {
    Serial.print("test index #");
    Serial.print(i);
    Serial.print(" with led at pin ");
    Serial.println(LEDS[i]);

    digitalWrite(LEDS[i], HIGH);
    delay(1000);
    digitalWrite(LEDS[i], LOW);
  }

  for (size_t i = 0; i < N; ++i) {
    Serial.print("test index #");
    Serial.print(i);
    Serial.print(" with led at pin ");
    Serial.print(LEDS[i]);
    Serial.print(" and button at pin ");
    Serial.println(BUTTONS[i]);

    digitalWrite(LEDS[i], HIGH);
    while (true) {
      const int value = digitalRead(BUTTONS[i]);
      if (value == LOW) { break; }
    }
    digitalWrite(LEDS[i], LOW);
  }
}

void setup() {
  // TODO: load max count
  // tft
  tft.begin();
  tft.background(0, 0, 0);
  tft.stroke(255, 255, 255);
  // title
  tft.setTextSize(2);
  tft.text("Score:\n ", 0, 0);
  // current score
  write_current_score(game_state.count);
  // max title
  tft.stroke(66, 135, 245);
  tft.setTextSize(2);
  tft.text("Maximum:\n ", 0, 60);
  // current max
  write_max_score(game_state.max_count);
  // setup leds and pins
  for (int i = 0; i < N; ++i) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
    pinMode(LEDS[i], OUTPUT);
  }

  // enable serial
  Serial.begin(9600);
  Serial.print("total time ms: ");
  Serial.println(p_countdown.total_time_ms);
  // test leds and buttons
  test_leds_and_buttons();

  // enable initial led
  digitalWrite(LEDS[game_state.current_led_index], HIGH);
}

bool change_led_on_button_press() {
  // read only current index
  const int value = digitalRead(BUTTONS[game_state.current_led_index]);
  // if pressed
  if (value == LOW) {
    // turn off
    digitalWrite(LEDS[game_state.current_led_index], LOW);
    // get new index different from current one
    int new_index = random(0, N);
    while (new_index == game_state.current_led_index) {
      new_index = random(0, N);
    }
    game_state.current_led_index = new_index;
    // turn on
    digitalWrite(LEDS[game_state.current_led_index], HIGH);
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
  // current time
  const unsigned long current_ms = millis();  // turns around in 50 days :)

  // game not started yet
  if (!game_state.started) {
    // read only current index (initial button)
    if (change_led_on_button_press()) {
      // start game on button press
      Serial.write("start game\n");
      game_state.started = true;
      game_state.end_time_ms = current_ms + GAME_TIME;
    }
  } else {
    // play music
    if (!p_countdown.playing) {
      p_countdown.startPlayIfTimeOk(current_ms, game_state.end_time_ms);
    } else {
      p_countdown.play(current_ms);
    }
    // still playing?
    if (current_ms < game_state.end_time_ms) {
      // keep playing and counting
      if (change_led_on_button_press()) {
        ++game_state.count;
        write_current_score(game_state.count);
      }
    } else {
      // show end with leds
      Serial.print("score ");
      Serial.println(game_state.count);
      all_on_and_then_off();
      // change scores
      if (game_state.count > game_state.max_count) {
        play(m_victory, m_victory_size);
        game_state.max_count = game_state.count;
        write_max_score(game_state.max_count);
      }
      // reset game
      delay(3000);  // wait a bit
      game_state.reset();
      p_countdown.reset();
      // set initial led on
      digitalWrite(LEDS[game_state.current_led_index], HIGH);
      write_current_score(game_state.count);
    }
  }
}
