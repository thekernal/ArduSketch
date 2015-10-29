#include "Arduboy.h"
#include "glcdfont.c"

Arduboy::Arduboy() { }

void Arduboy::start()
{
  #if F_CPU == 8000000L
  slowCPU();
  #endif

  SPI.begin();
  pinMode(DC, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(PIN_LEFT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_UP_BUTTON, INPUT_PULLUP);
  pinMode(PIN_DOWN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_A_BUTTON, INPUT_PULLUP);
  pinMode(PIN_B_BUTTON, INPUT_PULLUP);
  tunes.initChannel(PIN_SPEAKER_1);
  tunes.initChannel(PIN_SPEAKER_2);


  csport = portOutputRegister(digitalPinToPort(CS));
  cspinmask = digitalPinToBitMask(CS);
  dcport = portOutputRegister(digitalPinToPort(DC));
  dcpinmask = digitalPinToBitMask(DC);

  /**
   * Setup reset pin direction (used by both SPI and I2C)
   */
  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);
  delay(1);           // VDD (3.3V) goes high at start, lets just chill for a ms
  digitalWrite(RST, LOW);   // bring reset low
  delay(10);          // wait 10ms
  digitalWrite(RST, HIGH);  // bring out of reset

  bootLCD();

  #ifdef SAFE_MODE
  if (pressed(LEFT_BUTTON+UP_BUTTON))
    safeMode();
  #endif

  audio.setup();
  saveMuchPower();
}

#if F_CPU == 8000000L
// if we're compiling for 8Mhz we need to slow the CPU down because the
// hardware clock on the Arduboy is 16MHz
void Arduboy::slowCPU()
{
  uint8_t oldSREG = SREG;
  cli();                // suspend interrupts
  CLKPR = _BV(CLKPCE);  // allow reprogramming clock
  CLKPR = 1;            // set clock divisor to 2 (0b0001)
  SREG = oldSREG;       // restore interrupts
}
#endif

void Arduboy::bootLCD()
{
  LCDCommandMode();
  SPI.transfer(0xAE);  // Display Off
  SPI.transfer(0XD5);  // Set Display Clock Divisor v
  SPI.transfer(0xF0);  //   0x80 is default
  SPI.transfer(0xA8);  // Set Multiplex Ratio v
  SPI.transfer(0x3F);
  SPI.transfer(0xD3);  // Set Display Offset v
  SPI.transfer(0x0);
  SPI.transfer(0x40);  // Set Start Line (0)
  SPI.transfer(0x8D);  // Charge Pump Setting v
  SPI.transfer(0x14);  //   Enable
  // why are we running this next pair twice?
  SPI.transfer(0x20);  // Set Memory Mode v
  SPI.transfer(0x00);  //   Horizontal Addressing
  SPI.transfer(0xA1);  // Set Segment Re-map (A0) | (b0001)
  SPI.transfer(0xC8);  // Set COM Output Scan Direction
  SPI.transfer(0xDA);  // Set COM Pins v
  SPI.transfer(0x12);
  SPI.transfer(0x81);  // Set Contrast v
  SPI.transfer(0xCF);
  SPI.transfer(0xD9);  // Set Precharge
  SPI.transfer(0xF1);
  SPI.transfer(0xDB);  // Set VCom Detect
  SPI.transfer(0x40);
  SPI.transfer(0xA4);  // Entire Display ON
  SPI.transfer(0xA6);  // Set normal/inverse display
  SPI.transfer(0xAF);  // Display On

  LCDCommandMode();
  SPI.transfer(0x20);     // set display mode
  SPI.transfer(0x00);     // horizontal addressing mode

  SPI.transfer(0x21);     // set col address
  SPI.transfer(0x00);
  SPI.transfer(COLUMN_ADDRESS_END);

  SPI.transfer(0x22); // set page address
  SPI.transfer(0x00);
  SPI.transfer(PAGE_ADDRESS_END);

  LCDDataMode();
}

