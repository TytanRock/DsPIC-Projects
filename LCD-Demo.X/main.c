/*
 * File:   main.c
 * Author: Cory
 *
 * Created on March 2, 2020, 7:47 PM
 */


#include "xc.h"
#include "i2cDriver.h"
#include "lcdDriver.h"

#pragma config FNOSC = FRC
#pragma config POSCMD = NONE
#pragma config OSCIOFNC = OFF
#pragma config FCKSM = CSDCMD
#pragma config FWDTEN = OFF

static struct {
    unsigned long time;
} _module = {0} ;

void InitTimer() {
    T1CONbits.TGATE = 0;
    PR1 = 8000; // 8 Mhz Clock, which means 1 ms is 8000 cycles
    T1CONbits.TON = 1;
    
    IEC0bits.T1IE = 0;
    IFS0bits.T1IF = 0;
}

int main(void) {
    InitTimer();
    InitI2C();
    
    int startcall = 1;
    
    while(1) {
        I2CProcess();
        LcdProcess();
        
        if(IFS0bits.T1IF) {
            IFS0bits.T1IF = 0;
            TMR1 = 0;
            
            /* 1 ms process tasks are here */
            LcdProcess1Ms();
            
            /* Do whatever you want here */
            if(_module.time < 0xFFff) {           
                _module.time++;
    
            }
        }
        
        
        
        if(Ready() && startcall) {
            startcall = 0;
            Backlight();
            Print("ABCDEFGHIJKLM");
            SetCursor(0,1);
            Print("NOPQRSTUVWXYZ");
        }
    }
    
    return 0;
}
