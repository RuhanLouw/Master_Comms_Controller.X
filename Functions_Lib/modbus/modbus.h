/* 
 * File:   modbus.h
 * Author: Ruhan Louw
 *
 * Created on August 15, 2025, 10:40 AM
 */

#ifndef MODBUS_H
#define	MODBUS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <stdint.h>
#include "../../mcc_generated_files/uart/usart0.h"
#include "../../mcc_generated_files/uart/usart3.h"
    
#include "../../mcc_generated_files/timer/tcb0.h"
#include "../../mcc_generated_files/timer/tcb1.h"

    
    
/* Ports */
#define PORT_PI         0   // USART0 (Pi connection)
#define PORT_SENSOR     1   // USART3 (Sensor connection)
#define NUM_PORTS       2

/* Defaults you can change */
#define MAX_FRAME_SIZE  128     // RX/TX buffer size
#define CONTROLLER_SLAVE_ID 1   // Modbus address for this controller
#define SILENCE_TICKS   911     // TCB ticks for ~3.5 char @ chosen baud
#define SENSOR_TIMEOUT_MS 1000  // overall timeout for sensor response (ms)

/* Public API */

/**
 * Initialize modbus driver.
 * @param holding_registers_ptr pointer to uint16_t holding registers array
 * @param reg_count count of holding registers available
 */
void controller_modbus_init(uint16_t *holding_registers_ptr, uint16_t reg_count);

/**
 * Call periodically from main loop to process RX frames.
 * This will respond to Pi requests (0x03/0x10) and handle completed frames.
 */
void controller_modbus_process(void);

/*
 * Blocking master read from sensor.
 * @param sensor_slave_id sensor Modbus slave id
 * @param reg_addr register start
 * @param reg_count number of registers
 * @param out_data destination buffer (must have reg_count elements)
 * @return true if successful, false on timeout/CRC/error
 */
bool controller_read_sensor_holding(uint8_t sensor_slave_id,
                                    uint16_t reg_addr,
                                    uint16_t reg_count,
                                    uint16_t *out_data);

/**
 * Blocking master write (function 16) to sensor.
 */
bool controller_write_sensor_multiple(uint8_t sensor_slave_id,
                                      uint16_t reg_addr,
                                      uint16_t reg_count,
                                      const uint16_t *data);

/**
 * ISR/Callback hooks: call these from your USART RX ISRs when a byte is received.
 * The library will assemble frames automatically and mark them available for processing.
 */
void controller_pi_rx_callback(void);       // call from USART0 RX ISR
void controller_sensor_rx_callback(void);   // call from USART3 RX ISR

/**
 * Call these from USART TX complete ISRs if you prefer interrupt driven TX.
 * They are not required if you use the polling TX approach (provided below).
 */
void controller_pi_tx_complete_callback(void);
void controller_sensor_tx_complete_callback(void);

/**
 * Application callback: called when Pi writes to holding registers.
 * Implement this in your application to react to register changes.
 *
 * @param values array of values written
 * @param addr starting register index
 * @param count number of registers written
 */
void controller_update_holding_registers(const uint16_t *values, uint16_t addr, uint16_t count);


//// Port definitions for RS485 communication
//#define PORT_PI             0       // USART0: Controller as slave to Pi
//#define PORT_SENSOR         1       // USART3: Controller as master to sensor
//#define NUM_PORTS           2       // Total number of ports
//#define MAX_FRAME_SIZE      64      // Max Modbus frame size (bytes)
//#define CONTROLLER_SLAVE_ID 1       // Controller's slave ID for Pi
//#define SILENCE_TICKS       911     // ~3.645 ms at 9600 baud (3.5 chars, 16 MHz / 64)
//#define TIMEOUT_CYCLES      4       // TCB2 cycles for ~1000 ms (4 * 262 ms)
//
//// DE/RE pins for RS485 transceivers
//#define DE_PI_PORT          PORTA   // Port for Pi DE pin UART0
//#define DE_PI_PIN           7       // Pin PA7 for Pi RS485 DE/RE
//#define DE_SENSOR_PORT      PORTB   // Port for sensor DE pin UART3
//#define DE_SENSOR_PIN       3       // Pin PB3 for sensor RS485 DE/RE
//
//// Initialize Modbus with holding registers and timers (TCB0, TCB1, TCB2)
//void controller_modbus_init(uint16_t *holding_registers, uint16_t reg_count);
//
//// Master functions: Controller sends requests to sensor
//bool controller_read_sensor_holding(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, uint16_t *data);
//bool controller_write_sensor_multiple(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, const uint16_t *data);
//
//// Slave function: Controller processes Pi requests
//void controller_handle_pi_request(void);
//
//// RX callbacks for USART0 (Pi) and USART3 (sensor)
//void controller_pi_rx_callback(void);
//void controller_sensor_rx_callback(void);
//
//// TX complete callbacks for USART0 (Pi) and USART3 (sensor)
//void controller_pi_tx_complete_callback(void);
//void controller_sensor_tx_complete_callback(void);
//
//// Application callback: Handle Pi writing to Controller registers
//void controller_update_holding_registers(const uint16_t *registers, uint16_t addr, uint16_t count);


#ifdef	__cplusplus
}
#endif

#endif	/* MODBUS_H */

