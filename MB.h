/* 
 * File:   MB.h
 * Author: Ruhan Louw
 *
 * Created on October 26, 2025, 3:39 PM
 */

#ifndef MB_H
#define	MB_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "system_registers.h"
#include "definitions.h"
#include "mcc_generated_files/uart/usart0.h"
#include "mcc_generated_files/uart/usart1.h"
#include "mcc_generated_files/uart/usart3.h"
#include "mcc_generated_files/timer/tcb0.h"
#include "mcc_generated_files/timer/tcb1.h"

#define PI 0
#define SENSOR 1
#define SLAVE_ID_PI    1   // UART0: Slave to Pi
#define MY_MASTER_ID      2   // UART3: Master to Sensor
#define SENSOR_SLAVE_ID   2   // Sensor card ID
/* RS485 DE pins (change if needed) */
#define DE_PI_PORT      PORTA
#define DE_PI_PIN       7
#define DE_SENSOR_PORT  PORTB
#define DE_SENSOR_PIN   3

#define MAX_FRAME_LENGTH 255U

/* Normal sensor link uses USART3 through the isolated RS485 interface. */
#define SENSOR_LINK_TTL_DEBUG_TEST 0

/* Keep disabled during normal operation; the Pi remains on RS485 port 1. */
#define SENSOR_PORT_PI_SLAVE_TEST  0
    
extern volatile bool _POLL_SENSOR;
/* Callbacks */
// UART INTERRUPTS
void PI_RX_RECEIVE(void);
void PI_TX_COMPLETE(void);
void SENSOR_RX_RECEIVE(void);
void SENSOR_TX_COMPLETE(void);
// UART TIMER INTERRUPTS
void PI_FRAME_COMPLETE(void);
void SENSOR_FRAME_COMPLETE(void);
void TX_DELAY_COMPLETE(void);
void MB_SensorTick1s(void);

void modbus_master_read_holding(uint16_t start, uint16_t count);
void modbus_master_write_multiple(uint16_t start, const uint16_t* values, uint16_t count);
void process_pi(void);
void process_sensor(void);
void poll_sensor(void);
/* INIT */
void MB_INIT(void);

void MB_process(void);
    

    
    
    

#ifdef	__cplusplus
}
#endif

#endif	/* MB_H */

