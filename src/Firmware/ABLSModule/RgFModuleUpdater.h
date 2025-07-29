/*
 * RgFModuleUpdater.h - Professional OTA firmware update system for RgF ABLS modules
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
 * RgF ABLS Integration:
 * - Network-based firmware download (HTTP)
 * - Binary firmware handling (no Intel HEX)
 * - Progress reporting via UDP
 * - Enhanced safety checks for agricultural equipment
 * - Rollback capability for field reliability
 * 
 * Copyright (c) 2025 RgF - Rural & General Farming
 * Adapted for RgF ABLS project.
 */

#ifndef RGFMODULEUPDATER_H
#define RGFMODULEUPDATER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "FlasherX/FlashTxx.h"
#include "DiagnosticManager.h"
#include "VersionManager.h"

// RgFModuleUpdater version
#define RGFMODULEUPDATER_VERSION_MAJOR  1
#define RGFMODULEUPDATER_VERSION_MINOR  0
#define RGFMODULEUPDATER_VERSION_PATCH  0

// Use existing UpdateStatus_t from VersionManager.h
// No need to redefine - using UPDATE_IDLE, UPDATE_DOWNLOADING, etc.

// Update error codes
enum class UpdateError {
    NONE = 0,
    BUFFER_INIT_FAILED,
    DOWNLOAD_FAILED,
    VALIDATION_FAILED,
    FLASH_FAILED,
    VERIFICATION_FAILED,
    ROLLBACK_FAILED,
    NETWORK_ERROR,
    INSUFFICIENT_SPACE,
    INVALID_FIRMWARE,
    SAFETY_CHECK_FAILED
};

// Progress callback function type
typedef void (*ProgressCallback)(uint8_t progress, UpdateStatus_t status, const String& message);

// Firmware validation structure
struct FirmwareInfo {
    uint32_t size;
    uint32_t crc32;                    // Legacy CRC32 for compatibility
    uint8_t sha256_hash[32];           // SHA256 hash for integrity verification
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    char target_id[16];
    char build_date[16];
    char build_time[16];
};

class RgFModuleUpdater {
public:
    // Initialization and configuration
    static bool initialize();
    static void setProgressCallback(ProgressCallback callback);
    static void setDiagnosticManager(DiagnosticManager* diagnostics);
    
    // Flash buffer management
    static bool createFlashBuffer();
    static void freeFlashBuffer();
    static uint32_t getBufferAddress() { return _flashBuffer; }
    static uint32_t getBufferSize() { return _flashBufferSize; }
    
    // Firmware operations
    static bool downloadFirmware(const String& url);
    static bool downloadFirmwareFromBuffer(const uint8_t* data, uint32_t size);
    static bool validateFirmware();
    static bool flashFirmware();
    static bool verifyFirmware();
    
    // Backup and rollback
    static bool createBackup();
    static bool rollbackFirmware();
    static bool hasBackup() { return _hasBackup; }
    
    // Status and information
    static UpdateStatus_t getStatus() { return _status; }
    static UpdateError getLastError() { return _lastError; }
    static uint8_t getProgress() { return _progress; }
    static String getStatusMessage() { return _statusMessage; }
    static FirmwareInfo getCurrentFirmwareInfo();
    static FirmwareInfo getNewFirmwareInfo() { return _newFirmwareInfo; }
    
    // Safety checks
    static bool performSafetyChecks();
    static bool isSystemStationary();
    static bool isNetworkStable();
    
    // Complete update workflow
    static bool performUpdate(const String& firmwareUrl);
    static bool performUpdateFromBuffer(const uint8_t* data, uint32_t size);
    
    // Utility functions
    static uint32_t calculateCRC32(const uint8_t* data, uint32_t size);
    static void calculateSHA256(const uint8_t* data, uint32_t size, uint8_t* hash);
    static bool validateSHA256Hash(const uint8_t* data, uint32_t size, const uint8_t* expectedHash);
    static String sha256ToString(const uint8_t* hash);
    static bool validateTargetCompatibility(const char* targetId);
    static void reboot();
    
private:
    // Internal state
    static bool _initialized;
    static uint32_t _flashBuffer;
    static uint32_t _flashBufferSize;
    static uint32_t _backupBuffer;
    static uint32_t _backupSize;
    static bool _hasBackup;
    
    // Update state
    static UpdateStatus_t _status;
    static UpdateError _lastError;
    static uint8_t _progress;
    static String _statusMessage;
    static FirmwareInfo _newFirmwareInfo;
    
    // Callbacks and diagnostics
    static ProgressCallback _progressCallback;
    static DiagnosticManager* _diagnostics;
    
    // Internal helper functions
    static void setStatus(UpdateStatus_t status, const String& message = "");
    static void setError(UpdateError error, const String& message = "");
    static void updateProgress(uint8_t progress);
    static void logMessage(LogLevel_t level, const String& message);
    
    // Flash operations
    static bool eraseFlashRange(uint32_t address, uint32_t size);
    static bool writeFlashBlock(uint32_t address, const uint8_t* data, uint32_t size);
    static bool readFlashBlock(uint32_t address, uint8_t* data, uint32_t size);
    
    // Firmware parsing and validation
    static bool parseFirmwareHeader(const uint8_t* data, uint32_t size);
    static bool validateFirmwareIntegrity(const uint8_t* data, uint32_t size);
    static bool validateFirmwareCompatibility();
    
    // Network operations
    static bool downloadFromURL(const String& url, uint32_t bufferAddr, uint32_t maxSize);
    static bool parseHttpUrl(const String& url, String& host, int& port, String& path);
    
    // Constants
    static const uint32_t FIRMWARE_HEADER_SIZE = 256;
    static const uint32_t BACKUP_SIGNATURE = 0x52674642; // "RgFB" - RgF Backup
    static const char* EXPECTED_TARGET_ID;
};

#endif // RGFMODULEUPDATER_H
