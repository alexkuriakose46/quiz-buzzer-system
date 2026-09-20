#include <Adafruit_NeoPixel.h>

/*
 * ============================================================
 *  Hardware LED Tester for the 3 Broken Buzzers
 * ============================================================
 *  Upload this to your SECOND ESP32.
 *  It doesn't use Wi-Fi or buttons. It just forces the LEDs
 *  to cycle Red -> Green -> Blue -> White.
 *  
 *  If the LEDs STILL don't light up with this code, 
 *  the strips are wired wrong (DOUT instead of DIN) or broken!
 */

const uint8_t LED_PINS[6] = { 5, 18, 19, 21, 22, 23 };
#define NUM_LEDS 10

Adafruit_NeoPixel leds[6] = {
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[4], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[5], NEO_GRB + NEO_KHZ800),
};

void setAllColors(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < NUM_LEDS; j++) {
      leds[i].setPixelColor(j, leds[i].Color(r, g, b));
    }
    leds[i].show();
  }
}

void setup() {
  for (int i = 0; i < 6; i++) {
    leds[i].begin();
    leds[i].setBrightness(50); // Safe brightness
    leds[i].show();
  }
}

void loop() {
  setAllColors(255, 0, 0);   // Red
  delay(1000);
  setAllColors(0, 255, 0);   // Green
  delay(1000);
  setAllColors(0, 0, 255);   // Blue
  delay(1000);
  setAllColors(100, 100, 100); // White
  delay(1000);
}
