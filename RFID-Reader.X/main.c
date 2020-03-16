/*
 * File:   main.c
 * Author: Cory
 *
 * Created on September 1, 2019, 12:04 AM
 */


#include "xc.h"

#pragma config FNOSC = FRC
#pragma config POSCMD = NONE
#pragma config OSCIOFNC = OFF
#pragma config FCKSM = CSDCMD
#pragma config FWDTEN = OFF

#if 0
void InitClock()
{
// Configure Oscillator to operate the device at 40Mhz
// Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
// Fosc= 8M*40/(2*2)=80Mhz for 8M input clock
	PLLFBD=38;					// M=40
	CLKDIVbits.PLLPOST=0;		// N1=2
	CLKDIVbits.PLLPRE=0;		// N2=2
	OSCTUN=0;					// Tune FRC oscillator, if FRC is used

	
// Disable Watch Dog Timer
	RCONbits.SWDTEN=0;

// Clock switching to incorporate PLL
	__builtin_write_OSCCONH(0x03);		// Initiate Clock Switch to Primary
													// Oscillator with PLL (NOSC=0b011)
	__builtin_write_OSCCONL(0x01);		// Start clock switching
	while (OSCCONbits.COSC != 0b011);	// Wait for Clock switch to occur	

// Wait for PLL to lock
	while(OSCCONbits.LOCK!=1) {};
}
#endif

#define GENERATE_RANDOM (0b0010)
#define MEM             (0b0001)


/**
 *  SPI Pin assignments
 * MISO - RP15 (26)
 * MOSI - RP14 (25)
 * CLK  - RP13 (24)
 */

/**
 *  RPN Codes
 * MISO1 RPINR20
 * MOSI1 00111
 * CLK1  01000
 */

/**
 * RFID chip documentation:
 * MSB-order
 * Data provided on falling edge, stable on rising edge
 * First byte is length of message
 */

void StartTransaction()
{
    PORTBbits.RB12 = 0;
}
void EndTransaction()
{
    PORTBbits.RB12 = 1;
}

void SendSPIByte(uint8_t b)
{
    while(SPI1STATbits.SPITBF) ; // Wait until we can write
    /* Dummy read to clear SPIRBF flag */
    SPI1BUF;
    SPI1BUF = b;
    while(!SPI1STATbits.SPIRBF) ; // Wait until we clock out the data
}

uint8_t ReadSPIByte()
{
    return SPI1BUF;
}

uint8_t SendReceiveSPIByte(uint8_t b)
{
    SendSPIByte(b);
    return ReadSPIByte();
}

void WriteRegister(uint8_t b)
{
    SendSPIByte(0x00 | (b << 1));
}

uint8_t ReadRegister(uint8_t b)
{
    return SendReceiveSPIByte(0x80 | (b << 1));
}

void ReadData(uint8_t *dat, unsigned int count)
{
    int i = 0;
    for(i = 0; i < count; ++i)
    {
        dat[i] = ReadRegister(dat[i]);
    }
}

void InitializeSPIDriver()
{
    RPINR20bits.SDI1R = 15;     // RP15 (RB15)
    RPOR7bits.RP14R = 0b00111;  // RP14 (RB14)
    RPOR6bits.RP13R = 0b01000;  // RP13 (RB13)
    TRISBbits.TRISB12 = 0;      // RP12 (RB12)
    PORTBbits.RB12 = 1; /* Keep it high */
    
    SPI1STAT = 0;
    
    /* Actual SPI register configuration */
    SPI1CON1bits.MODE16 = 0x0; // Byte-communication
    SPI1CON1bits.CKE = 0x1;    // Data changes on falling edge
    SPI1CON1bits.CKP = 0x0;    // Active state is high
    SPI1CON1bits.MSTEN = 0x1;  // Master-Mode
    SPI1CON1bits.SPRE = 0b000;
    SPI1CON1bits.PPRE = 0b11;
    
    SPI1STATbits.SPIEN = 1;
}

void WaitUntilNopCommand()
{
    StartTransaction();
    ReadRegister(0x01);
    while((ReadRegister(0x01) & 0x0F) != 0x00) ;
    SendSPIByte(0); // End read
    EndTransaction();
}

unsigned int GetRandomFromRFID()
{
    /* Flush FIFO Buffer */
    StartTransaction();
    WriteRegister(0x0A);
    SendSPIByte(0x80);
    EndTransaction();
    WaitUntilNopCommand();
    
    /* Set the command for random */
    StartTransaction();
    WriteRegister(0x01);
    SendSPIByte(0x00 | GENERATE_RANDOM); /* Just generate random */
    EndTransaction();
    WaitUntilNopCommand();
    
    /* Set the command for mem */
    StartTransaction();
    WriteRegister(0x01);
    SendSPIByte(0x00 | MEM); /* Just move internal buffer to FIFO */
    EndTransaction();
    WaitUntilNopCommand();
    
    /* Read FIFO */
    StartTransaction();
    ReadRegister(0x0A); // Prep read
    while(ReadRegister(0x0A) != 25) ; // Read
    SendSPIByte(0); // End read
    EndTransaction();
    
    uint8_t dat[11];
    int i = 0;
    for(i = 0 ; i < 10; ++i)
    {
        dat[i] = 0x09;
    }
    dat[10] = 0x00; /* Null terminate transaction */
    
    StartTransaction();
    ReadData(dat, 11);
    EndTransaction();
    
    /* Flush FIFO Buffer */
    StartTransaction();
    WriteRegister(0x0A);
    SendSPIByte(0x80);
    EndTransaction();
    WaitUntilNopCommand();
    
    /* Just return the first two bytes appended together */
    unsigned int ret = ((unsigned int) dat[3] << 8) | dat[4];
    return ret;
}

int main(void) {
    int i;
    InitializeSPIDriver();
    
    TRISA = 0x00;
    unsigned int random = 0;
    while(1)
    {
        PORTAbits.RA0 = 1;
        for(i = 0; i < 0x7FFF; ++i) ;
        PORTAbits.RA0 = 0;
        do { random = GetRandomFromRFID(); } while(random > 0x5);
    }
    
    return 0;
}
