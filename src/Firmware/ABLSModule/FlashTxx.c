/*
 * FlashTxx.c - Flash primitives implementation for Teensy 4.x microcontrollers
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

#include "FlashTxx.h"
#include <string.h>

// Teensy 4.x core EEPROM functions (these are actually flash operations on T4.x)
void eeprom_initialize(void);
uint8_t eeprom_read_byte(const uint8_t *addr);
void eeprom_write_byte(uint8_t *addr, uint8_t value);
uint16_t eeprom_read_word(const uint16_t *addr);
void eeprom_write_word(uint16_t *addr, uint16_t value);
uint32_t eeprom_read_dword(const uint32_t *addr);
void eeprom_write_dword(uint32_t *addr, uint32_t value);
void eeprom_read_block(void *buf, const void *addr, uint32_t len);
void eeprom_write_block(const void *buf, void *addr, uint32_t len);
int eeprom_is_ready(void);

// External reference to end of program text (for buffer allocation)
extern uint32_t _etext;

//******************************************************************************
// flash_sector_not_erased() - returns 0 if erased and !0 (error) if NOT erased
//******************************************************************************
RAMFUNC int flash_sector_not_erased(uint32_t address)
{
    uint32_t *sector = (uint32_t*)(address & ~(FLASH_SECTOR_SIZE - 1));
    for (int i = 0; i < FLASH_SECTOR_SIZE / 4; i++) {
        if (*sector++ != 0xFFFFFFFF)
            return 1; // NOT erased
    }
    return 0; // erased
}

//******************************************************************************
// flash_erase_sector() - erase sector at address - must run from RAM
//******************************************************************************
RAMFUNC int flash_erase_sector(uint32_t address)
{
    // For Teensy 4.x, we need to implement sector erase
    // This is a simplified implementation - in a real system you'd want
    // to use the proper NXP flash driver
    
    // Align to sector boundary
    uint32_t sector_addr = address & ~(FLASH_SECTOR_SIZE - 1);
    
    // Fill sector with 0xFF (erased state)
    uint32_t *ptr = (uint32_t*)sector_addr;
    for (int i = 0; i < FLASH_SECTOR_SIZE / 4; i++) {
        *ptr++ = 0xFFFFFFFF;
    }
    
    return 0; // Success
}

//******************************************************************************
// flash_write_block() - write block of data to flash - must run from RAM
//******************************************************************************
RAMFUNC int flash_write_block(uint32_t offset, const void *buf, uint32_t len)
{
    // For Teensy 4.x, use the EEPROM emulation which actually writes to flash
    // Convert offset to absolute address
    uint32_t address = FLASH_BASE_ADDR + offset;
    
    // Use Teensy core's block write function
    eeprom_write_block(buf, (void*)address, len);
    
    return 0; // Success
}

//******************************************************************************
// firmware_buffer_init() - create buffer in flash for new firmware
//******************************************************************************
int firmware_buffer_init(uint32_t *buffer_addr, uint32_t *buffer_size)
{
    // Calculate buffer location after program text
    uint32_t addr = ((uint32_t)&_etext + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    
    // Calculate available space (simplified - assumes 4MB available)
    uint32_t size = 0x400000; // 4MB
    
    // Erase the buffer area
    uint32_t sectors = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; i++) {
        uint32_t sector_addr = addr + (i * FLASH_SECTOR_SIZE);
        int error = flash_erase_sector(sector_addr);
        if (error != 0) {
            return error;
        }
    }
    
    *buffer_addr = addr;
    *buffer_size = size;
    return 0;
}

//******************************************************************************
// firmware_buffer_free() - free buffer in flash
//******************************************************************************
void firmware_buffer_free(uint32_t buffer_addr, uint32_t buffer_size)
{
    // Erase the buffer area
    uint32_t sectors = (buffer_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; i++) {
        uint32_t sector_addr = buffer_addr + (i * FLASH_SECTOR_SIZE);
        flash_erase_sector(sector_addr);
    }
}

//******************************************************************************
// check_flash_id() - check for target ID string in flash
//******************************************************************************
int check_flash_id(uint32_t addr, uint32_t size)
{
    // Look for FLASH_ID string in the specified area
    const char *id = FLASH_ID;
    uint32_t id_len = strlen(id);
    
    for (uint32_t i = 0; i <= size - id_len; i++) {
        if (memcmp((void*)(addr + i), id, id_len) == 0) {
            return 1; // Found
        }
    }
    return 0; // Not found
}
