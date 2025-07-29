#include "FlashBackupManager.h"
#include "FlashTxx.h"

// =================================================================
// ABLS (Automatic Boom Level System)
// Flash Backup Manager - Dual-Bank Firmware Backup/Rollback Implementation
// 
// Provides dual-bank flash memory management for safe OTA firmware
// updates with fast rollback capability on Teensy 4.1 modules.
//
// Author: RgF Engineering
// Based on: OTA Firmware Update System Plan requirements
// Credit: Utilizes FlasherX flash primitives by Joe Pasquariello
// =================================================================

// Static member initialization
bool FlashBackupManager::_initialized = false;
bool FlashBackupManager::_verificationEnabled = true;
void (*FlashBackupManager::_progressCallback)(uint8_t progress) = nullptr;

BackupStatus FlashBackupManager::_backupStatus = {};
bool FlashBackupManager::_backupStatusValid = false;
uint32_t FlashBackupManager::_lastStatusUpdate = 0;

void FlashBackupManager::init() {
    if (_initialized) {
        return;
    }
    
    // Flash primitives are ready (no init needed for T4.x)
    
    // Initialize backup status
    _backupStatus = {};
    _backupStatus.hasValidBackup = false;
    _backupStatus.lastOperation = BACKUP_SUCCESS;
    _backupStatus.lastError = "";
    
    // Update backup status by scanning backup bank
    updateBackupStatus();
    
    _initialized = true;
    _lastStatusUpdate = millis();
    
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Initialized dual-bank firmware backup system");
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Current bank: 0x" + String(CURRENT_FIRMWARE_BASE, HEX));
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Backup bank: 0x" + String(BACKUP_FIRMWARE_BASE, HEX));
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Bank size: " + String(FIRMWARE_MAX_SIZE / 1024) + " KB each");
    
    if (_backupStatus.hasValidBackup) {
        DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Valid backup found: " + 
                                    VersionManager::getVersionString(_backupStatus.backupVersion));
    } else {
        DiagnosticManager::logMessage(LOG_WARNING, "FlashBackupManager", "No valid backup found in backup bank");
    }
}