// Safe Mode is engaged by holding down both the LEFT button and UP button
// when plugging the device into USB.  It puts your device into a tight
// loop and allows it to be reprogrammed even if you have uploaded a very
// broken sketch that interferes with the normal USB triggered auto-reboot
// functionality of the device.
void Arduboy::safeMode()
{
  display(); // too avoid random gibberish
  while (true) {
    asm volatile("nop \n");
  }
}

void Arduboy::LCDDataMode()
{
  *dcport |= dcpinmask;
  *csport &= ~cspinmask;
}

void Arduboy::LCDCommandMode()
{
  *csport |= cspinmask; // why are we doing this twice?
  *csport |= cspinmask;
  *dcport &= ~dcpinmask;
  *csport &= ~cspinmask;
}


/* Power Management */

void Arduboy::idle()
{
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
}

void Arduboy::saveMuchPower()
{
  power_adc_disable();
  power_usart0_disable();
  power_twi_disable();
  // timer 0 is for millis()
  // timers 1 and 3 are for music and sounds
  power_timer2_disable();
  power_usart1_disable();
  // we need USB, for now (to allow triggered reboots to reprogram)
  // power_usb_disable()
}


/* Frame management */

void Arduboy::setFrameRate(uint8_t rate)
{
  frameRate = rate;
  eachFrameMillis = 1000/rate;
}

bool Arduboy::nextFrame()
{
  long now = millis();
  uint8_t remaining;

  // post render
  if (post_render) {
    lastFrameDurationMs = now - lastFrameStart;
    frameCount++;
    post_render = false;
  }

  // if it's not time for the next frame yet
  if (now < nextFrameStart) {
    remaining = nextFrameStart - now;
    // if we have more than 1ms to spare, lets sleep
    // we should be woken up by timer0 every 1ms, so this should be ok
    if (remaining > 1)
      idle();
    return false;
  }

  // pre-render

  // technically next frame should be last frame + each frame but if we're
  // running a slow render we would constnatly be behind the clock
  // keep an eye on this and see how it works.  If it works well the
  // lastFrameStart variable could be eliminated completely
  nextFrameStart = now + eachFrameMillis;
  lastFrameStart = now;
  post_render = true;
  return post_render;
}

// returns the load on the CPU as a percentage
// this is based on how much of the time your app is spends rendering
// frames.  This number can be higher than 100 if your app is rendering
// really slowly.
int Arduboy::cpuLoad()
{
  return lastFrameDurationMs*100 / eachFrameMillis;
}

// seed the random number generator with entropy from the temperature,
// voltage reading, and microseconds since boot.
// this method is still most effective when called semi-randomly such
// as after a user hits a button to start a game or other semi-random
// events
void Arduboy::initRandomSeed()
{
  power_adc_enable(); // ADC on
  randomSeed(~rawADC(ADC_TEMP) * ~rawADC(ADC_VOLTAGE) * ~micros() + micros());
  power_adc_disable(); // ADC off
}

uint16_t Arduboy::rawADC(byte adc_bits)
{
  ADMUX = adc_bits;
  // we also need MUX5 for temperature check
  if (adc_bits == ADC_TEMP) {
    ADCSRB = _BV(MUX5);
  }

  delay(2); // Wait for ADMUX setting to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  return ADCW;
}


/* Graphics */

void Arduboy::blank()
{
  for (int a = 0; a < (HEIGHT*WIDTH)/8; a++) SPI.transfer(0x00);
}

void Arduboy::clearDisplay()
{
  for (int a = 0; a < (HEIGHT*WIDTH)/8; a++) sBuffer[a] = 0x00;
}

