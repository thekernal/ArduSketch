/*********************************************************
 *                                                       *
 *                       ARDUSKETCH                      *
 *                                                       *
 *                               Author: Jamie Saunders  *
 *                                 Date: June 15, 2015   *
 *                                                       *
 *     This program is a paint program designed for the  *
 * easy creation of bitmaps to later be used on the      *
 * arduboy system.                                       *
 *********************************************************/

/*

 ***********
 * License *
 ***********

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#include <SPI.h>    // Used For For The Screen
#include <Wire.h>
#include <EEPROM.h>

#include "Arduboy.h"
#include "audio.h"
#include "glcdfont.c"

/**
 * Program Information
 */
#define GAME_TITLE        F("ARDUSKETCH")
#define GAME_VERSION      F("0.1.1")

/**
 * Delay Values 
 */
#define FRAME_DELAY       20
#define SCROLL_DELAY      5

/**
 * Defined Screen Modes
 */
#define MODE_SPLASH       1
#define MODE_INSTRUCTIONS 2
#define MODE_CREDITS      3
#define MODE_SIZE_SELECT  4
#define MODE_DRAW         5
#define MODE_MENU         6

/**
 * Arduboy Interfaces
 */ 
Arduboy display;
ArduboyTunes audio;   

/**
 * System State Variables
 */
unsigned short input        = 0;
unsigned short next_mode    = 0;
unsigned short current_mode = 0;
unsigned short delay_scroll = 0;
unsigned short size_option  = 0;

/**
 * Cursor and Image Information
 */
unsigned short cursor_x     = 0;
unsigned short cursor_x_min = 0;
unsigned short cursor_x_max = 127;
unsigned short cursor_y     = 0;
unsigned short cursor_y_min = 0;
unsigned short cursor_y_max = 63;
unsigned short cursor_on    = 0;
unsigned short zoom_option  = 1;
unsigned short image_size_x = 128;
unsigned short image_size_y = 64;

// Menu Buffer used to write directly to display this prevents screen buffer from being deleted.
unsigned char menu[] = {
  ' ', ' ', 'M', 'E' ,'N' ,'U', ' ', ' ', ' ', ' ', 
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  'B', 'A', 'C', 'K', ' ', ' ', ' ', ' ', ' ', ' ',
  'C', 'L', 'E', 'A' ,'R', ' ', ' ', ' ', ' ', ' ',
  'Z', 'O', 'O', 'M' ,':', '?', 'X', ' ', ' ', ' ',
  'P', 'R', 'I', 'N', 'T', ' ', 'C', 'O' ,'D', 'E', 
  'P', 'R', 'I', 'N', 'T', ' ', 'S', 'V', 'G', ' ',
  'M', 'A', 'I', 'N', ' ', 'M', 'E', 'N', 'U', ' '
};

// Line Index of Arrow on Menu
unsigned char menu_option = 2;

/**********************************
 * COMPILED ASSETS                *
 **********************************/

// 8x8 Image of Arrow used in Menus
const static unsigned char  arrow[] PROGMEM =
{
  0x18, 0x18, 0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x18
};

// Audio Chord for Startup
const unsigned char PROGMEM startup[] = {
  0x90, 72, 0, 160, 0x80, 0, 10,
  0x90, 77, 1, 144, 0x80, 0xF0
};


/*********************
 * Functions         *
 *********************/
 
/**
 * Function: setup()
 * 
 * This function initializes arduboy and plays intro
 */
void setup()
{
  display.start();
  intro();
}

/**
 * Function: intro()
 *
 * This function preforms arduboy intro animation in similar 
 * fashion to classic gameboy. Will likely be replaced when /
 * if a standard intro is created.
 */
void intro()
{
  for(int i = -8; i < 28; i = i + 2)
  {
    display.clearDisplay();
    display.setCursor(46, i);
    display.print(F("ARDUBOY"));
    display.display();
    audio.delay(10);
  }
  display.setFrameRate(0);

  audio.playScore(startup);
  audio.delay(2000);
}

/**
 * Function : loop
 * 
 * This routine is the main program loop.
 * 
 *     1. Read Input
 *     2. Handle Current Mode
 *     3. Display if Necessary
 *     4. Repeat After Delay
 */
void loop() 
{
  input = display.getInput();
  
  switch (next_mode) 
  {
     case MODE_SPLASH:
       screen_splash();
       break;
     case MODE_INSTRUCTIONS:
       screen_instructions();
       break;
     case MODE_CREDITS:
       screen_credits();
       break;
     case MODE_SIZE_SELECT:
       screen_size_select();
       break;
     case MODE_DRAW:
       screen_draw();
       break;
     case MODE_MENU:
       screen_menu();
       break;
     default:
       next_mode = MODE_SPLASH;
  }
  
  if (current_mode != MODE_DRAW &&
      current_mode != MODE_MENU) {
    display.display();
  }
  audio.delay(FRAME_DELAY);
}

/**
 * Function : screen_splash()
 *
 * This routine displays programs Splash screen.
 * 
 * TODO: Add a 64x64 Image to thr Right Side of Screen Generated 
 *       by the program itself
 */
