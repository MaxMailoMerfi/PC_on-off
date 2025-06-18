#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// ---------------- Константи ----------------
constexpr uint8_t  LED_PIN       = 2;
constexpr uint8_t  MATRIX_WIDTH  = 15;
constexpr uint8_t  MATRIX_HEIGHT = 11;
constexpr uint16_t NUM_LEDS      = MATRIX_WIDTH * MATRIX_HEIGHT;
constexpr uint8_t  BRIGHTNESS    = 40;
constexpr int32_t  TIME_OFFSET   = 3 * 3600;              // +3 GMT
constexpr uint32_t NTP_REFRESH   = 60 * 1000UL;

// ---------------- Кольори ----------------
constexpr CRGB COLOR_HOURS   = CRGB::Red;
constexpr CRGB COLOR_MINUTES = CRGB::Blue;
constexpr CRGB COLOR_COLON   = CRGB::Yellow;

// ---------------- Глобальні змінні ----------------
CRGB leds[NUM_LEDS];
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", TIME_OFFSET, NTP_REFRESH);

// ---------------- Шрифт 3x5 ----------------
const uint8_t digitFont[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, {0b010, 0b110, 0b010, 0b010, 0b111},
  {0b111, 0b001, 0b111, 0b100, 0b111}, {0b111, 0b001, 0b010, 0b001, 0b111},
  {0b101, 0b101, 0b111, 0b001, 0b001}, {0b111, 0b100, 0b111, 0b001, 0b111},
  {0b111, 0b100, 0b111, 0b101, 0b111}, {0b111, 0b001, 0b010, 0b010, 0b010},
  {0b111, 0b101, 0b111, 0b101, 0b111}, {0b111, 0b101, 0b111, 0b001, 0b111}
};

// ---------------- Утиліти ----------------
constexpr inline uint16_t XY(uint8_t x, uint8_t y) {
  return y * MATRIX_WIDTH + x;
}

void clearMatrix() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

// ---------------- Малювання ----------------
void drawDigit(uint8_t digit, uint8_t xOffset, uint8_t yOffset, CRGB color) {
  if (digit > 9) return;

  for (uint8_t y = 0; y < 5; ++y) {
    uint8_t row = digitFont[digit][y];
    for (uint8_t x = 0; x < 3; ++x) {
      if (row & (1 << (2 - x))) {
        uint8_t xPos = x + xOffset;
        uint8_t yPos = y + yOffset;
        if (xPos < MATRIX_WIDTH && yPos < MATRIX_HEIGHT) {
          leds[XY(xPos, yPos)] = color;
        }
      }
    }
  }
}

void drawColon(uint8_t x, uint8_t y, CRGB color) {
  if (y + 1 < MATRIX_HEIGHT) leds[XY(x, y + 1)] = color;
  if (y + 3 < MATRIX_HEIGHT) leds[XY(x, y + 3)] = color;
}

void drawTime(uint8_t hours, uint8_t minutes, bool showColon) {
  clearMatrix();
  drawDigit(hours   / 10,  0, 3, COLOR_HOURS);
  drawDigit(hours   % 10,  4, 3, COLOR_HOURS);
  if (showColon) drawColon(7, 3, COLOR_COLON);
  drawDigit(minutes / 10,  8, 3, COLOR_MINUTES);
  drawDigit(minutes % 10, 12, 3, COLOR_MINUTES);
  FastLED.show();
}

// ---------------- Годинник ----------------
void clockInit() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  clearMatrix();
  FastLED.show();
  timeClient.begin();
  timeClient.update();
}

void clockTick() {
  static unsigned long lastBlink    = 0;
  static unsigned long lastNtpSync  = 0;
  static bool showColon             = true;
  unsigned long currentMillis       = millis();

  if (WiFi.status() == WL_CONNECTED && currentMillis - lastNtpSync >= NTP_REFRESH) {
    timeClient.update();
    lastNtpSync = currentMillis;
  }

  if (currentMillis - lastBlink >= 500) {
    showColon = !showColon;
    lastBlink = currentMillis;
  }

  drawTime(timeClient.getHours(), timeClient.getMinutes(), showColon);
}