BackupResult_t FlashBackupManager::backupCurrentFirmware() {
    if (!_initialized) {
        init();
    }
    
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Starting backup of current firmware");
    logBackupEvent("BACKUP_STARTED");
    
    // Perform safety checks
    BackupResult_t safetyResult = performSafetyChecks();
    if (safetyResult != BACKUP_SUCCESS) {
        setLastError(safetyResult, "Safety checks failed");
        return safetyResult;
    }
    
    // Get current firmware version and size
    FirmwareVersion_t currentVersion = VersionManager::getCurrentVersion();
    uint32_t firmwareSize = FIRMWARE_MAX_SIZE; // For now, backup entire bank
    
    reportProgress(10);
    
    // Erase backup bank
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Erasing backup bank");
    BackupResult_t eraseResult = eraseFirmwareBank(BACKUP_FIRMWARE_BASE, firmwareSize);
    if (eraseResult != BACKUP_SUCCESS) {
        setLastError(eraseResult, "Failed to erase backup bank");
        return eraseResult;
    }
    
    reportProgress(30);
    
    // Copy current firmware to backup bank in chunks
    const uint32_t CHUNK_SIZE = 4096; // 4KB chunks
    uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        setLastError(BACKUP_ERROR_UNKNOWN, "Failed to allocate copy buffer");
        return BACKUP_ERROR_UNKNOWN;
    }
    
    uint32_t totalCopied = 0;
    BackupResult_t copyResult = BACKUP_SUCCESS;
    
    while (totalCopied < firmwareSize && copyResult == BACKUP_SUCCESS) {
        uint32_t chunkSize = min(CHUNK_SIZE, firmwareSize - totalCopied);
        
        // Read chunk from current bank
        copyResult = readFirmwareFromBank(CURRENT_FIRMWARE_BASE, buffer, chunkSize, totalCopied);
        if (copyResult != BACKUP_SUCCESS) {
            break;
        }
        
        // Write chunk to backup bank
        copyResult = writeFirmwareToBank(BACKUP_FIRMWARE_BASE, buffer, chunkSize, totalCopied);
        if (copyResult != BACKUP_SUCCESS) {
            break;
        }
        
        totalCopied += chunkSize;
        
        // Update progress (30% to 80% for copy operation)
        uint8_t progress = 30 + (50 * totalCopied / firmwareSize);
        reportProgress(progress);
    }
    
    free(buffer);
    
    if (copyResult != BACKUP_SUCCESS) {
        setLastError(copyResult, "Failed to copy firmware to backup bank");
        return copyResult;
    }
    
    reportProgress(85);
    
    // Verify backup if verification is enabled
    if (_verificationEnabled) {
        DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Verifying backup integrity");
        BackupResult_t verifyResult = verifyBackupIntegrity();
        if (verifyResult != BACKUP_SUCCESS) {
            setLastError(verifyResult, "Backup verification failed");
            return verifyResult;
        }
    }
    
    reportProgress(95);
    
    // Update backup status
    _backupStatus.hasValidBackup = true;
    _backupStatus.backupVersion = currentVersion;
    _backupStatus.backupSize = totalCopied;
    _backupStatus.backupChecksum = calculateFirmwareChecksum(BACKUP_FIRMWARE_BASE, totalCopied);
    _backupStatus.backupTimestamp = millis();
    _backupStatus.lastOperation = BACKUP_SUCCESS;
    _backupStatus.lastError = "";
    _backupStatusValid = true;
    
    reportProgress(100);
    
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Backup completed successfully");
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Backup version: " + 
                                VersionManager::getVersionString(currentVersion));
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Backup size: " + String(totalCopied) + " bytes");
    
    logBackupEvent("BACKUP_COMPLETED_SUCCESS");
    return BACKUP_SUCCESS;
}

BackupResult_t FlashBackupManager::restoreFromBackup() {
    if (!_initialized) {
        init();
    }
    
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Starting firmware restore from backup");
    logBackupEvent("RESTORE_STARTED");
    
    // Check if we have a valid backup
    if (!hasValidBackup()) {
        setLastError(BACKUP_ERROR_NO_BACKUP, "No valid backup available for restore");
        return BACKUP_ERROR_NO_BACKUP;
    }
    
    // Perform safety checks
    BackupResult_t safetyResult = performSafetyChecks();
    if (safetyResult != BACKUP_SUCCESS) {
        setLastError(safetyResult, "Safety checks failed for restore");
        return safetyResult;
    }
    
    reportProgress(10);
    
    // Validate backup before restore
    BackupResult_t validateResult = validateBackup();
    if (validateResult != BACKUP_SUCCESS) {
        setLastError(validateResult, "Backup validation failed");
        return validateResult;
    }
    
    reportProgress(20);
    
    uint32_t firmwareSize = _backupStatus.backupSize;
    
    // Erase current bank (this is the risky part!)
    DiagnosticManager::logMessage(LOG_WARNING, "FlashBackupManager", "Erasing current firmware bank for restore");
    BackupResult_t eraseResult = eraseFirmwareBank(CURRENT_FIRMWARE_BASE, firmwareSize);
    if (eraseResult != BACKUP_SUCCESS) {
        setLastError(eraseResult, "CRITICAL: Failed to erase current bank during restore");
        return eraseResult;
    }
    
    reportProgress(40);
    
    // Copy backup firmware to current bank
    const uint32_t CHUNK_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        setLastError(BACKUP_ERROR_UNKNOWN, "CRITICAL: Failed to allocate restore buffer");
        return BACKUP_ERROR_UNKNOWN;
    }
    
    uint32_t totalCopied = 0;
    BackupResult_t copyResult = BACKUP_SUCCESS;
    
    while (totalCopied < firmwareSize && copyResult == BACKUP_SUCCESS) {
        uint32_t chunkSize = min(CHUNK_SIZE, firmwareSize - totalCopied);
        
        // Read chunk from backup bank
        copyResult = readFirmwareFromBank(BACKUP_FIRMWARE_BASE, buffer, chunkSize, totalCopied);
        if (copyResult != BACKUP_SUCCESS) {
            break;
        }
        
        // Write chunk to current bank
        copyResult = writeFirmwareToBank(CURRENT_FIRMWARE_BASE, buffer, chunkSize, totalCopied);
        if (copyResult != BACKUP_SUCCESS) {
            break;
        }
        
        totalCopied += chunkSize;
        
        // Update progress (40% to 90% for copy operation)
        uint8_t progress = 40 + (50 * totalCopied / firmwareSize);
        reportProgress(progress);
    }
    
    free(buffer);
    
    if (copyResult != BACKUP_SUCCESS) {
        setLastError(copyResult, "CRITICAL: Failed to restore firmware from backup");
        // At this point, the system may be in an unrecoverable state
        return copyResult;
    }
    
    reportProgress(95);
    
    // Verify restored firmware
    if (_verificationEnabled) {
        DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Verifying restored firmware");
        uint32_t restoredChecksum = calculateFirmwareChecksum(CURRENT_FIRMWARE_BASE, firmwareSize);
        if (restoredChecksum != _backupStatus.backupChecksum) {
            setLastError(BACKUP_ERROR_VERIFY_FAILED, "CRITICAL: Restored firmware checksum mismatch");
            return BACKUP_ERROR_VERIFY_FAILED;
        }
    }
    
    reportProgress(100);
    
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Firmware restore completed successfully");
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "Restored version: " + 
                                VersionManager::getVersionString(_backupStatus.backupVersion));
    
    logBackupEvent("RESTORE_COMPLETED_SUCCESS");
    
    // System will need to reboot to run restored firmware
    DiagnosticManager::logMessage(LOG_WARNING, "FlashBackupManager", "System reboot required to run restored firmware");
    
    return BACKUP_SUCCESS;
}

