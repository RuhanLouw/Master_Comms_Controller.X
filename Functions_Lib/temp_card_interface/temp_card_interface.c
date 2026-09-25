// Ruhan Louw
// temp sensor card interface
// 2025 V1.0


#include "temp_card_interface.h"
#include "../../mcc_generated_files/uart/usart1.h"
#include <stdint.h>
#include <avr/io.h>
//#include "uart_protocol.h" // Protocol definitions
//#include "mcc_generated_files/mcc.h" // MCC-generated UART drivers

// Buffer and flags for receiving responses
static volatile uint8_t rxBuffer[MAX_MESSAGE_LEN]; // Buffer for incoming response
static volatile uint8_t rxIndex = 0; // Current buffer index
static volatile bool responseReceived = false; // Flag for complete response

// Structure to store temperature readings
//typedef struct {
//    int16_t temperatures[SENSOR_COUNT]; // Array for 8 NTC + 1 K-type (°C × 10)
//    uint8_t count; // Number of temperatures received
//} SensorData;

// UART receive interrupt callback
void UART1_RxISR(void) {
    uint8_t rxByte = UART1_Read(); // Read byte from UART1
    if (rxIndex == 0 && rxByte != START_BYTE) {
        return; // Discard if not start byte
    }
    rxBuffer[rxIndex++] = rxByte; // Store byte
    if (rxIndex == 3) { // Length byte received
        uint8_t len = rxBuffer[2];
        if (len > MAX_PAYLOAD_LEN) {
            rxIndex = 0; // Reset on invalid length
            return;
        }
    }
    if (rxIndex >= 5 && rxBuffer[2] <= MAX_PAYLOAD_LEN) {
        uint8_t expectedLen = 5 + rxBuffer[2]; // Total message size
        if (rxIndex >= expectedLen && rxBuffer[expectedLen - 1] == END_BYTE) {
            responseReceived = true; // Flag complete response
            rxIndex = 0; // Reset for next
        }
    }
    if (rxIndex >= MAX_MESSAGE_LEN) {
        rxIndex = 0; // Prevent buffer overflow
        // TODO: Add error checking
        // TODO: Respond that error occured 
    } 
}

// Send command to sensor PCB
void sendCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    uint8_t buffer[MAX_MESSAGE_LEN];
    buffer[0] = START_BYTE; // 0xAA
    buffer[1] = cmd; // Command ID (e.g., 0x0C)
    buffer[2] = len; // Payload length
    for (uint8_t i = 0; i < len; i++) {
        buffer[3 + i] = payload[i]; // Copy payload
    }
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < 3 + len; i++) {
        checksum += buffer[i]; // Calculate checksum
    }
    buffer[3 + len] = checksum;
    buffer[4 + len] = END_BYTE; // 0xBB
    for (uint8_t i = 0; i < 5 + len; i++) {
        UART1_Write(buffer[i]); // Send via UART1
    }
}

// Send CMD_READ_SELECTED_SENSORS with 2-byte bitmask
void sendSelectedSensorsCommand(uint16_t sensorBitmask) {
    uint8_t payload[2];
    payload[0] = sensorBitmask >> 8; // High byte
    payload[1] = sensorBitmask & 0xFF; // Low byte
    sendCommand(CMD_READ_SELECTED_SENSORS, payload, 2);
}

// Process received response, return sensor data
bool processResponse(SensorData* data) {
    if (!responseReceived) {
        return false; // No response to process
    }
    cli(); // Disable interrupts
    responseReceived = false; // Clear flag
    
    // Extract response fields
    uint8_t cmd = rxBuffer[1]; // Command ID
    uint8_t len = rxBuffer[2]; // Payload length
    uint8_t checksum = rxBuffer[3 + len]; // Received checksum which should be checked to match
    
    // Calculate checksum
    uint8_t calcChecksum = 0;
    for (uint8_t i = 0; i < 3 + len; i++) {
        calcChecksum += rxBuffer[i];
    }
    
    // Verify checksum and end byte
    if (checksum != calcChecksum || rxBuffer[4 + len] != END_BYTE) {
        sei();
        return false; // Invalid response
    }
    
    // Handle CMD_READ_SELECTED_SENSORS response
    if (cmd == CMD_READ_SELECTED_SENSORS && len % 2 == 0) {
        data->count = len / 2; // Number of sensors (2 bytes each)
        // Extract temperatures (°C × 10)
        for (uint8_t i = 0; i < data->count; i++) {
            // Combine high and low bytes into int16_t
            data->temperatures[i] = ((int16_t)rxBuffer[3 + i*2] << 8) | rxBuffer[4 + i*2];
        }
        sei();
        return true; // Success
    } else if (cmd == RESP_ERROR) {
        // Handle error response (1-byte error code)
        uint8_t errorCode = rxBuffer[3];
        // TODO: Log or handle error (e.g., ERROR_INVALID_SENSOR)
        sei();
        return false;
    }
    sei();
    return false; // Unknown response
}





//// Example: Request and process temperatures
//void requestAndProcessTemperatures(uint16_t sensorBitmask) {
//    sendSelectedSensorsCommand(sensorBitmask); // Send command
//    // Wait for response (simple delay, replace with timeout)
////    _delay_ms(RESPONSE_TIMEOUT_MS);
//    
//    SensorData data = {0}; // Initialize data structure
//    if (processResponse(&data)) {
//        // Convert temperatures to float (°C) and process
//        for (uint8_t i = 0; i < data.count; i++) {
//            float tempC = data.temperatures[i] / 10.0; // Convert °C × 10 to °C
//            // TODO: Use tempC (e.g., display or log)
//        }
//    }
//}

//// Main function
//int main(void) {
//    SYSTEM_Initialize(); // Initialize MCC modules
//    UART1_SetRxISR(UART1_RxISR); // Set UART receive ISR
//    sei(); // Enable global interrupts
//    
//    while (1) {
//        // Example: Request NTC 1 and K-type
//        uint16_t sensorBitmask = SENSOR_NTC_1 | SENSOR_KTYPE; // 0x0101
//        requestAndProcessTemperatures(sensorBitmask);
//        _delay_ms(1000); // Wait before next request
//    }
//}