void Arduboy::drawPixel(int x, int y, uint8_t color)
{
  #ifdef PIXEL_SAFE_MODE
  if (x < 0 || x > (WIDTH-1) || y < 0 || y > (HEIGHT-1))
  {
    return;
  }
  #endif

  uint8_t row = (uint8_t)y / 8;
  if (color)
  {
    sBuffer[(row*WIDTH) + (uint8_t)x] |=   _BV((uint8_t)y % 8);
  }
  else
  {
    sBuffer[(row*WIDTH) + (uint8_t)x] &= ~ _BV((uint8_t)y % 8);
  }
}

uint8_t Arduboy::getPixel(uint8_t x, uint8_t y)
{
  uint8_t row = y / 8;
  uint8_t bit_position = y % 8;
  return (sBuffer[(row*WIDTH) + x] & _BV(bit_position)) >> bit_position;
}

void Arduboy::drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  drawPixel(x0, y0+r, color);
  drawPixel(x0, y0-r, color);
  drawPixel(x0+r, y0, color);
  drawPixel(x0-r, y0, color);

  while (x<y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void Arduboy::drawCircleHelper
(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint8_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x<y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornername & 0x4)
    {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2)
    {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8)
    {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1)
    {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void Arduboy::fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color)
{
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

void Arduboy::fillCircleHelper
(
 int16_t x0,
 int16_t y0,
 int16_t r,
 uint8_t cornername,
 int16_t delta,
 uint8_t color
)
{
  // used to do circles and roundrects!
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornername & 0x1)
    {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }

    if (cornername & 0x2)
    {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

void Arduboy::drawLine
(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
  // bresenham's algorithm - thx wikpedia
  boolean steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int8_t ystep;

  if (y0 < y1)
  {
    ystep = 1;
  }
  else
  {
    ystep = -1;
  }

  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      drawPixel(y0, x0, color);
    }
    else
    {
      drawPixel(x0, y0, color);
    }

    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}

void Arduboy::drawRect
(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}

void Arduboy::drawFastVLine
(int16_t x, int16_t y, int16_t h, uint8_t color)
{
  int end = y+h;
  for (int a = max(0,y); a < min(end,HEIGHT); a++)
  {
    drawPixel(x,a,color);
  }
}

void Arduboy::drawFastHLine
(int16_t x, int16_t y, int16_t w, uint8_t color)
{
  int end = x+w;
  for (int a = max(0,x); a < min(end,WIDTH); a++)
  {
    drawPixel(a,y,color);
  }
}

void Arduboy::fillRect
(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
  // stupidest version - update in subclasses if desired!
  for (int16_t i=x; i<x+w; i++)
  {
    drawFastVLine(i, y, h, color);
  }
}

void Arduboy::fillScreen(uint8_t color)
{
  fillRect(0, 0, WIDTH, HEIGHT, color);
}

void Arduboy::drawRoundRect
(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color)
{
  // smarter version
  drawFastHLine(x+r, y, w-2*r, color); // Top
  drawFastHLine(x+r, y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x, y+r, h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r, h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r, y+r, r, 1, color);
  drawCircleHelper(x+w-r-1, y+r, r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r, y+h-r-1, r, 8, color);
}

void Arduboy::fillRoundRect
(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color)
{
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r, y+r, r, 2, h-2*r-1, color);
}

void Arduboy::drawTriangle
(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void Arduboy::fillTriangle
(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{

  int16_t a, b, y, last;
  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1)
  {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2)
  {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1)
  {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2)
  { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)
    {
      a = x1;
    }
    else if(x1 > b)
    {
      b = x1;
    }
    if(x2 < a)
    {
      a = x2;
    }
    else if(x2 > b)
    {
      b = x2;
    }
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t dx01 = x1 - x0,
      dy01 = y1 - y0,
      dx02 = x2 - x0,
      dy02 = y2 - y0,
      dx12 = x2 - x1,
      dy12 = y2 - y1,
      sa = 0,
      sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
  {
    last = y1;   // Include y1 scanline
  }
  else
  {
    last = y1-1; // Skip it
  }


  for(y = y0; y <= last; y++)
  {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;

    if(a > b)
    {
      swap(a,b);
    }

    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);

  for(; y <= y2; y++)
  {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;

    if(a > b)
    {
      swap(a,b);
    }

    drawFastHLine(a, y, b-a+1, color);
  }
}

void Arduboy::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color) {
  // no need to dar at all of we're offscreen
  if (x+w < 0 || x > WIDTH-1 || y+h < 0 || y > HEIGHT-1)
    return;

  int yOffset = abs(y) % 8;
  int sRow = y / 8;
  if (y < 0) {
    sRow--;
    yOffset = 8 - yOffset;
  }
  int rows = h/8;
  if (h%8!=0) rows++;
  for (int a = 0; a < rows; a++) {
    int bRow = sRow + a;
    if (bRow > (HEIGHT/8)-1) break;
    if (bRow > -2) {
      for (int iCol = 0; iCol<w; iCol++) {
        if (iCol + x > (WIDTH-1)) break;
        if (iCol + x >= 0) {
          if (bRow >= 0) {
            if (color) this->sBuffer[ (bRow*WIDTH) + x + iCol  ]  |= pgm_read_byte(bitmap+(a*w)+iCol) << yOffset;
            else this->sBuffer[ (bRow*WIDTH) + x + iCol  ]  &= ~(pgm_read_byte(bitmap+(a*w)+iCol) << yOffset);
          }
          if (yOffset && bRow<(HEIGHT/8)-1 && bRow > -2) {
            if (color) this->sBuffer[ ((bRow+1)*WIDTH) + x + iCol  ] |= pgm_read_byte(bitmap+(a*w)+iCol) >> (8-yOffset);
            else this->sBuffer[ ((bRow+1)*WIDTH) + x + iCol  ] &= ~(pgm_read_byte(bitmap+(a*w)+iCol) >> (8-yOffset));
          }
        }
      }
    }
  }
}


// Draw images that are bit-oriented horizontally
//
// This requires a lot of additional CPU power and will draw images much
// more slowly than drawBitmap where the images are stored in a format that
// allows them to be directly written to the screen hardware fast. It is
// recommended you use drawBitmap when possible.
void Arduboy::drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color) {
  // no need to dar at all of we're offscreen
  if (x+w < 0 || x > WIDTH-1 || y+h < 0 || y > HEIGHT-1)
    return;

  int16_t xi, yi, byteWidth = (w + 7) / 8;
  for(yi = 0; yi < h; yi++) {
    for(xi = 0; xi < w; xi++ ) {
      if(pgm_read_byte(bitmap + yi * byteWidth + xi / 8) & (128 >> (xi & 7))) {
        drawPixel(x + xi, y + yi, color);
      }
    }
  }
}