BackupResult_t FlashBackupManager::validateBackup() {
    if (!_initialized) {
        init();
    }
    
    if (!_backupStatusValid) {
        updateBackupStatus();
    }
    
    if (!_backupStatus.hasValidBackup) {
        return BACKUP_ERROR_NO_BACKUP;
    }
    
    // Verify backup checksum
    uint32_t calculatedChecksum = calculateFirmwareChecksum(BACKUP_FIRMWARE_BASE, _backupStatus.backupSize);
    if (calculatedChecksum != _backupStatus.backupChecksum) {
        DiagnosticManager::logMessage(LOG_ERROR, "FlashBackupManager", "Backup checksum mismatch");
        return BACKUP_ERROR_CORRUPTED;
    }
    
    // Validate backup version
    BackupResult_t versionResult = validateFirmwareVersion(_backupStatus.backupVersion);
    if (versionResult != BACKUP_SUCCESS) {
        return versionResult;
    }
    
    return BACKUP_SUCCESS;
}

bool FlashBackupManager::hasValidBackup() {
    if (!_initialized) {
        init();
    }
    
    if (!_backupStatusValid) {
        updateBackupStatus();
    }
    
    return _backupStatus.hasValidBackup;
}

FirmwareVersion_t FlashBackupManager::getBackupVersion() {
    if (!_initialized) {
        init();
    }
    
    return _backupStatus.backupVersion;
}

BackupStatus FlashBackupManager::getBackupStatus() {
    if (!_initialized) {
        init();
    }
    
    if (!_backupStatusValid) {
        updateBackupStatus();
    }
    
    return _backupStatus;
}

String FlashBackupManager::getBackupStatusString() {
    BackupStatus status = getBackupStatus();
    
    String statusStr = "Backup Status: ";
    if (status.hasValidBackup) {
        statusStr += "VALID, Version: " + VersionManager::getVersionString(status.backupVersion);
        statusStr += ", Size: " + String(status.backupSize) + " bytes";
        statusStr += ", Created: " + String((millis() - status.backupTimestamp) / 1000) + "s ago";
    } else {
        statusStr += "NO_BACKUP";
    }
    
    if (status.lastOperation != BACKUP_SUCCESS) {
        statusStr += ", Last Error: " + String(backupResultToString(status.lastOperation));
    }
    
    return statusStr;
}

