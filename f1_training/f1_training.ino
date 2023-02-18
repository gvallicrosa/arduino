const int N = 5;
const int BUTTONS[] = { 2, 3, 4, 5, 6 };
const int LEDS[] = { 8, 9, 10, 11, 12 };
//const unsigned long GAME_TIME = 30000;  // 30 seconds
const unsigned long GAME_TIME = 10000;  // 10 seconds

char write_buffer[50];

int current_led_index = 0;
bool game_started = false;
unsigned long time_started;
int count = 0;

void setup() {
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
      sprintf(write_buffer, "score %d\n", count);
      Serial.write(write_buffer);
      all_on_and_then_off();
      // reset game
      game_started = false;
      current_led_index = 0;
      // wait a bit
      delay(3000);
      digitalWrite(LEDS[current_led_index], HIGH);
    } else {
      // keep playing and counting
      if (change_led_on_button_press()) {
        ++count;
      }
    }
  } else {
    // read only current index
    if (change_led_on_button_press()) {
      Serial.write("start game\n");
      game_started = true;
      time_started = millis();  // turns around in 50 days :)
      count = 1;
    }
  }
}
