#include "DisplayDriver.h"
#include <TFT_eSPI.h>

DisplayDriver displayDriver;

// Native portrait panel dimensions — hardcoded to avoid TFT_eSPI macro collision
static TFT_eSPI tft = TFT_eSPI(135, 240);

void DisplayDriver::begin() {
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(COL_BG);
  ledcAttach(PIN_TFT_BL, 5000, 8);
  setBacklight(100);
}

void DisplayDriver::setBacklight(uint8_t pct) {
  uint8_t duty = (uint8_t)((uint16_t)pct * 255 / 100);
  if (pct > 100) duty = 255;
  ledcWrite(PIN_TFT_BL, duty);
}

void DisplayDriver::fillScreen(uint16_t color) {
  tft.fillScreen(color);
}

void DisplayDriver::setTextColor(uint16_t fg, uint16_t bg) {
  if (bg == 0xFFFF)
    tft.setTextColor(fg);
  else
    tft.setTextColor(fg, bg);
}

void DisplayDriver::setTextSize(uint8_t s) {
  tft.setTextSize(s);
}

void DisplayDriver::setCursor(int16_t x, int16_t y) {
  tft.setCursor(x, y);
}

void DisplayDriver::print(const char* s) {
  tft.print(s);
}

void DisplayDriver::print(int n) {
  tft.print(n);
}

void DisplayDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
}

void DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  tft.fillRect(x, y, w, h, color);
}

void DisplayDriver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
}

uint16_t DisplayDriver::color565(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}
