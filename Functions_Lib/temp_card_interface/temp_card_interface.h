/* 
 * File:   temp_card_interface.h
 * Author: Ruhan Louw
 *
 * Created on June 29, 2025, 4:27 PM
 */

#ifndef TEMP_CARD_INTERFACE_H
#define	TEMP_CARD_INTERFACE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h> // For uint8_t, uint16_t types
#include <stdbool.h> // For bool type

// Message format constants
#define START_BYTE      0xAA  // Start of message, marks beginning of a packet
#define END_BYTE        0xBB  // End of message, marks end of a packet
#define MAX_PAYLOAD_LEN 18    // Maximum payload length for responses (9 sensors x 2 bytes)
#define SENSOR_COUNT    9     // Total sensors (8 NTC + 1 K-type)

// Command codes (Master to Sensor PCB)
//#define CMD_READ_NTC_1           0x01  // Read NTC sensor 1 temperature
//#define CMD_READ_NTC_2           0x02  // Read NTC sensor 2 temperature
//#define CMD_READ_NTC_3           0x03  // Read NTC sensor 3 temperature
//#define CMD_READ_NTC_4           0x04  // Read NTC sensor 4 temperature
//#define CMD_READ_NTC_5           0x05  // Read NTC sensor 5 temperature
//#define CMD_READ_NTC_6           0x06  // Read NTC sensor 6 temperature
//#define CMD_READ_NTC_7           0x07  // Read NTC sensor 7 temperature
//#define CMD_READ_NTC_8           0x08  // Read NTC sensor 8 temperature
//#define CMD_READ_KTYPE           0x09  // Read K-type thermocouple temperature
//#define CMD_READ_ALL_NTC         0x0A  // Read all NTC sensors (1-8)
#define CMD_READ_ALL_SENSORS     0x0B  // Read all sensors (NTC 1-8 + K-type) //len=1, cmd = 0xFF
#define CMD_READ_SELECTED_SENSORS 0x0C  // Read selected sensors (2-byte bitmask payload)
#define CMD_STATUS               0x0D  // Request sensor PCB status (e.g., sensor health)
#define CMD_RESET                0x0E  // Reset sensor PCB
#define CMD_DEBUG                0x0F  // Request debug info (e.g., raw ADC values)

// Sensor selection bitmasks (for CMD_READ_SELECTED_SENSORS)
#define SENSOR_NTC_1    0x0001  // Bit 0: NTC sensor 1
#define SENSOR_NTC_2    0x0002  // Bit 1: NTC sensor 2
#define SENSOR_NTC_3    0x0004  // Bit 2: NTC sensor 3
#define SENSOR_NTC_4    0x0008  // Bit 3: NTC sensor 4
#define SENSOR_NTC_5    0x0010  // Bit 4: NTC sensor 5
#define SENSOR_NTC_6    0x0020  // Bit 5: NTC sensor 6
#define SENSOR_NTC_7    0x0040  // Bit 6: NTC sensor 7
#define SENSOR_NTC_8    0x0080  // Bit 7: NTC sensor 8
#define SENSOR_KTYPE    0x0100  // Bit 8: K-type thermocouple
// Bits 9-15 reserved for future expansion, must be 0

// Response codes (Sensor PCB to Master)
#define RESP_OK                 0x00  // Successful response (used in status payload)
#define RESP_ERROR              0xFF  // Error response
#define ERROR_SENSOR_FAIL       0x01  // Error: Sensor failure or not connected
#define ERROR_INVALID_CMD       0x02  // Error: Invalid command received
#define ERROR_INVALID_SENSOR    0x03  // Error: Invalid sensor selection (reserved bits set)
#define ERROR_INVALID_RANGE     0x04  // Error: Temperature measurement out of Range (ntc and ktype differ)

// Timing and buffer constants
#define RESPONSE_TIMEOUT_MS 100 // Timeout for master to wait for response (ms)
#define UART_BAUD_RATE     9600 // UART baud rate for AVR128DB32
#define MAX_MESSAGE_LEN    (MAX_PAYLOAD_LEN + 5) // Start + Cmd + Len + Payload + Chksum + End
// Structure to store temperature readings
typedef struct {
    int16_t temperatures[SENSOR_COUNT]; // Array for 8 NTC + 1 K-type (°C × 10)
    uint8_t count; // Number of temperatures received
} SensorData;

    void UART1_RxISR(void);
    void sendCommand(uint8_t cmd, uint8_t* payload, uint8_t len);
    void sendSelectedSensorsCommand(uint16_t sensorBitmask);
    bool processResponse(SensorData* data);
    
//#include <stdint.h>
//    
//
//
//// Function declarations for RS485 communication
//void RS485_init(uint32_t baudrate);
//void RS485_send(uint8_t *data, uint16_t length);
//uint16_t RS485_receive(uint8_t *buffer, uint16_t max_length);
//
//// Function to read temperature from a specific sensor
//uint16_t read_sensor_temperature(uint8_t sensor_id);
//
//typedef struct{
//    uint8_t read_1,
//    uint8_t read_all
//}temp_cmd;

#ifdef	__cplusplus
}
#endif

#endif	/* TEMP_CARD_INTERFACE_H */

