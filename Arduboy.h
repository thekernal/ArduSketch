#ifndef Arduboy_h
#define Arduboy_h

#include <SPI.h>
#include <Print.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <limits.h>

#define DEVKIT

// EEPROM settings

#define EEPROM_VERSION 0
#define EEPROM_BRIGHTNESS 1
#define EEPROM_AUDIO_ON_OFF 2
// we reserve the first 16 byte of EEPROM for system use
#define EEPROM_STORAGE_SPACE_START 16 // and onward

// eeprom settings above are neded for audio
#include "audio.h"

#define PIXEL_SAFE_MODE
#define SAFE_MODE

#define CS 6
#define DC 4
#define RST 12

// compare Vcc to 1.1 bandgap
#define ADC_VOLTAGE _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1)
// compare temperature to 2.5 internal reference
// also _BV(MUX5)
#define ADC_TEMP _BV(REFS0) | _BV(REFS1) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0)

#define LEFT_BUTTON _BV(5)
#define RIGHT_BUTTON _BV(2)
#define UP_BUTTON _BV(4)
#define DOWN_BUTTON _BV(6)
#define A_BUTTON _BV(1)
#define B_BUTTON _BV(0)

#define PIN_LEFT_BUTTON 9
#define PIN_RIGHT_BUTTON 5
#define PIN_UP_BUTTON 8
#define PIN_DOWN_BUTTON 10
#define PIN_A_BUTTON A0
#define PIN_B_BUTTON A1

#define PIN_SPEAKER_1 A2
#define PIN_SPEAKER_2 A3

#define WIDTH 128
#define HEIGHT 64

#define WHITE 1
#define BLACK 0

#define COLUMN_ADDRESS_END (WIDTH - 1) & 0x7F
#define PAGE_ADDRESS_END ((HEIGHT/8)-1) & 0x07


class Arduboy : public Print
{
public:
  Arduboy();
  void LCDDataMode();
  void LCDCommandMode();

  uint8_t getInput();
  boolean pressed(uint8_t buttons);
  boolean not_pressed(uint8_t buttons);
  void start();
  void saveMuchPower();
  void idle();
  void blank();
  void clearDisplay();
  void display();
  void prepZoomSwitch(uint8_t zoom);
  void scrollScreen(uint8_t xcur, uint8_t ycur, uint8_t width, uint8_t height);
  void drawScreen1X(uint8_t xcur, uint8_t ycur); 
  void drawScreen2X(uint8_t xcur, uint8_t ycur); 
  void drawScreen4X(uint8_t xcur, uint8_t ycur); 
  void drawScreen(const unsigned char *image);
  void drawScreen(unsigned char image[]);
  void drawPixel(int x, int y, uint8_t color);
  void flipPixel(int x, int y);
  uint8_t getPixel(uint8_t x, uint8_t y);
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
  void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint8_t color);
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
  void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint8_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
  void fillScreen(uint8_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
  void fillTriangle (int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
  void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color);
  void drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color);
  void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size);
  void setCursor(int16_t x, int16_t y);
  void setTextSize(uint8_t s);
  void setTextWrap(boolean w);
  void writeCode(uint8_t width, uint8_t height);
  void writeHex(uint8_t width, uint8_t height);
  void writeSVG(uint8_t width, uint8_t height);
  void svgWrite();
  void svgByte(uint8_t width, uint8_t height, uint8_t pixel);
  void svgPixel(uint8_t width, uint8_t height);
  void print2Hex(uint8_t ch);
  inline unsigned char* getBuffer();
  uint8_t width();
  uint8_t height();
  virtual size_t write(uint8_t);
  void initRandomSeed();
  void swap(int16_t& a, int16_t& b);

  ArduboyTunes tunes;
  ArduboyAudio audio;

  void setFrameRate(uint8_t rate);
  bool nextFrame();
  int cpuLoad();
  uint8_t frameRate = 60;
  uint8_t frameCount = 0;
  uint8_t eachFrameMillis = 1000/60;
  long lastFrameStart = 0;
  long nextFrameStart = 0;
  bool post_render = false;
  uint8_t lastFrameDurationMs = 0;

private:
  unsigned char sBuffer[(HEIGHT*WIDTH)/8];

  void bootLCD() __attribute__((always_inline));
  void safeMode() __attribute__((always_inline));
  void slowCPU() __attribute__((always_inline));
  uint8_t readCapacitivePin(int pinToMeasure);
  uint8_t readCapXtal(int pinToMeasure);
  uint16_t rawADC(byte adc_bits);
  volatile uint8_t *mosiport, *clkport, *csport, *dcport;
  uint8_t mosipinmask, clkpinmask, cspinmask, dcpinmask;
  uint8_t x_start, y_start;
// Adafruit stuff
protected:
  int16_t cursor_x = 0;
  int16_t cursor_y = 0;
  uint8_t textsize = 1;
  boolean wrap; // If set, 'wrap' text at right edge of display
};

#endif
