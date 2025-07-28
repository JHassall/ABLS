/*
 * FlashTxx.h - Flash primitives for Teensy microcontrollers
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
 * Adapted for RgF ABLS project.
 */

#ifndef FLASHTXX_H
#define FLASHTXX_H

#include <Arduino.h>

// Platform-specific flash definitions for Teensy variants
#if defined(__MKL26Z64__)
  #define FLASH_ID		"fw_teensyLC"		// target ID (in code)
  #define FLASH_SIZE		(0x10000)		// 64KB program flash
  #define FLASH_SECTOR_SIZE	(0x400)			// 1KB sector size
  #define FLASH_WRITE_SIZE	(4)			// 4-byte/32-bit writes
  #define FLASH_RESERVE		(2*FLASH_SECTOR_SIZE)	// reserve top of flash
  #define FLASH_BASE_ADDR	(0)			// code starts here

#elif defined(__MK20DX128__)
  #define FLASH_ID		"fw_teensy30"		// target ID (in code)
  #define FLASH_SIZE		(0x20000)		// 128KB program flash
  #define FLASH_SECTOR_SIZE	(0x400)			// 1KB sector size
  #define FLASH_WRITE_SIZE	(4)			// 4-byte/32-bit writes
  #define FLASH_RESERVE		(0*FLASH_SECTOR_SIZE)	// reserve top of flash
  #define FLASH_BASE_ADDR	(0)			// code starts here

#elif defined(__MK20DX256__)
  #define FLASH_ID		"fw_teensy32"		// target ID (in code)
  #define FLASH_SIZE		(0x40000)		// 256KB program flash
  #define FLASH_SECTOR_SIZE	(0x800)			// 2KB sectors
  #define FLASH_WRITE_SIZE	(4)    			// 4-byte/32-bit writes
  #define FLASH_RESERVE 	(0*FLASH_SECTOR_SIZE)	// reserve top of flash
  #define FLASH_BASE_ADDR	(0)			// code starts here

#elif defined(__MK64FX512__)
  #define FLASH_ID		"fw_teensy35"		// target ID (in code)
  #define FLASH_SIZE		(0x80000)		// 512KB program flash
  #define FLASH_SECTOR_SIZE	(0x1000)		// 4KB sector size
  #define FLASH_WRITE_SIZE	(8)			// 8-byte/64-bit writes
  #define FLASH_RESERVE		(0*FLASH_SECTOR_SIZE)	// reserve to of flash
  #define FLASH_BASE_ADDR	(0)			// code starts here

#elif defined(__MK66FX1M0__)
  #define FLASH_ID		"fw_teensy36"		// target ID (in code)
  #define FLASH_SIZE		(0x100000)		// 1MB program flash
  #define FLASH_SECTOR_SIZE	(0x1000)		// 4KB sector size
  #define FLASH_WRITE_SIZE	(8)			// 8-byte/64-bit writes
  #define FLASH_RESERVE		(2*FLASH_SECTOR_SIZE)	// reserve top of flash
  #define FLASH_BASE_ADDR	(0)			// code starts here

#elif defined(__IMXRT1062__) && defined(ARDUINO_TEENSY40)
  #define FLASH_ID		"fw_teensy40"		// target ID (in code)
  #define FLASH_SIZE		(0x200000)		// 2MB program flash
  #define FLASH_SECTOR_SIZE	(0x1000)		// 4KB sector size
  #define FLASH_WRITE_SIZE	(4)			// 4-byte/32-bit writes
  #define FLASH_RESERVE		(4*FLASH_SECTOR_SIZE)	// reserve top of flash
  #define FLASH_BASE_ADDR	(0x60000000)		// code starts here

#elif defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  #define FLASH_ID		"fw_teensy41"		// target ID (in code)
  #define FLASH_SIZE		(0x800000)		// 8MB
  #define FLASH_SECTOR_SIZE	(0x1000)		// 4KB sector size
  #define FLASH_WRITE_SIZE	(4)			// 4-byte/32-bit writes    
  #define FLASH_RESERVE		(4*FLASH_SECTOR_SIZE)	// reserve top of flash 
  #define FLASH_BASE_ADDR	(0x60000000)		// code starts here

#elif defined(__IMXRT1062__) && defined(ARDUINO_TEENSY_MICROMOD)
  #define FLASH_ID		"fw_teensyMM"		// target ID (in code)
  #define FLASH_SIZE		(0x1000000)		// 16MB
  #define FLASH_SECTOR_SIZE	(0x1000)		// 4KB sector size
  #define FLASH_WRITE_SIZE	(4)			// 4-byte/32-bit writes    
  #define FLASH_RESERVE		(4*FLASH_SECTOR_SIZE)	// reserve top of flash 
  #define FLASH_BASE_ADDR	(0x60000000)		// code starts here

#else
  #error MCU NOT SUPPORTED
#endif

#if defined(FLASH_ID)
  #define RAM_BUFFER_SIZE	(0 * 1024)
  #define IN_FLASH(a) ((a) >= FLASH_BASE_ADDR && (a) < FLASH_BASE_ADDR+FLASH_SIZE)
#endif

// Reboot macro - same for all ARM devices
#define REBOOT (*(uint32_t *)0xE000ED0C = 0x5FA0004)

// RAM function attribute
#define RAMFUNC __attribute__ ((section(".ramfunc"), noinline, noclone, optimize("Os")))

// Buffer type definition
#define RAM_BUFFER_TYPE		(2)

// Function prototypes for flash operations
#ifdef __cplusplus
extern "C" {
#endif

// Core flash functions
int firmware_buffer_init(uint32_t *buffer_addr, uint32_t *buffer_size);
void firmware_buffer_free(uint32_t buffer_addr, uint32_t buffer_size);
int flash_write_block(uint32_t offset, const void *buf, uint32_t len);
int check_flash_id(uint32_t addr, uint32_t size);

#if defined(KINETISK) || defined(KINETISL)
// T3.x flash primitives (must be in RAM)
RAMFUNC int flash_word(uint32_t address, uint32_t value, int aFSEC, int oFSEC);
RAMFUNC int flash_phrase(uint32_t address, uint64_t value, int aFSEC, int oFSEC);
RAMFUNC int flash_erase_sector(uint32_t address, int aFSEC);
RAMFUNC int flash_sector_not_erased(uint32_t address);

#if defined(__MK66FX1M0__)
// Cache control functions for T3.6 only
void LMEM_EnableCodeCache(bool enable);
void LMEM_CodeCacheInvalidateAll(void);
void LMEM_CodeCachePushAll(void);
void LMEM_CodeCacheClearAll(void);
#endif

#elif defined(__IMXRT1062__)
// T4.x flash primitives
RAMFUNC int flash_erase_sector(uint32_t address);
RAMFUNC int flash_sector_not_erased(uint32_t address);
RAMFUNC int flash_write_block(uint32_t offset, const void *buf, uint32_t len);

#endif

#ifdef __cplusplus
}
#endif

#endif // FLASHTXX_H