void Arduboy::drawChar
(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)
{

  if ((x >= WIDTH) ||         // Clip right
    (y >= HEIGHT) ||        // Clip bottom
    ((x + 5 * size - 1) < 0) ||   // Clip left
    ((y + 8 * size - 1) < 0)    // Clip top
  )
  {
    return;
  }

  for (int8_t i=0; i<6; i++ )
  {
    uint8_t line;
    if (i == 5)
    {
      line = 0x0;
    }
    else
    {
      line = pgm_read_byte(font+(c*5)+i);
    }

    for (int8_t j = 0; j<8; j++)
    {
      if (line & 0x1)
      {
        if (size == 1) // default size
        {
          drawPixel(x+i, y+j, color);
        }
        else  // big size
        {
          fillRect(x+(i*size), y+(j*size), size, size, color);
        }
      }
      else if (bg != color)
      {
        if (size == 1) // default size
        {
          drawPixel(x+i, y+j, bg);
        }
        else
        {  // big size
          fillRect(x+i*size, y+j*size, size, size, bg);
        }
      }

      line >>= 1;
    }
  }
}

void Arduboy::setCursor(int16_t x, int16_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void Arduboy::setTextSize(uint8_t s)
{
  textsize = (s > 0) ? s : 1;
}

void Arduboy::setTextWrap(boolean w)
{
  wrap = w;
}

size_t Arduboy::write(uint8_t c)
{
  if (c == '\n')
  {
    cursor_y += textsize*8;
    cursor_x = 0;
  }
  else if (c == '\r')
  {
    // skip em
  }
  else
  {
    drawChar(cursor_x, cursor_y, c, 1, 0, textsize);
    cursor_x += textsize*6;
    if (wrap && (cursor_x > (WIDTH - textsize*6)))
    {
      cursor_y += textsize*8;
      cursor_x = 0;
    }
  }
}

void Arduboy::display()
{
  this->drawScreen(sBuffer);
}

void Arduboy::drawScreen(const unsigned char *image)
{
  for (int a = 0; a < (HEIGHT*WIDTH)/8; a++)
  {
    SPI.transfer(pgm_read_byte(image + a));
  }
}

void Arduboy::drawScreen(unsigned char image[])
{
  for (int a = 0; a < (HEIGHT*WIDTH)/8; a++)
  {
    SPI.transfer(image[a]);
  }
}

inline unsigned char* Arduboy::getBuffer(){
  return sBuffer;
}

uint8_t Arduboy::width() { return WIDTH; }

uint8_t Arduboy::height() { return HEIGHT; }

// returns true if the button mask passed in is pressed
//
//   if (pressed(LEFT_BUTTON + A_BUTTON))
boolean Arduboy::pressed(uint8_t buttons)
{
 uint8_t button_state = getInput();
 return (button_state & buttons) == buttons;
}

// returns true if the button mask passed in not pressed
//
//   if (not_pressed(LEFT_BUTTON))
boolean Arduboy::not_pressed(uint8_t buttons)
{
 uint8_t button_state = getInput();
 return (button_state & buttons) == 0;
}


uint8_t Arduboy::getInput()
{
  // using ports here is ~100 bytes smaller than digitalRead()
  #ifdef DEVKIT
  // down, left, up
  uint8_t buttons = ((~PINB) & B01110000);
  // right button
  buttons = buttons | (((~PINC) & B01000000) >> 4);
  // A and B
  buttons = buttons | (((~PINF) & B11000000) >> 6);
  #endif

  // b0dlu0rab - see button defines in Arduboy.h
  return buttons;
}

void Arduboy::swap(int16_t& a, int16_t& b) {
  int temp = a;
  a = b;
  b = temp;
}


void Arduboy::flipPixel(int x, int y) {
  
  uint8_t remainder = y & B00000111;
  y = y >> 3;
  uint8_t enable = B00000001 << remainder;
  
  sBuffer[(y*WIDTH) + x] ^= enable;
} 

void Arduboy::drawScreen1X(uint8_t xcur, uint8_t ycur) {
  
  uint8_t remainder = ycur & B00000111;
  ycur = ycur >> 3;
  uint8_t enable = B00000001 << remainder;
  
  sBuffer[(ycur*WIDTH) + xcur] ^= enable;
  display();
  sBuffer[(ycur*WIDTH) + xcur] ^= enable;
} 

void Arduboy::drawScreen2X(uint8_t xcur, uint8_t ycur) {
  uint8_t x_width = 64;
  uint8_t y_width = 32;

  scrollScreen(xcur, ycur, x_width, y_width);

  uint8_t x;
  uint8_t y;
  uint8_t tmp;
  uint8_t out;
  
   for(y = y_start; y < y_start + y_width; y += 4) {
     for(x = x_start; x < x_start + x_width; x++) {
      tmp = sBuffer[((y >> 3)*WIDTH) + x];
      if (y & B00000100) {
         tmp = tmp >> 4;
      } else {
         tmp = tmp & B00001111; 
      }
      
      out = 0;
      if (tmp & B00001000) { out |= B11000000;}
      if (tmp & B00000100) { out |= B00110000;}
      if (tmp & B00000010) { out |= B00001100;}
      if (tmp & B00000001) { out |= B00000011;}
      
      if (x == xcur && y == (ycur & B11111100)) {
        ycur = ycur & B00000011;
        switch (ycur) {
          case 0:
            SPI.transfer(out ^ B00000010);
            SPI.transfer(out ^ B00000001);
            break;
          case 1:
            SPI.transfer(out ^ B00001000);
            SPI.transfer(out ^ B00000100);
            break;
          case 2:
            SPI.transfer(out ^ B00100000);
            SPI.transfer(out ^ B00010000);
            break;
          case 3: 
            SPI.transfer(out ^ B10000000);
            SPI.transfer(out ^ B01000000);
            break; 
        }
      } else {     
        SPI.transfer(out);
        SPI.transfer(out);
      } 
    }
  }
}

void Arduboy::drawScreen4X(uint8_t xcur, uint8_t ycur) {
  uint8_t x_width = 32;
  uint8_t y_width = 16;

  scrollScreen(xcur, ycur, x_width, y_width);
  uint8_t x;
  uint8_t y;
  uint8_t tmp;
  uint8_t out;
  uint8_t sequence;
  
   for(y = y_start; y < y_start + y_width; y += 2) {
     for(x = x_start; x < x_start + x_width; x++) {
      tmp = sBuffer[((y >> 3)*WIDTH) + x];
      
      sequence = y & B00000110;
      
      switch (sequence) {
        case 6:
          tmp = (tmp & B11000000) >> 6;          
          break;
        case 4:
          tmp = (tmp & B00110000) >> 4;
          break;
        case 2:
          tmp = (tmp & B00001100) >> 2;
          break;
        case 0:
          tmp = (tmp & B00000011);
          break;
      }
           
      out = 0;
      if (tmp & B00000010) { out |= B11110000;}
      if (tmp & B00000001) { out |= B00001111;}
      
      if (x == xcur && y == (ycur & B11111110)) {
        ycur = ycur & B00000001;
        switch (ycur) {
          case 1:
            SPI.transfer(out ^ B10010000);
            SPI.transfer(out ^ B01100000);
            SPI.transfer(out ^ B01100000);
            SPI.transfer(out ^ B10010000);
            break;
          case 0:
            SPI.transfer(out ^ B00001001);
            SPI.transfer(out ^ B00000110);
            SPI.transfer(out ^ B00000110);
            SPI.transfer(out ^ B00001001);
            break;
        }
      } else {     
        SPI.transfer(out);
        SPI.transfer(out);
        SPI.transfer(out);
        SPI.transfer(out);
      } 
    }
  } 
}

void Arduboy::prepZoomSwitch(uint8_t zoom) 
{
  switch (zoom) {
    case 1: 
      x_start = 0;
      y_start = 0;
      break;
    case 2:
      x_start = 32;
      y_start = 16;
      break;
    case 4:
      x_start = 48;
      y_start = 24;
      break;
  }
}
  
void Arduboy::scrollScreen(uint8_t xcur, uint8_t ycur, uint8_t width, uint8_t height) 
{
  if (xcur < x_start) {
    x_start = xcur & B11111100;
  }
  if (ycur < y_start) {
    y_start = ycur & B11111100;
  }
  if (xcur >= (x_start + width)) {
    x_start = (xcur & B11111100) - width + 4;
  }
  if (ycur >= (y_start + height)) {
    y_start = (ycur & B11111100) - height + 4;
  }  
}

void Arduboy::writeCode(uint8_t width, uint8_t height)
{
  Serial.print (F("const static unsigned char image[] PROGMEM =\n{\n"));
  
  uint8_t i;
  uint8_t j;
  uint8_t count = 0;
  uint8_t b1;
  uint8_t b2;

  switch(width) {
    case 8:
      for(i = 60; i < 68; i++)
      {
        b1 = 0x00;
        b2 = 0x00;
        b1 = sBuffer[(3*WIDTH) + (uint8_t)i];          
        b2 = sBuffer[(4*WIDTH) + (uint8_t)i];         
        b1 = b1 >> 4;
        b2 = b2 << 4;
        b1 = b1 + b2;
        if((count & B00000111) == 0)
        {
          tunes.delay(50);
          Serial.print("\n");
        }
        Serial.print("0x");
        print2Hex(b1);
        if (i != 67)
        {
          Serial.print(", ");
        }
      }
      break;
    case 16:
      for(j = 3; j < 5; j++)
      {
        for(i = 56; i < 72; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            tunes.delay(50);
            Serial.print("\n  ");
          }
          Serial.print("0x");
          print2Hex(b1);
          if (i != 71 || j != 4)
          {
            tunes.delay(50);
            Serial.print(", ");
          }
          count++;
        }
      }
      break;
    case 32:
      for(j = 2; j < 6; j++)
      {
        for(i = 48; i < 80; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            tunes.delay(50);
            Serial.print("\n  ");
          }
          Serial.print("0x");
          print2Hex(b1);
          if (i != 79 || j != 5)
          {
            tunes.delay(50);
            Serial.print(", ");
          }
          count++;
        }
      }
      break;
    case 64:
      for(j = 0; j < 8; j++)
      {
        for(i = 32; i < 96; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            tunes.delay(50);
            Serial.print("\n  ");
          }
          Serial.print("0x");
          print2Hex(b1);
          if (i != 95 || j != 7)
          {
            tunes.delay(50);
            Serial.print(", ");
          }
          count++;
        }
      }
      break;
    case 128:
      for(j = 0; j < 128; j++)
      {
        for(i = 0; i < 8; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            tunes.delay(50);
            Serial.print("\n  ");
          }
          Serial.print("0x");
          print2Hex(b1);
          if (i != 127 || j != 7)
          {
            tunes.delay(50);
            Serial.print(", ");
          }
          count++;
        }
      }
      break;
  }
  
  Serial.print (F("\n};\n"));  
}


