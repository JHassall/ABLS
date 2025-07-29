/*
 * FlashTxx.h - Flash primitives for Teensy 4.x microcontrollers
 * 
 * Based on FlasherX by Joe Pasquariello
 * Original repository: https://github.com/joepasquariello/FlasherX
 * 
 * FlasherX provides over-the-air firmware updates for Teensy microcontrollers
 * and has been adapted for use in the ABLS RgFModuleUpdater system.
 * 
 * Original FlasherX code is released into the public domain.
 * Original author: Joe Pasquariello
 * 
 * Adapted for RgF ABLS project - Teensy 4.x only.
 */

#ifndef FLASHTXX_H
#define FLASHTXX_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Teensy 4.x flash memory layout
#define FLASH_BASE_ADDR         0x60000000
#define FLASH_SECTOR_SIZE       4096        // 4KB sectors on Teensy 4.x
#define FLASH_WRITE_SIZE        256         // Write block size
#define FLASH_ID                "T4X"

// RAMFUNC attribute for functions that must run from RAM
#define RAMFUNC __attribute__ ((section(".ramfunc"), noinline, noclone, optimize("Os")))

// Flash function declarations for Teensy 4.x only
RAMFUNC int flash_erase_sector(uint32_t address);
RAMFUNC int flash_sector_not_erased(uint32_t address);
RAMFUNC int flash_write_block(uint32_t offset, const void *buf, uint32_t len);

// Firmware buffer management
int firmware_buffer_init(uint32_t *buffer_addr, uint32_t *buffer_size);
void firmware_buffer_free(uint32_t buffer_addr, uint32_t buffer_size);
int check_flash_id(uint32_t addr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // FLASHTXX_H
