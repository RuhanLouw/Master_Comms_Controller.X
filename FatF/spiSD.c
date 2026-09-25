#define F_CPU 16000000UL
#include <util/delay.h>
#include "../mcc_generated_files/spi/spi0.h"
#include "spiSD.h"
#include "../mcc_generated_files/system/pins.h"
#include "../mcc_generated_files/system/clock.h"
// Initialize SPI using MCC Melody driver
//void spi_init(void) {
//    SPI0_Initialize(); // Sets up SPI0 for PB0 (MOSI), PB1 (MISO), PB2 (SCK), PB3 (CS)
//}

// Send/receive a byte over SPI
uint8_t spi_transfer(uint8_t data) {
    return SPI0_ByteExchange(data); // Uses PB0-PB2 for data transfer
}

// Select SD card (set PB3 low)
void spi_cs_select(void) {
    IO_PA1_SetLow(); // ~CS for uSD card
    _delay_us(10); // Small delay for SD card stability
}

// Deselect SD card (set PB3 high)
void spi_cs_deselect(void) {
    IO_PA1_SetHigh(); // ~CS for uSD card
    _delay_us(10); // Small delay for SD card stability
}

// Switch to high-speed SPI (~833 kHz)
void spi_set_high_speed(void) {
    SPI0.CTRLA = (SPI0.CTRLA & ~SPI_PRESC_gm) | SPI_PRESC_DIV16_gc | SPI_ENABLE_bm;// Set clock to F_CPU/4
}
