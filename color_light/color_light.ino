#include <FastLED.h>
#include <WiFi.h>

const char WIFI_SSID[] = "LEDS";
const char WIFI_PASSWORD[] = "banyetes";
WiFiServer server(80);

#define PIN 33
//#define NUM_LEDS 22 // DINOSAUR
//#define NUM_LEDS 20 // DUCK
#define NUM_LEDS 16  // BANYETES
#define BRIGHTNESS 32
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 2

const char webpage[] = ("HTTP/1.1 200 OK\n"
                        "Content-type:text/html\n"
                        "Connection: close\n"
                        "\n"
                        "<!DOCTYPE html><html>\n"
                        "<head>\n"
                        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                        "  <link rel=\"icon\" href=\"data:,\">\n"
                        "  <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n"
                        "         .button { background-color: #4CAF50; border: 2px solid #4CAF50;; color: white; padding: 15px 32px; text-align: center;\n"
                        "                   text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; }\n"
                        "         text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}\n"
                        "  </style>\n"
                        "</head>\n"
                        "<body>"
                        "  <center><h1>ESP32 Web server LED controlling example</h1></center>"
                        "  <center><h2>Web Server Example</h2></center>"
                        "  <center><h2>Press on button to turn on led and off button to turn off LED</h3></center>"
                        "  <form>"
                        "    <center>"
                        "  <input type=\"range\" id=\"intensity\" name=\"intens\" min=\"0\" max=\"100\" value=\"90\" step=\"10\">"                        
                        "  <input type=\"submit\" value=\"Apply\">"                        
                        "      <p> LED one is ON</p>"
                        "      <button class=\"button\" name=\"LED0\" value=\"ON\" type=\"submit\">LED0 ON</button>"
                        "      <button class=\"button\" name=\"LED0\" value=\"OFF\" type=\"submit\">LED0 OFF</button><br><br>"
                        "      <p>LED two is OFF</p>"
                        "      <button class=\"button\" name=\"LED1\" value=\"ON\" type=\"submit\">LED1 ON</button>"
                        "      <button class=\"button\" name=\"LED1\" value=\"OFF\" type=\"submit\">LED1 OFF</button> <br><br>"
                        "      <p>LED three is ON</p>"
                        "      <button class=\"button\" name=\"LED2\" value=\"ON\" type=\"submit\">LED2 ON</button>"
                        "      <button class=\"button\" name=\"LED2\" value=\"OFF\" type=\"submit\">LED2 OFF</button></center>"
                        "    </center>"
                        "  </form>"
                        "</body>"
                        "</html>");



CRGBPalette16 currentPalette;
TBlendType currentBlending;

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;


void setup() {
  delay(3000);  // power-up safety delay
  FastLED.addLeds<LED_TYPE, PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  // debug
  Serial.begin(9600);

  // wifi server
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
}

void parse_user_input() {
  static String header;
  // client communicating
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client is requesting web page");
    String current_data_line = "";
    while (client.connected()) {
      if (client.available()) {
        // read what it says
        char new_byte = client.read();
        Serial.write(new_byte);
        header += new_byte;

        if (new_byte == '\n') {

          if (current_data_line.length() == 0) {
            // check header
            // Referer: http://192.168.4.1/?intens=100&LED0=ON

            // send webpage
            client.print(webpage);

            break;
          } else {
            current_data_line = "";
          }
        } else if (new_byte != '\r') {
          current_data_line += new_byte;
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


void loop() {
  parse_user_input();
  //int ldrStatus = analogRead(ldrPin);
  //Serial.println(ldrStatus);
  //delay(500);
  ChangePalettePeriodically();

  static uint8_t startIndex = 0;
  startIndex = startIndex + 1; /* motion speed */

  FillLEDsFromPaletteColors(startIndex);

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
  uint8_t brightness = 255;

  for (int i = 0; i < NUM_LEDS; ++i) {
    leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}

void ChangePalettePeriodically() {
  uint8_t secondHand = (millis() / 1000) % 60;
  static uint8_t lastSecond = 99;

  if (lastSecond != secondHand) {
    lastSecond = secondHand;
    if (secondHand == 0) {
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
    }
  }
}

const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM = {
  CRGB::Red,
  CRGB::Gray,  // 'white' is too bright compared to red and blue
  CRGB::Blue,
  CRGB::Black,

  CRGB::Red,
  CRGB::Gray,
  CRGB::Blue,
  CRGB::Black,

  CRGB::Red,
  CRGB::Red,
  CRGB::Gray,
  CRGB::Gray,
  CRGB::Blue,
  CRGB::Blue,
  CRGB::Black,
  CRGB::Black
};

void other_loop() {
  static String header;
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client is requesting web page");
    String current_data_line = "";
    while (client.connected()) {
      if (client.available()) {
        char new_byte = client.read();
        Serial.write(new_byte);
        header += new_byte;
        if (new_byte == '\n') {

          if (current_data_line.length() == 0) {

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // check header

            // send webpage

            break;
          } else {
            current_data_line = "";
          }
        } else if (new_byte != '\r') {
          current_data_line += new_byte;
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}