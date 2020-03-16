/*
 * File:   lcdDriver.c
 * Author: Cory
 *
 * Created on March 8, 2020, 3:54 PM
 */


#include "xc.h"
#include "i2cDriver.h"
#include "utils.h"

#define ADDRESS 0x27

// Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

#define En 0x4 // Enable bit
#define Rw 0x2 // Read/Write bit
#define Rs 0x1 // Register select bit

enum LCDStates {
    Startup, /* Initialize module */
    ResetBacklight,
    FourBitMode1, /* Prep 4 bit mode x1 */
    FourBitMode2, /* Prep 4 bit mode x2 */
    FourBitMode3, /* Prep 4 bit mode x3 */
    FourBitMode4, /* Actually enter 4 bit mode */
    FinishInit, /* Finish off initialization */
    
    Idle,
    WriteChar,
};

static struct {
    enum LCDStates st;
    
    uint8_t displayFunction;
    uint8_t displayMode;
    uint8_t displayControl;
    uint8_t cols;
    uint8_t rows;
    uint8_t charsize;
    
    uint16_t time;
    
    unsigned backlight : 1; // Keep track of backlight state
    volatile unsigned i2cFinished : 1; // Set when callback is called
} _module = {0};

/****** Utility functions that get called occasionally *******/

void EmptyCallback(uint8_t *dat) {
    
}

void SetCallback(uint8_t *dat) {
    _module.i2cFinished = 1;
}

/****** Low Level Commands that interact with I2C driver *******/
void ExpanderWrite(uint8_t value,  void (*callback)(uint8_t*)) {
    uint8_t dat = value;
    dat |= (_module.backlight << 3);
    if(CreateTransaction(ADDRESS, &dat, 1, 0, callback) < 0) {
        while (1) ;
    }
}

void PulseEnable(uint8_t data) {
    ExpanderWrite(data | En, SetCallback);
    while (_module.i2cFinished == 0) I2CProcess(); // Wait for i2cFinished to be true
    _module.i2cFinished = 0;
    DelayMicroseconds(1);
    
    ExpanderWrite(data & ~En, SetCallback);
    while (_module.i2cFinished == 0) I2CProcess(); // Wait for i2cFinished to be true
    _module.i2cFinished = 0;
    DelayMicroseconds(50);
}

void Write4Bits(uint8_t value) {
    ExpanderWrite(value, EmptyCallback);
    PulseEnable(value);
}

/****** Mid Level Commands that simplify high level API *******/

void Send(uint8_t value, uint8_t mode) {
    uint8_t highNib = value & 0xF0;
    uint8_t lowNib = (value << 4) & 0xF0;
    Write4Bits(highNib | mode);
    Write4Bits(lowNib | mode);
}
inline void Command(uint8_t value) {
    Send(value, 0);
}
inline int Write(uint8_t value) {
    Send(value, Rs);
    return 1;
}

/****** High Level Commands that user calls *******/


void Clear(){
	Command(LCD_CLEARDISPLAY);// clear display, set cursor position to zero
	DelayMicroseconds(2000);  // this Command takes a long time!
}

void Home(){
	Command(LCD_RETURNHOME);  // set cursor position to zero
	DelayMicroseconds(2000);  // this Command takes a long time!
}

void SetCursor(uint8_t col, uint8_t row){
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (row > _module.rows) {
		row = _module.rows-1;    // we count rows starting w/0
	}
	Command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void NoDisplay() {
	_module.displayControl &= ~LCD_DISPLAYON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}
void Display() {
	_module.displayControl |= LCD_DISPLAYON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}

// Turns the underline cursor on/off
void NoCursor() {
	_module.displayControl &= ~LCD_CURSORON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}
void Cursor() {
	_module.displayControl |= LCD_CURSORON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}

// Turn on and off the blinking cursor
void NoBlink() {
	_module.displayControl &= ~LCD_BLINKON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}
void Blink() {
	_module.displayControl |= LCD_BLINKON;
	Command(LCD_DISPLAYCONTROL | _module.displayControl);
}

// These Commands scroll the display without changing the RAM
void ScrollDisplayLeft(void) {
	Command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void ScrollDisplayRight(void) {
	Command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LeftToRight(void) {
	_module.displayMode |= LCD_ENTRYLEFT;
	Command(LCD_ENTRYMODESET | _module.displayMode);
}

// This is for text that flows Right to Left
void RightToLeft(void) {
	_module.displayMode &= ~LCD_ENTRYLEFT;
	Command(LCD_ENTRYMODESET | _module.displayMode);
}

// This will 'right justify' text from the cursor
void Autoscroll(void) {
	_module.displayMode |= LCD_ENTRYSHIFTINCREMENT;
	Command(LCD_ENTRYMODESET | _module.displayMode);
}

// This will 'left justify' text from the cursor
void NoAutoscroll(void) {
	_module.displayMode &= ~LCD_ENTRYSHIFTINCREMENT;
	Command(LCD_ENTRYMODESET | _module.displayMode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void CreateChar(uint8_t location, uint8_t charmap[]) {
	location &= 0x7; // we only have 8 locations 0-7
	Command(LCD_SETCGRAMADDR | (location << 3));
    int i = 0;
	for (; i<8; i++) {
		Write(charmap[i]);
	}
}

// Turn the (optional) backlight off/on
void NoBacklight(void) {
	_module.backlight = 0;
	ExpanderWrite(0, EmptyCallback);
}

void Backlight(void) {
	_module.backlight = 1;
	ExpanderWrite(0, EmptyCallback);
}
int GetBacklight() {
  return _module.backlight == 1;
}

void Print(char *str) {
    int i = 0;
    for(; str[i] != 0; ++i) {
        Write(str[i]);
    }
}

void LcdProcess() {
    switch(_module.st) {
        case Startup:
            _module.cols = 16;
            _module.rows = 2;
            _module.charsize = LCD_5x8DOTS;
            _module.displayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
            _module.displayFunction |= LCD_2LINE;
            _module.time = 0;
            _module.st = ResetBacklight;
            break;
        case ResetBacklight:
            if(_module.time < 50) break;
            _module.time = 0;
            ExpanderWrite((_module.backlight << 3), EmptyCallback);
            
            _module.st = FourBitMode1;
            break;
        case FourBitMode1:
            if(_module.time < 1000) break;
            _module.time = 0;
            Write4Bits(0x03 << 4);
            _module.st = FourBitMode2;
            break;
        case FourBitMode2:
            if(_module.time < 5) break;
            _module.time = 0;
            Write4Bits(0x03 << 4);
            _module.st = FourBitMode3;
            break;
        case FourBitMode3:
            if(_module.time < 5) break;
            _module.time = 0;
            Write4Bits(0x03 << 4);
            _module.st = FourBitMode4;
            break;
        case FourBitMode4:
            if(_module.time < 1) break;
            _module.time = 0;
            Write4Bits(0x02 << 4);
            _module.st = FinishInit;
            break;
        case FinishInit:
            Command(LCD_FUNCTIONSET | _module.displayFunction);
            _module.displayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
            Display();
            Clear();
            _module.displayMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
            Command(LCD_ENTRYMODESET | _module.displayMode);
            Home();
            
            _module.st = Idle;
            break;
            
        case Idle:
            break;
        case WriteChar:
            break;
            
    }
}

void LcdProcess1Ms() {
    if(_module.time < 0xFFff) {
        _module.time++;
    }
}

int Ready() {
    return _module.st == Idle;
}