void screen_splash()
{
  if (next_mode != current_mode)
  {
    current_mode = MODE_SPLASH;
    delay_scroll = SCROLL_DELAY;
  }
  
  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    if (input & LEFT_BUTTON)  { next_mode = MODE_CREDITS; }
    if (input & RIGHT_BUTTON) { next_mode = MODE_INSTRUCTIONS; }
    if (input & A_BUTTON)     { next_mode = MODE_SIZE_SELECT; }
  }   

  display.clearDisplay();
  display.setCursor(8,8);
  display.setTextSize(1);
  display.print(F("ARDU-"));
  display.setCursor(13,18);
  display.print(F("SKETCH"));

}


/**
 * Function : screen_instructions()
 *
 * Displays use instructions to the screen
 */
void screen_instructions()
{
  if (next_mode != current_mode)
  {
    current_mode = MODE_INSTRUCTIONS;
    delay_scroll = SCROLL_DELAY;
  }
  
  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    if (input & LEFT_BUTTON)  { next_mode = MODE_SPLASH; }
    if (input & RIGHT_BUTTON) { next_mode = MODE_CREDITS; }
    if (input & A_BUTTON)     { next_mode = MODE_SIZE_SELECT; }
  }   

  display.clearDisplay();
  display.setCursor(34,0);
  display.setTextSize(1);
  display.print(F("INSTRUCTIONS"));
  display.setCursor(8,20);
  display.print(F("A - TOGGLE PIXEL"));
  display.setCursor(8,30);
  display.print(F("B - MENU"));

}

/**
 * Function : screen_credits()
 *
 * Displays Credit information
 */
void screen_credits()
{
  if (next_mode != current_mode)
  {
    current_mode = MODE_CREDITS;
    delay_scroll = SCROLL_DELAY;
  }
  
  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    if (input & LEFT_BUTTON)  { next_mode = MODE_INSTRUCTIONS; }
    if (input & RIGHT_BUTTON) { next_mode = MODE_SPLASH; }
    if (input & A_BUTTON)     { next_mode = MODE_SIZE_SELECT; }
  }   

  display.clearDisplay();
  display.setCursor(43,0);
  display.setTextSize(1);
  display.print(F("CREDITS"));
  display.setCursor(8,15);
  display.print(F("JAMIE SAUNDERS"));
  display.setCursor(43,36);
  display.print(F("VERSION"));
  display.setCursor(8,51);
  display.print(GAME_VERSION);
}

/**
 * Function: screen_size_select()
 *
 * lists the available screen sizes and waits until one is 
 * selected.
 */
void screen_size_select()
{
  if (next_mode != current_mode)
  {
    current_mode = MODE_SIZE_SELECT;
    delay_scroll = SCROLL_DELAY;
    size_option = 0;
  }
  
  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    
    if (input & UP_BUTTON)    { delay_scroll = SCROLL_DELAY;   if (size_option) {size_option--;}}
    if (input & DOWN_BUTTON)  { delay_scroll = SCROLL_DELAY; size_option++;  if (size_option >4) {size_option=4;}}
    if (input & A_BUTTON)     { next_mode = MODE_DRAW; }
  }   

  display.clearDisplay();
  display.setCursor(34,0);
  display.setTextSize(1);
  display.print(F("SIZE SELECT"));
  display.setCursor(16,20);
  display.print(F("8x8"));
  display.setCursor(16,30);
  display.print(F("16x16"));
  display.setCursor(16,40);
  display.print(F("32x32"));
  display.setCursor(16,50);
  display.print(F("64x64"));
  display.setCursor(74,20);
  display.print(F("128x64"));
  
  switch (size_option) 
  {
    case 0:
       display.drawBitmap(4,21, arrow, 8, 8, 1);
       break;
    case 1:
       display.drawBitmap(4,31, arrow, 8, 8, 1);
       break;
    case 2:
       display.drawBitmap(4,41, arrow, 8, 8, 1);
       break;
    case 3:
       display.drawBitmap(4,51, arrow, 8, 8, 1);
       break;
    case 4:
       display.drawBitmap(54,21, arrow, 8, 8, 1);
       break;
  }
}

/**
 *
 */
void screen_draw(){
  if (next_mode != current_mode)
  {
    
    current_mode = MODE_DRAW;
    delay_scroll = SCROLL_DELAY;
    prep_display();
    display.prepZoomSwitch(zoom_option); 
  }

  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    if (input & UP_BUTTON)    { if (cursor_y > cursor_y_min) { cursor_y--;} delay_scroll = 5;}
    if (input & DOWN_BUTTON)  { if (cursor_y < cursor_y_max) { cursor_y++;} delay_scroll = 5;}
    if (input & LEFT_BUTTON)  { if (cursor_x > cursor_x_min) { cursor_x--;} delay_scroll = 5;}
    if (input & RIGHT_BUTTON) { if (cursor_x < cursor_x_max) { cursor_x++;} delay_scroll = 5;}
    if (input & A_BUTTON)     { display.flipPixel(cursor_x, cursor_y); delay_scroll = 5; }
    if (input & B_BUTTON)     { next_mode = MODE_MENU; }
  }  

  switch (zoom_option)
  {
    case 1 : display.drawScreen1X(cursor_x, cursor_y); break;
    case 2 : display.drawScreen2X(cursor_x, cursor_y); break;
    case 4 : display.drawScreen4X(cursor_x, cursor_y); break;
  }  
}

