#ifndef FLASH_BACKUP_MANAGER_H
#define FLASH_BACKUP_MANAGER_H

// =================================================================
// ABLS (Automatic Boom Level System)
// Flash Backup Manager - Dual-Bank Firmware Backup/Rollback System
// 
// Provides dual-bank flash memory management for safe OTA firmware
// updates with fast rollback capability on Teensy 4.1 modules.
//
// Author: RgF Engineering
// Based on: OTA Firmware Update System Plan requirements
// Credit: Utilizes FlasherX flash primitives by Joe Pasquariello
// =================================================================

#include <Arduino.h>
#include "VersionManager.h"
#include "DiagnosticManager.h"

extern "C" {
    #include "FlasherX/FlashTxx.h"
}

// Flash memory layout for Teensy 4.1 (8MB total flash)
// Bank A: Current firmware (0x60000000 - 0x603FFFFF) - 4MB
// Bank B: Backup firmware  (0x60400000 - 0x607FFFFF) - 4MB
#define CURRENT_FIRMWARE_BASE   0x60000000UL    // Bank A: Current firmware
#define BACKUP_FIRMWARE_BASE    0x60400000UL    // Bank B: Backup firmware  
#define FIRMWARE_MAX_SIZE       0x400000UL      // 4MB per bank
// FLASH_SECTOR_SIZE is already defined in FlashTxx.h

// Backup operation result codes
typedef enum {
    BACKUP_SUCCESS = 0,           // Operation completed successfully
    BACKUP_ERROR_INVALID_SIZE,    // Firmware size exceeds bank capacity
    BACKUP_ERROR_READ_FAILED,     // Failed to read current firmware
    BACKUP_ERROR_WRITE_FAILED,    // Failed to write backup firmware
    BACKUP_ERROR_VERIFY_FAILED,   // Backup verification failed
    BACKUP_ERROR_ERASE_FAILED,    // Failed to erase backup bank
    BACKUP_ERROR_NO_BACKUP,       // No valid backup available
    BACKUP_ERROR_CORRUPTED,       // Backup firmware is corrupted
    BACKUP_ERROR_VERSION_MISMATCH, // Backup version doesn't match expected
    BACKUP_ERROR_FLASH_BUSY,      // Flash memory is busy
    BACKUP_ERROR_UNKNOWN          // Unknown error occurred
} BackupResult_t;

// Backup status information
struct BackupStatus {
    bool hasValidBackup;              // True if backup bank contains valid firmware
    FirmwareVersion_t backupVersion;  // Version of firmware in backup bank
    uint32_t backupSize;              // Size of backup firmware in bytes
    uint32_t backupChecksum;          // CRC32 checksum of backup firmware
    uint32_t backupTimestamp;         // Timestamp when backup was created
    BackupResult_t lastOperation;     // Result of last backup operation
    String lastError;                 // Description of last error
};

class FlashBackupManager {
public:
    // Initialization and status
    static void init();
    static bool isInitialized() { return _initialized; }
    
    // Primary backup operations
    static BackupResult_t backupCurrentFirmware();
    static BackupResult_t restoreFromBackup();
    static BackupResult_t validateBackup();
    
    // Backup information and status
    static bool hasValidBackup();
    static FirmwareVersion_t getBackupVersion();
    static BackupStatus getBackupStatus();
    static String getBackupStatusString();
    
    // Advanced backup operations
    static BackupResult_t createBackupFromBuffer(const uint8_t* firmwareData, uint32_t size, 
                                                const FirmwareVersion_t& version);
    static BackupResult_t eraseBackupBank();
    static BackupResult_t verifyBackupIntegrity();
    
    // Rollback preparation and execution
    static bool canRollback();
    static BackupResult_t prepareRollback();
    static BackupResult_t executeRollback();
    
    // Configuration and thresholds
    static void setVerificationEnabled(bool enabled);
    static void setProgressCallback(void (*callback)(uint8_t progress));
    
    // Diagnostics and logging
    static void logBackupEvent(const String& event);
    static uint32_t getBackupBankFreeSpace();
    static bool isBackupBankEmpty();
    
private:
    // Initialization state
    static bool _initialized;
    static bool _verificationEnabled;
    static void (*_progressCallback)(uint8_t progress);
    
    // Backup state tracking
    static BackupStatus _backupStatus;
    static bool _backupStatusValid;
    static uint32_t _lastStatusUpdate;
    
    // Flash operation helpers
    static BackupResult_t readFirmwareFromBank(uint32_t bankAddress, uint8_t* buffer, 
                                             uint32_t size, uint32_t offset = 0);
    static BackupResult_t writeFirmwareToBank(uint32_t bankAddress, const uint8_t* buffer, 
                                            uint32_t size, uint32_t offset = 0);
    static BackupResult_t eraseFirmwareBank(uint32_t bankAddress, uint32_t size);
    
    // Verification and integrity checking
    static uint32_t calculateFirmwareChecksum(uint32_t bankAddress, uint32_t size);
    static bool verifyFirmwareIntegrity(uint32_t bankAddress, uint32_t size, uint32_t expectedChecksum);
    static BackupResult_t compareFirmwareBanks(uint32_t bank1Address, uint32_t bank2Address, uint32_t size);
    
    // Version management
    static bool extractVersionFromFirmware(uint32_t bankAddress, FirmwareVersion_t* version);
    static BackupResult_t validateFirmwareVersion(const FirmwareVersion_t& version);
    
    // Progress reporting
    static void reportProgress(uint8_t progress);
    static void updateBackupStatus();
    
    // Error handling and logging
    static void setLastError(BackupResult_t result, const String& error);
    static void logBackupOperation(const String& operation, BackupResult_t result);
    
    // Flash memory utilities
    static bool isValidFlashAddress(uint32_t address);
    static bool isAddressInCurrentBank(uint32_t address);
    static bool isAddressInBackupBank(uint32_t address);
    static uint32_t alignToSectorBoundary(uint32_t address);
    static uint32_t calculateSectorsNeeded(uint32_t size);
    
    // Safety checks
    static bool isFlashOperationSafe();
    static BackupResult_t performSafetyChecks();
};

// Helper functions for result code conversion
const char* backupResultToString(BackupResult_t result);
bool isBackupResultSuccess(BackupResult_t result);
bool isBackupResultError(BackupResult_t result);

#endif // FLASH_BACKUP_MANAGER_H
