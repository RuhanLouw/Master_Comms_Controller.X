/* 
 * File:   pinDef.h
 * Author: Ruhan Louw
 *
 * Created on July 25, 2025, 9:51 AM
 */

#ifndef PINDEF_H
#define	PINDEF_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "mcc_generated_files/system/pins.h"
#include "mcc_generated_files/system/port.h"    

#define RUN_LED PD6
#define ERR_LED PD5
#define TXD_UART0 PA5
#define RXD_UART0 PA4
#define TX3_LED PC4
#define RX3_LED PC5
#define AC_EVY_COIL_PWM PC6
#define COMP_ERROR_INTERRUPT PC7
#define AC_HEATER_RESISTOR_PWM PD0
#define DC_HEAT_FAN_PWM PD1
#define DC_EVAP_FAN_PWM PD2
#define DC_HEATER_RESISTOR_2_PWM PD3
#define AC_HEATER_RESISTOR_1_PWM PD4
#define TXD_UART1 PC0
#define RXD_UART1 PC1
#define XDIR_UART1 PC2
#define TXD_UART3 PB0
#define RXD_UART3 PB1
#define XDIR_UART3 PB2
#define TX0_LED1 PB3
#define RX0_LED1 PB4
#define XDIR_UART0 PA7
#define SCL_I2C0 PA3
#define SDA_I2C0 PA2
#define CS_USDcard PA1
#define CardDetect_USDcard PA0
#define SCK_SPI01 PE1
#define MISO_SPI01 PE0
#define MOSI_SPI01 PE2
#define ON_OFF_STATE_INPUT PE3
#define ON_OFF_STATE_CLK1 PE4
#define ON_OFF_STATE_LATCH PF1
#define ON_OFF_STATE_CLR PF5
#define MCU_UDPI PF6


#ifdef	__cplusplus
}
#endif

#endif	/* PINDEF_H */

