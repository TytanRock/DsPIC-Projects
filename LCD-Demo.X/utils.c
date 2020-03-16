/*
 * File:   utils.c
 * Author: Cory
 *
 * Created on March 14, 2020, 8:46 PM
 */


#include "xc.h"
#include "global.h"
#include "utils.h"

void DelayMicroseconds(unsigned int usec) {
    /* one mip is 1/10th of a microsecond */
    unsigned long i = 0;
    
    /* 10 instructions per mip */
    for(; i < (usec * MIPS); ++i) {
        // 1 instruction to check
        // 1 instruction to increment
        // 8 more instructions inside bracket
        Nop();
        Nop();
        Nop();
        Nop();
        Nop();
        Nop();
        Nop();
        Nop();
    }
}