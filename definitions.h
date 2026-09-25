/* 
 * File:   definitions.h
 * Author: Ruhan Louw
 *
 * Created on July 20, 2025, 8:29 PM
 */

#ifndef DEFINITIONS_H
#define	DEFINITIONS_H

#include "mcc_generated_files/system/pins.h"


#ifdef	__cplusplus
extern "C" {
#endif
    

// UART Pins
    
    //RS485 1 <-> uart0
#define TXD_UART_0 PA4
#define RXD_UART_0 PA5
#define TXDir_UART_0 PA7
#define TX0_LED PB4
#define RX0_LED PB5

    //RS485 2 <-> uart3
#define TXD_UART_3 PB0
#define RXD_UART_3 PB1
#define TXDir_UART_3 PB3
#define TX3_LED PC4
#define RX3_LED PC5

    //debug <_> uart1
#define TXD_UART_1 PC0
#define RXD_UART_1 PC1
#define TXDir_UART_1 PC3

// I2C Pins
#define SCL_I2C0 PA3
#define SDA_I2C0 PA2

// USDcard Pins
#define CS_USDcard PA1
#define CardDetect_USDcard PA0

// Interrupt Pin
#define v PC7

// SPI Pins
#define SCK_SPI0 PE2
#define MISO_SPI0 PE1
#define MOSI_SPI0 PE0

// State Control Pins
#define ON_OFF_STATE_INPUT PE3
#define ON_OFF_STATE_LATCH PF1
#define ON_OFF_STATE_CLK PF0
#define ON_OFF_STATE_nCLR PF4
  
// PWM Pins
#define AC_EEV_COIL_PWM_port_pin PC6
#define AC_Heater_Resistor_PWM_port_pin PD0
#define DC_HEAT_FAN_PWM_port_pin PD1
#define DC_EVAP_FAN_PWM_port_pin PD2
#define DC_Heater_Resistor_1_PWM_port_pin PD3
#define DC_Heater_Resistor_2_PWM_port_pin PD4

// On Off Switches    
#define AC_COMPRESSOR   (1 << 0)  // QA
#define AC_FAN          (1 << 1)  // QB
#define AC_DEFROST      (1 << 2)  // QC
#define NULL_SR_PIN     (0 << 3)  // QD
#define DC_COMPESSOR    (1 << 4)  // QE  
#define GREEN_PILOT     (1 << 5)  // QF
#define ORANGE_PILOT    (1 << 6)  // QG
#define RED_PILOT       (1 << 7)  // QH


// Reset Pin
#define RESET PE7

// LED Indicators
#define ERR_LED PD5
#define RUN_LED PD6
    
#define LED_TX_PI_ON()          IO_PB4_SetHigh()
#define LED_TX_PI_OFF()         IO_PB4_SetLow()
#define LED_TX_PI_TOGGLE()      IO_PB4_Toggle()

#define LED_RX_PI_ON()          IO_PB5_SetHigh()
#define LED_RX_PI_OFF()         IO_PB5_SetLow()
#define LED_RX_PI_TOGGLE()      IO_PB5_Toggle()
        
#define LED_TX_SENSOR_ON()      IO_PC4_SetHigh()
#define LED_TX_SENSOR_OFF()     IO_PC4_SetLow()
#define LED_TX_SENSOR_toggle()  IO_PC4_Toggle()
        
#define LED_RX_SENSOR_ON()      IO_PC5_SetHigh()
#define LED_RX_SENSOR_OFF()     IO_PC5_SetLow()
#define LED_RX_SENSOR_toggle()  IO_PC5_Toggle()
    
#define LED_ERR_ON()            IO_PD5_SetHigh()
#define LED_ERR_OFF()           IO_PD5_SetLow()
#define LED_ERR_toggle()        IO_PD5_Toggle()
    
#define LED_RUN_ON()            IO_PD6_SetHigh()
#define LED_RUN_OFF()           IO_PD6_SetLow()
#define LED_RUN_TOGGLE()        IO_PD6_Toggle()
#define LED_RUN_toggle()        IO_PD6_Toggle()    
/*
 Timers' posisions
 TCB for MODBUS
 * Silence 3.5 char
 *  -TCB0 Pi
 *  -TCB1 Sensor
 * TCB2 Timeout
 TCA for PWM
 * TCA0 for 6s EEV period
 * TCA1 for rest
 
 
 */

#ifdef	__cplusplus
}
#endif

#endif	/* DEFINITIONS_H */