void screen_menu()
{
  if (next_mode != current_mode)
  { 
    current_mode = MODE_MENU;
    delay_scroll = SCROLL_DELAY;
    menu_option  = 2;
  }

  if (delay_scroll)
  {
    delay_scroll--;
  } else {
    if (input & UP_BUTTON)    { if(menu_option > 2) { menu_option--; } delay_scroll = SCROLL_DELAY;}
    if (input & DOWN_BUTTON)  { if(menu_option < 7) { menu_option++; } delay_scroll = SCROLL_DELAY;}
    if (input & A_BUTTON)     { 
       switch (menu_option) {
         case 2: 
           next_mode = MODE_DRAW;
           current_mode = MODE_DRAW;
           delay_scroll = SCROLL_DELAY;
           break;
         case 3:
           next_mode = MODE_DRAW;
           break;
         case 4:
           zoom_option = zoom_option << 1;
           if (zoom_option > 4)  { zoom_option = 1; }
           display.prepZoomSwitch(zoom_option); 
           delay_scroll = SCROLL_DELAY;
           break;
         case 5:
           display.writeCode(image_size_x, image_size_y);
           // Print Code;
           break;
         case 6:
           display.writeSVG(image_size_x, image_size_y);
           break;
         case 7:
           next_mode = MODE_SPLASH;
           break;
       }
    }
    if (input & B_BUTTON)     { next_mode = MODE_DRAW; current_mode = MODE_DRAW; delay_scroll = SCROLL_DELAY;}
  } 

  switch (zoom_option) {
    case 1: menu[45] = '1'; break;
    case 2: menu[45] = '2'; break;
    case 4: menu[45] = '4'; break;
    default: zoom_option = 1; menu[45] = '1'; break;
  }
  
  unsigned short i = 0;
  unsigned short j = 0;
  for (i = 0; i < 8; i++)
  {
    for(j = 0; j < 4; j++)
    {
      SPI.transfer(0x00);
    }
    for(j = 0; j < 8; j++)
    {
      if (i == menu_option)
      {
        SPI.transfer(pgm_read_byte(arrow+j));
      } else {
        SPI.transfer(0x00);
      }
    }
    for(j = 0; j < 4; j++)
    {
      SPI.transfer(0x00);
    }
    for(j = 0; j < 10; j++)
    {
      unsigned long ref = menu[i*10 + j];
      ref = ref * 5;
   //   ref = font + ref;
      
      SPI.transfer(pgm_read_byte(font+ref));
      SPI.transfer(pgm_read_byte(font+ref+1));
      SPI.transfer(pgm_read_byte(font+ref+2));
      SPI.transfer(pgm_read_byte(font+ref+3));
      SPI.transfer(pgm_read_byte(font+ref+4));
      SPI.transfer(0x00);
    }
    for(j = 0; j < 52; j++)
    {
      SPI.transfer(0x00);
    }    
  }
}

void prep_display()
{
  display.clearDisplay();
  
  switch (size_option) {
    case 0: 
      display.drawRect(59,27,10, 10, 1);
      cursor_x     = 60;
      cursor_x_min = 60;
      cursor_x_max = 67;
      cursor_y     = 28;
      cursor_y_min = 28;
      cursor_y_max = 35; 
      image_size_x = 8;
      image_size_y = 8;
      break;
    case 1:
      display.drawRect(55,23,18,18, 1);
      cursor_x     = 56;
      cursor_x_min = 56;
      cursor_x_max = 71;
      cursor_y     = 24;
      cursor_y_min = 24;
      cursor_y_max = 39; 
      image_size_x = 16;
      image_size_y = 16;
      break;
    case 2:
      display.drawRect(47,15,34,34, 1);
      cursor_x     = 48;
      cursor_x_min = 48;
      cursor_x_max = 79;
      cursor_y     = 16;
      cursor_y_min = 16;
      cursor_y_max = 47;
      image_size_x = 32;
      image_size_y = 32;      
      break;
    case 3:
      display.drawFastVLine(31,0,64,1);
      display.drawFastVLine(96,0,64,1);     
      cursor_x     = 32;
      cursor_x_min = 32;
      cursor_x_max = 95;
      cursor_y     = 0;
      cursor_y_min = 0;
      cursor_y_max = 63; 
      image_size_x = 64;
      image_size_y = 64;
      break;
    case 4:
      cursor_x     = 0;
      cursor_x_min = 0;
      cursor_x_max = 127;
      cursor_y     = 0;
      cursor_y_min = 0;
      cursor_y_max = 63; 
      image_size_x = 128;
      image_size_y = 64;
      break;
  }
}


