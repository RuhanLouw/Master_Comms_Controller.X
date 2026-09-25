/* 
 * File:   sd_logger.h
 * Author: Ruhan Louw
 *
 * Created on July 24, 2025, 3:57 PM
 */

#ifndef SD_LOGGER_H
#define	SD_LOGGER_H

#ifdef	__cplusplus
extern "C" {
#endif

//==
    
// File: fs_logger.h
// Purpose: Header for file system logger to write temperature data
// Defines timestamp structure and logging function

#include <stdint.h>     // Standard integer types
#include "pff.h"        // Petit FatFs header

// Structure to hold timestamp for CSV logging
typedef struct {
    uint16_t year;  // Year (e.g., 2025)
    uint8_t month;  // Month (1-12)
    uint8_t day;    // Day (1-31)
    uint8_t hour;   // Hour (0-23)
    uint8_t minute; // Minute (0-59)
    uint8_t second; // Second (0-59)
} Timestamp;

// Log temperature data to YYYY_MM_DD.csv
// Input: ts - timestamp
// Input: temps - array of 9 temperature values
// Input: error_code - error status
// Output: 0 (FR_OK) on success, non-zero (FRESULT) on error
uint8_t log_data(Timestamp *ts, float *temps, uint8_t error_code);

//==
    
#ifdef	__cplusplus
}
#endif

#endif	/* SD_LOGGER_H */

