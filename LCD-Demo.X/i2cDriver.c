/*
 * File:   i2cDriver.c
 * Author: Cory
 *
 * Created on March 7, 2020, 6:14 PM
 */


#include "xc.h"
#include "i2cDriver.h"

#define TRANSACTION_COUNT 16
#define BYTE_COUNT 16
#define MAX_RETRIES 16

enum states {
    Idle,
    Start,
    Address,
    AddressAck,
    Data_T,
    Data_R,
    Data_TAck,
    Data_RAck,
    Data_RAckAck,
    Stop,
    StopAck,
    Retry,
    Fail,
};

typedef struct _transaction_t {
    uint8_t addr;
    uint8_t dat[BYTE_COUNT];
    unsigned int start;
    unsigned int end;
    
    void (*callbackFunction)(uint8_t *);
    
    unsigned read : 1;
}transaction_t;

static struct {
    enum states st;
    
    unsigned int retryCnt;
    
    transaction_t transactions[TRANSACTION_COUNT];
    unsigned int startCnt;
    unsigned int endCnt;
    
    volatile unsigned callback : 1;
    
}_module;

void incrementStart() {
    _module.startCnt++;
    if(_module.startCnt >= TRANSACTION_COUNT) _module.startCnt = 0;
}
void incrementEnd() {
    _module.endCnt++;
    if(_module.endCnt >= TRANSACTION_COUNT) _module.endCnt = 0;
}
int messageCnt() {
    int cnt = _module.endCnt - _module.startCnt;
    if(cnt >= 0) return cnt;
    else return cnt + TRANSACTION_COUNT;
}

/*=============================================================================
I2C Master Interrupt Service Routine
=============================================================================*/
void __attribute__((interrupt, no_auto_psv)) _MI2C1Interrupt(void)
{
    _module.callback = 1;
    IFS1bits.MI2C1IF = 0;		//Clear the DMA0 Interrupt Flag;
}

/*=============================================================================
I2C Slave Interrupt Service Routine
=============================================================================*/
void __attribute__((interrupt, no_auto_psv)) _SI2C1Interrupt(void)
{
    IFS1bits.SI2C1IF = 0;		//Clear the DMA0 Interrupt Flag
}

void InitI2C() {
    I2C1CONbits.I2CSIDL = 1;
    I2C1CONbits.SCLREL = 0;
    I2C1CONbits.I2CSIDL = 1;
    I2C1CONbits.I2CEN = 1;
    
    // Configure SCA/SDA pin as open-drain
    ODCBbits.ODCB9 = 1;
    ODCBbits.ODCB8 = 1;


	I2C1CONbits.A10M=0;
	I2C1CONbits.SCLREL=1;
	I2C1BRG=300;

	I2C1ADD=0;
	I2C1MSK=0;

	I2C1CONbits.I2CEN = 1; /* Enable I2C module */
	IEC1bits.MI2C1IE = 1; /* Enable master interrupt */
  	IFS1bits.MI2C1IF = 0; /* Disable slave interrupt */
}

