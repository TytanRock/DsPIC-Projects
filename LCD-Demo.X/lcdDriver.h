#ifndef __LCD_DRIVER_H_
#define	__LCD_DRIVER_H_

#include <xc.h> // include processor files - each processor file is guarded.  


#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

void Print(char *str);
void LcdProcess();
void LcdProcess1Ms();
int Ready();

void SetCursor(uint8_t col, uint8_t row);

// Turn the display on/off (quickly)
void NoDisplay();
void Display();

// Turns the underline cursor on/off
void NoCursor();
void Cursor();

// Turn on and off the blinking cursor
void NoBlink();
void Blink();

// These Commands scroll the display without changing the RAM
void ScrollDisplayLeft(void);
void ScrollDisplayRight(void);

// This is for text that flows Left to Right
void LeftToRight(void);

// This is for text that flows Right to Left
void RightToLeft(void);

// This will 'right justify' text from the cursor
void Autoscroll(void);

// This will 'left justify' text from the cursor
void NoAutoscroll(void);

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void CreateChar(uint8_t location, uint8_t charmap[]);

// Turn the (optional) backlight off/on
void NoBacklight(void);
void Backlight(void);
int GetBacklight();

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif	/* XC_HEADER_TEMPLATE_H */