// Private helper methods implementation
BackupResult_t FlashBackupManager::readFirmwareFromBank(uint32_t bankAddress, uint8_t* buffer, 
                                                       uint32_t size, uint32_t offset) {
    if (!isValidFlashAddress(bankAddress + offset) || !isValidFlashAddress(bankAddress + offset + size - 1)) {
        return BACKUP_ERROR_UNKNOWN;
    }
    
    // Use FlasherX flash read primitives
    memcpy(buffer, (void*)(bankAddress + offset), size);
    return BACKUP_SUCCESS;
}

BackupResult_t FlashBackupManager::writeFirmwareToBank(uint32_t bankAddress, const uint8_t* buffer, 
                                                      uint32_t size, uint32_t offset) {
    if (!isValidFlashAddress(bankAddress + offset) || !isValidFlashAddress(bankAddress + offset + size - 1)) {
        return BACKUP_ERROR_UNKNOWN;
    }
    
    // Use FlasherX flash write primitives
    uint32_t offset_addr = bankAddress + offset - FLASH_BASE_ADDR; // Convert to offset from flash base
    
    if (flash_write_block(offset_addr, buffer, size) != 0) {
        return BACKUP_ERROR_WRITE_FAILED;
    }
    
    return BACKUP_SUCCESS;
}

BackupResult_t FlashBackupManager::eraseFirmwareBank(uint32_t bankAddress, uint32_t size) {
    if (!isValidFlashAddress(bankAddress) || !isValidFlashAddress(bankAddress + size - 1)) {
        return BACKUP_ERROR_UNKNOWN;
    }
    
    uint32_t sectorsToErase = calculateSectorsNeeded(size);
    
    for (uint32_t sector = 0; sector < sectorsToErase; sector++) {
        uint32_t sectorAddress = bankAddress + (sector * FLASH_SECTOR_SIZE);
        if (flash_erase_sector(sectorAddress) != 0) {
            return BACKUP_ERROR_ERASE_FAILED;
        }
    }
    
    return BACKUP_SUCCESS;
}

