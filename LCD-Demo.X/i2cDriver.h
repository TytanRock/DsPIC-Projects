
#ifndef __I2C_DRIVER_H_
#define	__I2C_DRIVER_H_

#include <xc.h> // include processor files - each processor file is guarded.  

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */
    
    void InitI2C();
    void I2CProcess();
    int CreateTransaction(uint8_t address, uint8_t *bytes, unsigned int byteCnt, int read, void (*callback)(uint8_t*));

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif	/* XC_HEADER_TEMPLATE_H */

