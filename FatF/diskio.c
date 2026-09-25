/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include "diskio.h"

// File: diskio.c
// Purpose: Implements low-level SD card operations for Petit FatFs
// Uses MCC SPI driver (PB0-PB3) to send SD card commands and read/write data
// Handles initialization, reading, and writing sectors
#define F_CPU 16000000UL
#include <avr/io.h>         // AVR register definitions
#include <util/delay.h>     // Delay functions for timing
#include "pff.h"            // Petit FatFs header
#include "spiSD.h"       // MCC SPI wrapper
#include "diskio.h"

// File: diskio.c
// SD card commands (6-byte packets sent over SPI)
#define CMD0    (0x40 + 0)   // GO_IDLE_STATE: Resets SD card to idle state
#define CMD1    (0x40 + 1)   // SEND_OP_COND: Initializes MMC cards
#define CMD8    (0x40 + 8)   // SEND_IF_COND: Checks SD card version and voltage
#define ACMD41  (0x40 + 41)  // SEND_OP_COND: Initializes SD cards
#define CMD17   (0x40 + 17)  // READ_SINGLE_BLOCK: Reads a 512-byte sector
#define CMD24   (0x40 + 24)  // WRITE_BLOCK: Writes a 512-byte sector

// Card type flags
#define CT_MMC   0x01        // MMC (MultiMediaCard, older type)
#define CT_SD1   0x02        // SDv1 (Standard Capacity, SDSC)
#define CT_SD2   0x04        // SDv2 (Standard Capacity, SDSC)
#define CT_BLOCK 0x08        // Block addressing (SDHC/SDXC)

// Disk status flags
#define STA_NOINIT 0x01      // Disk not initialized
#define STA_NODISK 0x02      // No disk present (optional)
#define STA_PROTECT 0x04     // Write protection (optional)

// SD card status variable
static DSTATUS _disk_status = STA_NOINIT; // Initially not initialized

// Get disk status
// Output: Current status (STA_NOINIT if not initialized)
DSTATUS disk_status(void) {
    return _disk_status;
}

// Send an SD card command and get response
// Input: cmd - command byte (e.g., CMD0 = 0x40)
// Input: arg - 32-bit argument (e.g., sector address or 0x1AA for CMD8)
// Output: R1 response (0x01 = idle, 0x00 = ready)
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t res, n; // res = response, n = loop counter
    
    // Deselect SD card (PB3 high) and send 10 dummy clocks
    // Ensures SD card is ready for a new command
    spi_cs_deselect();
    for (n = 0; n < 10; n++) spi_transfer(0xFF);
    
    // Select SD card (PB3 low)
    spi_cs_select();
    
    // Send 6-byte command packet
    spi_transfer(cmd);              // Send command byte
    spi_transfer((uint8_t)(arg >> 24)); // Argument bits 31-24
    spi_transfer((uint8_t)(arg >> 16)); // Argument bits 23-16
    spi_transfer((uint8_t)(arg >> 8));  // Argument bits 15-8
    spi_transfer((uint8_t)arg);         // Argument bits 7-0
    // Send CRC (only required for CMD0 and CMD8 in SPI mode)
    spi_transfer(cmd == CMD0 ? 0x95 : cmd == CMD8 ? 0x87 : 0xFF);
    
    // Wait for R1 response (up to 10 bytes)
    // R1 has MSB=0 when valid (e.g., 0x01 = idle)
    for (n = 0; n < 10; n++) {
        res = spi_transfer(0xFF);
        if (!(res & 0x80)) break;
    }
    
    return res;
}


// Initialize SD card
// Output: Status after initialization attempt
DSTATUS disk_initialize(void) {
    uint8_t n, cmd, ty, ocr[4]; // n = counter, cmd = command, ty = card type, ocr = response
    
    // Skip if already initialized
    if (!(_disk_status & STA_NOINIT)) return _disk_status;
    
    // Send 80 dummy clocks with CS high to prepare SD card
    spi_cs_deselect();
    for (n = 0; n < 10; n++) spi_transfer(0xFF);
    
    // Send CMD0 to enter idle state
    if (send_cmd(CMD0, 0) == 1) {
        // Check SDv2 with CMD8 (0x1AA = 2.7-3.6V, check pattern)
        if (send_cmd(CMD8, 0x1AA) == 1) {
            // Read 4-byte R7 response for CMD8
            for (n = 0; n < 4; n++) ocr[n] = spi_transfer(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) { // Valid SDv2 response
                // Initialize SDv2 with ACMD41 (HCS bit for SDHC)
                for (n = 0; n < 1000; n++) {
                    if (send_cmd(ACMD41, 1UL << 30) == 0) break;
                    _delay_ms(1); // Wait for card to initialize
                }
                if (n < 1000) {
                    _disk_status &= ~STA_NOINIT; // Mark as initialized
                    spi_set_high_speed();       // Switch to ~833 kHz
                }
            }
        } else { // SDv1 or MMC
            // Try SDv1 with ACMD41
            if (send_cmd(ACMD41, 0) <= 1) {
                ty = CT_SD1; cmd = ACMD41;
            } else { // Try MMC with CMD1
                ty = CT_MMC; cmd = CMD1;
            }
            for (n = 0; n < 1000; n++) {
                if (send_cmd(cmd, 0) == 0) break;
                _delay_ms(1);
            }
            if (n < 1000) {
                _disk_status &= ~STA_NOINIT;
                spi_set_high_speed();
            }
        }
    }
    
    // Deselect SD card
    spi_cs_deselect();
    return _disk_status;
}

