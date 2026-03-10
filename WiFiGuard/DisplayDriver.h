#ifndef DISPLAYDRIVER_H
#define DISPLAYDRIVER_H

#include "Config.h"
#include <Arduino.h>

// Wrapper for ST7789 display (TFT_eSPI or Adafruit). Default: TFT_eSPI.
class DisplayDriver {
public:
  void begin();
  void setBacklight(uint8_t pct);
  void fillScreen(uint16_t color);
  void setTextColor(uint16_t fg, uint16_t bg = 0xFFFF);
  void setTextSize(uint8_t s);
  void setCursor(int16_t x, int16_t y);
  void print(const char* s);
  void print(int n);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  int16_t width() const { return SCREEN_W; }
  int16_t height() const { return SCREEN_H; }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
};

extern DisplayDriver displayDriver;

// Semantic color palette
#define COL_BG       0x0000  // pure black
#define COL_FG       0xFFFF  // white — primary text
#define COL_DIM      0x7BEF  // light gray — secondary text
#define COL_SAFE     0x07E0  // green — LOW risk, secure auth, STABLE
#define COL_WARN     0xFD20  // orange — MED risk, legacy auth, MODERATE
#define COL_DANGER   0xF800  // red — HIGH risk, open auth, evil twin
#define COL_INFO     0x04FF  // cyan — badges, channel suggestions
#define COL_SEL      0x18E3  // dark blue-gray — selected item bg
#define COL_CHROME   0x1082  // very dark gray — header/footer bg

// Legacy aliases
#define COLOR_BG     COL_BG
#define COLOR_FG     COL_FG
#define COLOR_ACCENT COL_SAFE
#define COLOR_WARN   COL_WARN
#define COLOR_RISK   COL_DANGER
#define COLOR_DIM    COL_DIM

#endif