void Arduboy::writeSVG(uint8_t width, uint8_t height)
{
  int wval = width * 5 + 1;
  int hval = height * 5 + 1;
  Serial.print(F("<svg width=\""));
  Serial.print(wval, DEC);
  Serial.print(F("\" height=\""));
  Serial.print(hval, DEC);
  Serial.print(F("\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n  <rect width=\""));
  Serial.print(wval, DEC);
  Serial.print(F("\" height=\""));
  Serial.print(hval, DEC);
  Serial.print(F("\" fill=\"black\" />\n"));

  uint8_t i;
  uint8_t j;
  uint8_t count = 0;
  uint8_t b1;
  uint8_t b2;
  
  switch(width)
  {
    case 8:
      for(i = 60; i < 68; i++)
      {
        b1 = 0x00;
        b2 = 0x00;
        b1 = sBuffer[(3*WIDTH) + (uint8_t)i];          
        b2 = sBuffer[(4*WIDTH) + (uint8_t)i];         
        b1 = b1 >> 4;
        b2 = b2 << 4;
        b1 = b1 + b2;
        svgByte(i - 60, 0, b1);
      }
      break;
    case 16:
      for(j = 3; j < 5; j++)
      {
        for(i = 56; i < 72; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          svgByte(i - 56, j - 3, b1);
        }
      }
      break;
    case 32:
      for(j = 2; j < 6; j++)
      {
        for(i = 48; i < 80; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          svgByte(i - 48, j-2, b1);
        }
      }
      break;
    case 64:
      for(j = 0; j < 8; j++)
      {
        for(i = 32; i < 96; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
         svgByte(i - 32, j, b1);
        }
      }
      break;
    case 128:
      for(j = 0; j < 8; j++)
      {
        for(i = 0; i < 128; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          svgByte(i, j, b1);
        }
      }
      break;
  }  
  Serial.print(F("</svg>"));
}

