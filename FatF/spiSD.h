/* 
 * File:   spiSD.h
 * Author: Ruhan Louw
 *
 * Created on July 24, 2025, 1:57 PM
 */

#ifndef SPISD_H
#define	SPISD_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "spiSD.h"

// Initialize SPI module using MCC Melody driver
//void spi_init(void);

// Transmit/receive a byte over SPI (uses PB0: MOSI, PB1: MISO, PB2: SCK)
uint8_t spi_transfer(uint8_t data);

// Select SD card (sets PB3: CS low)
void spi_cs_select(void);

// Deselect SD card (sets PB3: CS high)
void spi_cs_deselect(void);

// Switch to high-speed SPI after SD card initialization
void spi_set_high_speed(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SPISD_H */

