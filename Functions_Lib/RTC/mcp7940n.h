/* 
 * File:   mcp7940n.h
 * Author: Ruhan Louw
 *
 * Created on June 29, 2025, 2:52 PM
 */

#ifndef MCP7940N_H
#define	MCP7940N_H

#ifdef	__cplusplus
extern "C" {
#endif
    
    //=======

#include <avr/io.h>
#include "../../mcc_generated_files/i2c_host/twi0.h"
#include <stdbool.h>
#include <stdint.h>

// I2C address (7-bit)
#define MCP7940N_ADDR             0x6F

// --- Register Addresses ---
#define MCP7940N_REG_RTCSEC       0x00
#define MCP7940N_REG_RTCMIN       0x01
#define MCP7940N_REG_RTCHOUR      0x02
#define MCP7940N_REG_RTCWKDAY     0x03
#define MCP7940N_REG_RTCDATE      0x04
#define MCP7940N_REG_RTCMTH       0x05
#define MCP7940N_REG_RTCYEAR      0x06
#define MCP7940N_REG_CONTROL      0x07

// --- Bit Masks ---
#define MCP7940N_ST               (1 << 7)  // Start oscillator
#define MCP7940N_VBATEN           (1 << 3)  // Enable battery backup
#define MCP7940N_OSCRUN           (1 << 5)  // Oscillator running
#define MCP7940N_EXTOSC           (1 << 3)  // External oscillator (CONTROL register)

// --- Time Structure ---
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t dayOfWeek; // 1?7
    uint8_t day;
    uint8_t month;
    uint16_t year;     // e.g. 2025
} rtc_time_t;

// --- API ---
void RTC_Init(void);
bool RTC_HasValidTime(void);
bool RTC_SetTime(const rtc_time_t *t);
bool RTC_GetTime(rtc_time_t *t);



#ifdef	__cplusplus
}
#endif

#endif	/* MCP7940N_H */