void Arduboy::svgWrite()
{
  Serial.print(F("<svg width=\"641\" height=\"321\""));
  Serial.print(F("\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n"));
  Serial.print(F("<rect width=\"641\" height=\"321\" fill=\"black\""));

  uint8_t i;
  uint8_t j;

  for(j = 0; j < 8; j++)
  {
    for(i = 0; i < 128; i++)
    {   
      svgByte(i, j, sBuffer[(j*WIDTH) + (uint8_t)i]);
    }
  }  

  Serial.print(F("</svg>"));
}

void Arduboy::svgByte(uint8_t x, uint8_t y, uint8_t pixel)
{
  y = y * 8;    // Bytes to bits
  if(pixel & B00000001) { svgPixel(x, y); } 
  if(pixel & B00000010) { svgPixel(x, y + 1); } 
  if(pixel & B00000100) { svgPixel(x, y + 2); }  
  if(pixel & B00001000) { svgPixel(x, y + 3); } 
  if(pixel & B00010000) { svgPixel(x, y + 4); } 
  if(pixel & B00100000) { svgPixel(x, y + 5); } 
  if(pixel & B01000000) { svgPixel(x, y + 6); } 
  if(pixel & B10000000) { svgPixel(x, y + 7); } 
}

void Arduboy::svgPixel(uint8_t x, uint8_t y)
{
  int xval = x * 5 + 1;
  int yval = y * 5 + 1;  

  Serial.print(F("  <rect rx=\"1\" ry=\"1\" width=\"4\" height=\"4\" x=\""));
  Serial.print(xval, DEC);
  Serial.print(F("\" y=\""));
  Serial.print(yval, DEC);
  Serial.print(F("\" fill=\"white\" />\n"));
}