// Read partial data from a sector (optional, for file reads)
DRESULT disk_readp(BYTE *buff, DWORD sector, UINT offset, UINT count) {
    DRESULT res = RES_ERROR; // Result code
    uint8_t r1;              // Response byte
    
    // Check if disk is initialized
    if (_disk_status & STA_NOINIT) return RES_NOTRDY;
    
    // Convert sector to byte address for SDSC cards
    if (!(_disk_status & CT_BLOCK)) sector *= 512;
    
    // Send CMD17 to read a 512-byte sector
    if ((r1 = send_cmd(CMD17, sector)) == 0) {
        // Wait for data token (0xFE)
        uint16_t timeout = 1000;
        while (timeout--) {
            r1 = spi_transfer(0xFF);
            if (r1 == 0xFE) break;
            _delay_ms(1);
        }
        if (r1 == 0xFE) {
            // Skip offset bytes
            while (offset--) spi_transfer(0xFF);
            // Read requested bytes
            while (count--) *buff++ = spi_transfer(0xFF);
            // Skip remaining bytes
            for (uint16_t i = 0; i < 512 - offset - count; i++) spi_transfer(0xFF);
            spi_transfer(0xFF); // CRC byte 1
            spi_transfer(0xFF); // CRC byte 2
            res = RES_OK;
        }
    }
    
    // Deselect SD card
    spi_cs_deselect();
    return res;
}

// Write data to a sector (used for file writes)
DRESULT disk_writep(const BYTE *buff, DWORD sector) {
    DRESULT res = RES_ERROR; // Result code
    uint8_t r1;              // Response byte
    UINT wc;                 // Write counter
    
    // Check if disk is initialized
    if (_disk_status & STA_NOINIT) return RES_NOTRDY;
    
    // Convert sector to byte address for SDSC
    if (!(_disk_status & CT_BLOCK)) sector *= 512;
    
    // Send CMD24 to write a 512-byte sector
    if ((r1 = send_cmd(CMD24, sector)) == 0) {
        spi_transfer(0xFE); // Send data token
        // Write 512 bytes (data or 0xFF padding)
        for (wc = 0; wc < 512; wc++) {
            spi_transfer(buff ? *buff++ : 0xFF);
        }
        spi_transfer(0xFF); // CRC byte 1
        spi_transfer(0xFF); // CRC byte 2
        r1 = spi_transfer(0xFF); // Get data response
        if ((r1 & 0x1F) == 0x05) { // Data accepted
            // Wait for write to complete
            uint16_t timeout = 1000;
            while (timeout-- && spi_transfer(0xFF) == 0);
            if (timeout) res = RES_OK;
        }
    }
    
    // Deselect SD card
    spi_cs_deselect();
    return res;
}

// Timer function (not needed for periodic logging)
void disk_timerproc(void) {
    // Empty stub, as timeouts are handled elsewhere
}

///*-----------------------------------------------------------------------*/
///* Initialize Disk Drive                                                 */
///*-----------------------------------------------------------------------*/
//
//DSTATUS disk_initialize (void)
//{
//	DSTATUS stat;
//
//	// Put your code here
//
//	return stat;
//}
//
//
//
///*-----------------------------------------------------------------------*/
///* Read Partial Sector                                                   */
///*-----------------------------------------------------------------------*/
//
//DRESULT disk_readp (
//	BYTE* buff,		/* Pointer to the destination object */
//	DWORD sector,	/* Sector number (LBA) */
//	UINT offset,	/* Offset in the sector */
//	UINT count		/* Byte count (bit15:destination) */
//)
//{
//	DRESULT res;
//
//	
//
//	return res;
//}
//
//
//
///*-----------------------------------------------------------------------*/
///* Write Partial Sector                                                  */
///*-----------------------------------------------------------------------*/
//
//DRESULT disk_writep (
//	BYTE* buff,		/* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
//	DWORD sc		/* Sector number (LBA) or Number of bytes to send */
//)
//{
//	DRESULT res;
//
//
//	if (!buff) {
//		if (sc) {
//
//			// Initiate write process
//
//		} else {
//
//			// Finalize write process
//
//		}
//	} else {
//
//		// Send data to the disk
//
//	}
//
//	return res;
//}
//