uint32_t FlashBackupManager::calculateFirmwareChecksum(uint32_t bankAddress, uint32_t size) {
    // Simple CRC32 calculation
    uint32_t crc = 0xFFFFFFFF;
    uint8_t* data = (uint8_t*)bankAddress;
    
    for (uint32_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    
    return ~crc;
}

bool FlashBackupManager::verifyFirmwareIntegrity(uint32_t bankAddress, uint32_t size, uint32_t expectedChecksum) {
    uint32_t calculatedChecksum = calculateFirmwareChecksum(bankAddress, size);
    return (calculatedChecksum == expectedChecksum);
}

BackupResult_t FlashBackupManager::verifyBackupIntegrity() {
    if (!_backupStatus.hasValidBackup) {
        return BACKUP_ERROR_NO_BACKUP;
    }
    
    bool isValid = verifyFirmwareIntegrity(BACKUP_FIRMWARE_BASE, _backupStatus.backupSize, _backupStatus.backupChecksum);
    return isValid ? BACKUP_SUCCESS : BACKUP_ERROR_VERIFY_FAILED;
}

bool FlashBackupManager::extractVersionFromFirmware(uint32_t bankAddress, FirmwareVersion_t* version) {
    // TODO: Implement version extraction from firmware binary
    // This would require knowing where version information is stored in the firmware
    // For now, return false to indicate version extraction is not implemented
    return false;
}

BackupResult_t FlashBackupManager::validateFirmwareVersion(const FirmwareVersion_t& version) {
    // Basic version validation
    if (version.major == 0 && version.minor == 0 && version.patch == 0) {
        return BACKUP_ERROR_VERSION_MISMATCH;
    }
    
    return BACKUP_SUCCESS;
}

void FlashBackupManager::reportProgress(uint8_t progress) {
    if (_progressCallback) {
        _progressCallback(progress);
    }
}

void FlashBackupManager::updateBackupStatus() {
    // Scan backup bank to determine if valid backup exists
    // This is a simplified implementation - in practice, you'd look for
    // specific markers or headers to identify valid firmware
    
    _backupStatus.hasValidBackup = false;
    _backupStatus.backupSize = 0;
    _backupStatus.backupChecksum = 0;
    
    // TODO: Implement proper backup detection logic
    // For now, assume no backup until we can properly detect it
    
    _backupStatusValid = true;
    _lastStatusUpdate = millis();
}

void FlashBackupManager::setLastError(BackupResult_t result, const String& error) {
    _backupStatus.lastOperation = result;
    _backupStatus.lastError = error;
    
    DiagnosticManager::logMessage(LOG_ERROR, "FlashBackupManager", error);
}

void FlashBackupManager::logBackupOperation(const String& operation, BackupResult_t result) {
    String message = operation + " - " + String(backupResultToString(result));
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", message);
}

void FlashBackupManager::logBackupEvent(const String& event) {
    DiagnosticManager::logMessage(LOG_INFO, "FlashBackupManager", "BACKUP_EVENT - " + event);
}

BackupResult_t FlashBackupManager::performSafetyChecks() {
    if (!isFlashOperationSafe()) {
        return BACKUP_ERROR_FLASH_BUSY;
    }
    
    return BACKUP_SUCCESS;
}

bool FlashBackupManager::isFlashOperationSafe() {
    // TODO: Implement flash safety checks
    // Check if flash is not busy, power is stable, etc.
    return true;
}

bool FlashBackupManager::isValidFlashAddress(uint32_t address) {
    return (address >= CURRENT_FIRMWARE_BASE && address < (BACKUP_FIRMWARE_BASE + FIRMWARE_MAX_SIZE));
}

bool FlashBackupManager::isAddressInCurrentBank(uint32_t address) {
    return (address >= CURRENT_FIRMWARE_BASE && address < BACKUP_FIRMWARE_BASE);
}

bool FlashBackupManager::isAddressInBackupBank(uint32_t address) {
    return (address >= BACKUP_FIRMWARE_BASE && address < (BACKUP_FIRMWARE_BASE + FIRMWARE_MAX_SIZE));
}

uint32_t FlashBackupManager::alignToSectorBoundary(uint32_t address) {
    return (address / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
}

uint32_t FlashBackupManager::calculateSectorsNeeded(uint32_t size) {
    return (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
}

// Helper functions
const char* backupResultToString(BackupResult_t result) {
    switch (result) {
        case BACKUP_SUCCESS: return "SUCCESS";
        case BACKUP_ERROR_INVALID_SIZE: return "INVALID_SIZE";
        case BACKUP_ERROR_READ_FAILED: return "READ_FAILED";
        case BACKUP_ERROR_WRITE_FAILED: return "WRITE_FAILED";
        case BACKUP_ERROR_VERIFY_FAILED: return "VERIFY_FAILED";
        case BACKUP_ERROR_ERASE_FAILED: return "ERASE_FAILED";
        case BACKUP_ERROR_NO_BACKUP: return "NO_BACKUP";
        case BACKUP_ERROR_CORRUPTED: return "CORRUPTED";
        case BACKUP_ERROR_VERSION_MISMATCH: return "VERSION_MISMATCH";
        case BACKUP_ERROR_FLASH_BUSY: return "FLASH_BUSY";
        case BACKUP_ERROR_UNKNOWN: return "UNKNOWN_ERROR";
        default: return "INVALID_RESULT";
    }
}

bool isBackupResultSuccess(BackupResult_t result) {
    return (result == BACKUP_SUCCESS);
}

bool isBackupResultError(BackupResult_t result) {
    return (result != BACKUP_SUCCESS);
}