void Arduboy::writeHex(uint8_t width, uint8_t height) 
{
  print2Hex(width);
  print2Hex(height);

  uint8_t i;
  uint8_t j;
  uint8_t count = 0;
  uint8_t b1;
  uint8_t b2;
  
  switch(width)
  {
    case 8:
      for(i = 60; i < 68; i++)
      {
        b1 = 0x00;
        b2 = 0x00;
        b1 = sBuffer[(3*WIDTH) + (uint8_t)i];          
        b2 = sBuffer[(4*WIDTH) + (uint8_t)i];         
        b1 = b1 >> 4;
        b2 = b2 << 4;
        b1 = b1 + b2;
        if((count & B00000111) == 0)
        {
          Serial.print("\n");
        }
        print2Hex(b1);
        count++;
      }
      break;
    case 16:
      for(j = 3; j < 5; j++)
      {
        for(i = 56; i < 72; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            Serial.print("\n");
          }
          print2Hex(b1);
          count++;
        }
      }
      break;
    case 32:
      for(j = 2; j < 6; j++)
      {
        for(i = 48; i < 80; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            Serial.print("\n");
          }
          print2Hex(b1);
          count++;
        }
      }
      break;
    case 64:
      for(j = 0; j < 8; j++)
      {
        for(i = 32; i < 96; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            Serial.print("\n");
          }
          print2Hex(b1);
          count++;
        }
      }
      break;
    case 128:
      for(j = 0; j < 8; j++)
      {
        for(i = 0; i < 128; i++)
        {
          b1 = sBuffer[(j*WIDTH) + (uint8_t)i];          
          if((count & B00000111) == 0)
          {
            Serial.print("\n");
          }
          print2Hex(b1);
          count++;
        }
      }
      break;
  } 
  Serial.print("\n");
}

void Arduboy::print2Hex(uint8_t ch) 
{
   if(ch < 16)
   {
     Serial.print('0');
   }
   Serial.print(ch, HEX);
}