void I2CProcess() {
    
    transaction_t *queued = &_module.transactions[_module.startCnt];
    
    /* State Machine Stuff */
    switch(_module.st) {
        case Idle:
            /* Check if we have messages, if we do, move to start */
            if (messageCnt() > 0) _module.st = Start;
            break;
        case Start:
            /* Tell module to send start bit */
            I2C1CONbits.SEN = 1;
            _module.st = Address;
            break;
        case Address:
            /* Check to see if start bit is sent */
            if(!_module.callback) break;
            _module.callback = 0;
            /* Send out address */
            I2C1TRN = (queued->addr << 1) | queued->read;
            _module.st = AddressAck;
            break;
        case AddressAck:
            /* Check to see if address is sent */
            if(!_module.callback) break;
            _module.callback = 0;
            /* Check Address Acknowledge */
            if(I2C1STATbits.ACKSTAT) {
                /* NACK received */
                /* Check if we've exceeded maximum retries */
                if(_module.retryCnt >= MAX_RETRIES) {
                    _module.st = Fail;
                } else {
                    /* Try again */
                    _module.retryCnt++;
                    _module.st = Retry;
                }
            } else {
                /* ACK Received */
                _module.retryCnt = 0;
                /* Continue to data bit depending on read state */
                if(queued->read) {
                    I2C1CONbits.RCEN;
                    _module.st = Data_R;
                } else {
                    _module.st = Data_T;
                }
            }
            break;
        case Data_R:
            /* Check to see if we've read a value */
            if(!_module.callback) break;
            _module.callback = 0;
            /* We received a byte, let's store it */
            queued->dat[queued->start] = I2C1RCV;
            /* Move on to acknowledge */
            _module.st = Data_RAck;
            break;
        case Data_RAck:
            /* Figure out if we should acknowledge or not */
            /* Start with incrementing our start */
            queued->start++;
            /* Now see if we've reached the end */
            if(queued->start >= queued->end) {
                /* We've reached the end, let's NACK and Stop */
                I2C1CONbits.ACKDT = 1;
            } else {
                /* We need more data, let's ACK and continue */
                I2C1CONbits.ACKDT = 0;
            }
            /* Initiate acknowledge sequence now */
            _module.st = Data_RAckAck;
            I2C1CONbits.ACKEN = 1;
            break;
        case Data_RAckAck:
            /* Check if we've acknowledged */
            if(!_module.callback) break;
            _module.callback = 0;
            if(queued->start >= queued->end) {
                /* Move on to stop */
                _module.st = Stop;
            } else {
                /* Move on to get more data */
                I2C1CONbits.RCEN = 1;
                _module.st = Data_R;
            }
            break;
        case Data_T:
            /* Send out byte */
            I2C1TRN = queued->dat[queued->start];
            /* Go to data acknowledge now */
            _module.st = Data_TAck;
            break;
        case Data_TAck:
            /* Check if we've sent data */
            if(!_module.callback) break;
            _module.callback = 0;
            /* Check Transmission Acknowledge */
            if(I2C1STATbits.ACKSTAT) {
                /* NACK received */
                /* Check if we've exceeded maximum retries */
                if(_module.retryCnt >= MAX_RETRIES) {
                    _module.st = Fail;
                } else {
                    /* Try again */
                    _module.retryCnt++;
                    _module.st = Retry;
                }
            } else {
                /* ACK Received */
                _module.retryCnt = 0;
                queued->start++;
                /* Determine if we stop or continue */
                if(queued->start >= queued->end) {
                    /* We've reached the end */
                    _module.st = Stop;
                } else {
                    /* Continue to data bit */
                    _module.st = Data_T;
                }
            }
            break;
        case Stop:
            /* Send stop bit */
            I2C1CONbits.PEN = 1;
            /* Move to next transaction */
            incrementStart();
            (*queued->callbackFunction)(queued->dat);
            _module.st = StopAck;
            break;
        case StopAck:
            /* Check if we've Stopped */
            if(!_module.callback) break;
            _module.callback = 0;
            
            _module.st = Idle;
            break;
        case Retry:
            _module.st = Start;
            break;
        case Fail:
            incrementStart();
            (*queued->callbackFunction)(0);
            _module.st = Idle;
            break;
    }
}

int CreateTransaction(uint8_t address, uint8_t *bytes, unsigned int byteCnt, int read, void (*callback)(uint8_t*)) {
    if(messageCnt() >= TRANSACTION_COUNT) return -1;
    if(byteCnt > BYTE_COUNT) return -2;
    
    transaction_t *toFill = &_module.transactions[_module.endCnt];
    toFill->addr = address;
    int i = 0;
    for(; i < byteCnt; ++i) {
        toFill->dat[i] = bytes[i];
    }
    toFill->start = 0;
    toFill->end = byteCnt;
    toFill->read = read;
    toFill->callbackFunction = callback;
    
    incrementEnd();
    return 0;
}