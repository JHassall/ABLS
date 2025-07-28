/*
 * FlashTxx.c - Flash primitives implementation for Teensy microcontrollers
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

#include "FlashTxx.h"		// FLASH_BASE_ADDRESS, FLASH_SECTOR_SIZE, etc.
#include <string.h>
#include <Arduino.h>

// For Teensy 4.x, we need to include the core EEPROM functions
#if defined(__IMXRT1062__)
extern "C" {
    int eeprom_erase_sector(uint32_t addr);
    int eeprom_write_block(const void *buf, void *addr, uint32_t len);
}
extern uint32_t _etext;
#endif

#define FTFL_READ_MARGIN_FACTORY	(0x02)

// Missing flash command definitions for older Teensy variants
#if defined(KINETISK) || defined(KINETISL)
#define FCMD_PROGRAM_LONGWORD     0x06
#define FCMD_PROGRAM_PHRASE       0x07
#define FCMD_ERASE_FLASH_SECTOR   0x09
#define FCMD_READ_1S_SECTION      0x40
#define FTFL_READ_MARGIN_NORMAL   0x00

// Flash command helper functions for older Teensy variants
static void flash_init_command(uint8_t cmd, uint32_t addr) {
    // Implementation would go here for T3.x support
    // For now, focusing on T4.x support
}

static void flash_exec(void) {
    // Implementation would go here for T3.x support
    // For now, focusing on T4.x support
}

// Flash status register definitions
#define FTFL_FSTAT_ACCERR   0x20
#define FTFL_FSTAT_FPVIOL   0x10
#define FTFL_FSTAT_MGSTAT0  0x01

// Flash command registers (placeholders for T3.x)
static volatile uint8_t FTFL_FCCOB4, FTFL_FCCOB5, FTFL_FCCOB6, FTFL_FCCOB7;
static volatile uint8_t FTFL_FCCOB8, FTFL_FCCOB9, FTFL_FCCOBA, FTFL_FCCOBB;
static volatile uint8_t FTFL_FSTAT;

// Flash alignment macro
#define FLASH_ALIGN(addr, size) ((addr) & ~((size) - 1))

#endif

#if (FLASH_WRITE_SIZE==4) // TLC, T30, T31, T32
//******************************************************************************
// flash_word()		write 4-byte word to flash - must run from ram
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
// oFSEC = overwrite FSEC value   (set BOTH  = true to write to FSEC address)
RAMFUNC int flash_word( uint32_t address, uint32_t value, int aFSEC, int oFSEC )
{
  FLASH_ALIGN( address, FLASH_WRITE_SIZE );

  if (address == (0x408 & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;
  if (address == 0x408)
    if (oFSEC == 0)
      return 0;

  flash_init_command( FCMD_PROGRAM_LONGWORD, address );

  FTFL_FCCOB4 = value >> 24;
  FTFL_FCCOB5 = value >> 16;
  FTFL_FCCOB6 = value >> 8;
  FTFL_FCCOB7 = value >> 0;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#elif (FLASH_WRITE_SIZE==8) // T35, T36
//******************************************************************************
// flash_phrase()	write 8-byte phrase to flash - must run from ram
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
// oFSEC = overwrite FSEC value   (set BOTH  = true to write to FSEC address)
RAMFUNC int flash_phrase( uint32_t address, uint64_t value, int aFSEC, int oFSEC )
{
  FLASH_ALIGN( address, FLASH_WRITE_SIZE );

  if (address == (0x408 & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;
  if (address == 0x408)
    if (oFSEC == 0)
      return 0;

  flash_init_command( FCMD_PROGRAM_PHRASE, address );

  FTFL_FCCOB4 = value >> 24;
  FTFL_FCCOB5 = value >> 16;
  FTFL_FCCOB6 = value >> 8;
  FTFL_FCCOB7 = value >> 0;

  FTFL_FCCOB8 = value >> 56;
  FTFL_FCCOB9 = value >> 48;
  FTFL_FCCOBA = value >> 40;
  FTFL_FCCOBB = value >> 32;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#endif // FLASH_WRITE_SIZE

//******************************************************************************
// flash_erase_sector()		erase sector at address - must run from RAM
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
RAMFUNC int flash_erase_sector( uint32_t address, int aFSEC )
{
  FLASH_ALIGN( address, FLASH_SECTOR_SIZE );

  if (address == (0x400 & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;

  flash_init_command( FCMD_ERASE_FLASH_SECTOR, address );

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

//******************************************************************************
// flash_sector_not_erased()	returns 0 if erased and !0 (error) if NOT erased
//******************************************************************************
RAMFUNC int flash_sector_not_erased( uint32_t address )
{
  uint16_t num = (FLASH_SECTOR_SIZE / FLASH_WRITE_SIZE);
  FLASH_ALIGN( address, FLASH_SECTOR_SIZE );
  flash_init_command( FCMD_READ_1S_SECTION, address );

  FTFL_FCCOB4 = num >> 8;
  FTFL_FCCOB5 = num >> 0;
  FTFL_FCCOB6 = FTFL_READ_MARGIN_NORMAL;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#elif defined(__IMXRT1062__) // T4.x

//******************************************************************************
// flash_sector_not_erased()	returns 0 if erased and !0 (error) if NOT erased
//******************************************************************************
RAMFUNC int flash_sector_not_erased( uint32_t address )
{
  uint32_t *sector = (uint32_t*)(address & ~(FLASH_SECTOR_SIZE - 1));
  for (int i=0; i<FLASH_SECTOR_SIZE/4; i++) {
    if (*sector++ != 0xFFFFFFFF)
      return 1; // NOT erased
  }
  return 0; // erased
}

//******************************************************************************
// flash_erase_sector()		erase sector at address - must run from RAM
//******************************************************************************
RAMFUNC int flash_erase_sector( uint32_t address )
{
  // Use Teensy 4.x core erase function
  return eeprom_erase_sector(address);
}

//******************************************************************************
// flash_write_block()		write block of data to flash - must run from RAM
//******************************************************************************
RAMFUNC int flash_write_block( uint32_t offset, const void *buf, uint32_t len )
{
  // Use Teensy 4.x core write function
  return eeprom_write_block(buf, (void*)offset, len);
}

#endif // __IMXRT1062__

//******************************************************************************
// firmware_buffer_init()	create buffer in flash for new firmware
//******************************************************************************
int firmware_buffer_init( uint32_t *buffer_addr, uint32_t *buffer_size )
{
  uint32_t buffer_sectors;
  uint32_t code_size = (uint32_t)&_etext - FLASH_BASE_ADDR;

  // round up to next sector boundary
  code_size = (code_size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);

  // buffer is in flash after code
  *buffer_addr = FLASH_BASE_ADDR + code_size;
  *buffer_size = FLASH_SIZE - FLASH_RESERVE - code_size;

  // buffer must be multiple of sector size
  buffer_sectors = *buffer_size / FLASH_SECTOR_SIZE;
  *buffer_size = buffer_sectors * FLASH_SECTOR_SIZE;

  // erase buffer sectors
  for (uint32_t i=0; i<buffer_sectors; i++) {
    uint32_t sector_addr = *buffer_addr + i * FLASH_SECTOR_SIZE;
    
    #if defined(__IMXRT1062__)
    int error = flash_erase_sector(sector_addr);
    #else
    int error = flash_erase_sector(sector_addr, 1);
    #endif
    
    if (error) {
      return error;
    }
  }

  return 0; // success
}

//******************************************************************************
// firmware_buffer_free()	free buffer in flash
//******************************************************************************
void firmware_buffer_free( uint32_t buffer_addr, uint32_t buffer_size )
{
  uint32_t buffer_sectors = buffer_size / FLASH_SECTOR_SIZE;

  // erase buffer sectors to free them
  for (uint32_t i=0; i<buffer_sectors; i++) {
    uint32_t sector_addr = buffer_addr + i * FLASH_SECTOR_SIZE;
    
    #if defined(__IMXRT1062__)
    flash_erase_sector(sector_addr);
    #else
    flash_erase_sector(sector_addr, 1);
    #endif
  }
}

//******************************************************************************
// check_flash_id()		check for target ID string in flash
//******************************************************************************
int check_flash_id( uint32_t addr, uint32_t size )
{
  uint32_t *ptr = (uint32_t *)addr;
  uint32_t *end = (uint32_t *)(addr + size);
  
  while (ptr < end) {
    if (memcmp(ptr, FLASH_ID, strlen(FLASH_ID)) == 0) {
      return 1; // found
    }
    ptr++;
  }
  
  return 0; // not found
}
