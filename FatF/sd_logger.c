// File: fs_logger.c
// Purpose: Logs temperature data to daily CSV files in SD card root
// Writes data in format: YYYY-MM-DD HH:MM:SS,sensor1,...,sensor9,error_code
// Uses Petit FatFs to create/append to YYYY_MM_DD.csv

#include <avr/io.h>     // AVR register definitions
#include <string.h>     // String functions (e.g., strlen)
#include <stdio.h>      // sprintf for formatting CSV
#include "pff.h"        // Petit FatFs header
#include "sd_logger.h"   // Logger header

// Number of temperature sensors
#define NUM_SENSORS 9

// Log temperature data to SD card
uint8_t log_data(Timestamp *ts, float *temps, uint8_t error_code) {
    FATFS fs;           // File system object for Petit FatFs
    FRESULT res;        // Result code for FatFs operations
    UINT bw;            // Number of bytes written
    char path[16];      // Buffer for file path (e.g., "2025_07_22.csv")
    char buffer[128];   // Buffer for CSV line (holds ~100 chars)

    // Mount the file system
    // Initializes SD card via disk_initialize if needed
    res = pf_mount(&fs);
    if (res != FR_OK) {
        // Return error if mount fails (e.g., no SD card)
        return res;
    }

    // Create file path for daily CSV (e.g., "2025_07_22.csv")
    // Stored in root directory
    snprintf(path, sizeof(path), "%04u_%02u_%02u.csv",
             ts->year, ts->month, ts->day);
    
    // Open or create the CSV file
    // pf_open creates file if it doesn't exist
    res = pf_open(path);
    if (res != FR_OK) {
        // Return error if file can't be opened/created
        return res;
    }

    // Move to end of file for appending
    // Ensures new data is added to the end of the CSV
    res = pf_lseek(fs.fsize);
    if (res != FR_OK) {
        // Return error if seek fails
        return res;
    }

    // Format CSV line
    // Example: "2025-07-22 18:42:00,25.5,24.8,25.2,26.0,25.7,24.9,25.1,25.3,25.6,0"
    snprintf(buffer, sizeof(buffer),
             "%04u-%02u-%02u %02u:%02u:%02u,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%u\n",
             ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second,
             temps[0], temps[1], temps[2], temps[3], temps[4],
             temps[5], temps[6], temps[7], temps[8], error_code);

    // Write CSV line to file
    // Uses disk_writep to send data to SD card sectors
    res = pf_write(buffer, strlen(buffer), &bw);
    if (res != FR_OK) {
        // Return error if write fails
        return res;
    }

    // Flush writes to SD card
    // Ensures data is physically written to prevent loss on power-off/unplug
    res = pf_write(0, 0, &bw);
    if (res != FR_OK) {
        // Return error if flush fails
        return res;
    }

    return res; // Return FR_OK (0) on success
}



//// File: main.c
//// Purpose: Main program to log temperature data every minute
//// Initializes SPI and calls fs_logger to write to YYYY_MM_DD.csv
//
//#include <avr/io.h>         // AVR register definitions
//#include <util/delay.h>     // Delay functions for timing
//#include "spi_mcc.h"        // MCC SPI wrapper
//#include "fs_logger.h"       // File system logger
//
//// Main function
//int main(void) {
//    // Initialize SPI module
//    // Sets up PB0 (MOSI), PB1 (MISO), PB2 (SCK), PB3 (CS) via MCC driver
//    spi_init();
//    
//    // Configure PC0 as output for error LED
//    PORTC.DIR |= PIN0_bm;
//    
//    // Example data (replace with DS18B20 readings and RTC)
//    Timestamp ts = {2025, 7, 22, 18, 42, 0}; // 2025-07-22 18:42:00
//    float temps[9] = {25.5, 24.8, 25.2, 26.0, 25.7, 24.9, 25.1, 25.3, 25.6}; // 9 sensor values
//    uint8_t error_code = 0; // Example error code (0 = no error)
//    
//    // Main loop
//    while (1) {
//        // Log data to YYYY_MM_DD.csv
//        uint8_t res = log_data(&ts, temps, error_code);
//        if (res != FR_OK) {
//            // Toggle LED on PC0 if logging fails (e.g., no SD card, write error)
//            PORTC.OUTTGL = PIN0_bm;
//        }
//        
//        // Wait 60 seconds before next log
//        // Matches your requirement to log every minute
//        _delay_ms(60000);
//    }
//}
