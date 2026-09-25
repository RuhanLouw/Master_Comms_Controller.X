/* 
 * File:   shift_register.h
 * Author: Ruhan Louw
 *
 * Created on August 1, 2025, 2:29 PM
 */

#ifndef SHIFT_REGISTER_H
#define	SHIFT_REGISTER_H

#ifdef	__cplusplus
extern "C" {
#endif

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include "../../mcc_generated_files/system/port.h"
#include "../../mcc_generated_files/system/pins.h"
    
// Pin definitions
//#define DATA_PIN_bm   PIN3_bm  // PE0 as DS (data)
//#define CLOCK_PIN_bm  PIN0_bm  // PF0 as SHCP (clock)
//#define LATCH_PIN_bm  PIN1_bm  // PF1 as STCP (latch)
//#define CLEAR_PIN_bm  PIN4_bm  // PF4 as NCLR (not clear)
//#define DATA_PORT     PORTE    // Data port
//#define CLOCK_PORT    PORTF    // Clock port
//#define LATCH_PORT    PORTF    // Latch port
//#define CLEAR_PORT    PORTF    // Clear port


// === Pin definitions ===
#define DATA_PORT      PORTE
#define DATA_PIN_bp    PIN3_bp
#define DATA_PIN_bm    PIN3_bm

#define CLOCK_PORT     PORTF
#define CLOCK_PIN_bp   PIN0_bp
#define CLOCK_PIN_bm   PIN0_bm
    
#define LATCH_PORT     PORTF
#define LATCH_PIN_bp   PIN1_bp
#define LATCH_PIN_bm   PIN1_bm

#define CLEAR_PORT     PORTF
#define CLEAR_PIN_bp   PIN4_bp
#define CLEAR_PIN_bm   PIN4_bm

// === Helper Macros ===
#define SR_CLEAR_HIGH()  IO_PF4_SetHigh()
#define SR_CLEAR_LOW()   IO_PF4_SetLow()

#define SR_LATCH_HIGH()  IO_PF1_SetHigh()
#define SR_LATCH_LOW()   IO_PF1_SetLow()

#define SR_DATA_HIGH()   IO_PE3_SetHigh()
#define SR_DATA_LOW()    IO_PE3_SetLow()

#define SR_CLOCK_HIGH()  IO_PF0_SetHigh()
#define SR_CLOCK_LOW()   IO_PF0_SetLow()

void SR_Init(void);
void SR_Clear(void);
void SR_Write(uint8_t data);
void SR_COIL_REG_update(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SHIFT_REGISTER_H */






