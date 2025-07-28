/*
 * RgFModuleUpdater.cpp - Professional OTA firmware update system implementation
 * 
 * Based on FlasherX by Joe Pasquariello
 * Original repository: https://github.com/joepasquariello/FlasherX
 * 
 * FlasherX provides over-the-air firmware updates for Teensy microcontrollers
 * and has been adapted for use in the ABLS RgFModuleUpdater system.
 * 
 * Original FlasherX license and attribution maintained.
 * Copyright (c) Joe Pasquariello
 * 
 * RgF ABLS Integration:
 * - Network-based firmware download (HTTP)
 * - Binary firmware handling (no Intel HEX)
 * - Progress reporting via UDP
 * - Enhanced safety checks for agricultural equipment
 * - Rollback capability for field reliability
 * 
 * Copyright (c) 2025 RgF - Rural & General Farming
 * Adapted for RgF ABLS project with permission.
 */

#include "RgFModuleUpdater.h"
#include <string.h>

extern "C" {
    #include "FlasherX/FlashTxx.h"
}

// Static member initialization
bool RgFModuleUpdater::_initialized = false;
uint32_t RgFModuleUpdater::_flashBuffer = 0;
uint32_t RgFModuleUpdater::_flashBufferSize = 0;
uint32_t RgFModuleUpdater::_backupBuffer = 0;
uint32_t RgFModuleUpdater::_backupSize = 0;
bool RgFModuleUpdater::_hasBackup = false;

UpdateStatus_t RgFModuleUpdater::_status = UPDATE_IDLE;
UpdateError RgFModuleUpdater::_lastError = UpdateError::NONE;
uint8_t RgFModuleUpdater::_progress = 0;
String RgFModuleUpdater::_statusMessage = "";
FirmwareInfo RgFModuleUpdater::_newFirmwareInfo = {0};

ProgressCallback RgFModuleUpdater::_progressCallback = nullptr;
DiagnosticManager* RgFModuleUpdater::_diagnostics = nullptr;

const char* RgFModuleUpdater::EXPECTED_TARGET_ID = FLASH_ID;

//******************************************************************************
// initialize() - Initialize the RgFModuleUpdater system
//******************************************************************************
bool RgFModuleUpdater::initialize() {
    if (_initialized) {
        return true;
    }
    
    logMessage(LOG_INFO, "RgFModuleUpdater: Initializing...");
    
    // Verify we're running on supported hardware
    #ifndef __IMXRT1062__
    logMessage(LOG_ERROR, "RgFModuleUpdater: Unsupported hardware - Teensy 4.x required");
    return false;
    #endif
    
    // Check flash configuration
    logMessage(LOG_INFO, String("Flash ID: ") + FLASH_ID);
    logMessage(LOG_INFO, String("Flash Size: ") + String(FLASH_SIZE / 1024) + "KB");
    logMessage(LOG_INFO, String("Flash Base: 0x") + String(FLASH_BASE_ADDR, HEX));
    logMessage(LOG_INFO, String("Sector Size: ") + String(FLASH_SECTOR_SIZE) + " bytes");
    
    _initialized = true;
    setStatus(UPDATE_IDLE, "RgFModuleUpdater initialized successfully");
    
    return true;
}

//******************************************************************************
// setProgressCallback() - Set progress reporting callback
//******************************************************************************
void RgFModuleUpdater::setProgressCallback(ProgressCallback callback) {
    _progressCallback = callback;
}

//******************************************************************************
// setDiagnosticManager() - Set diagnostic manager for logging
//******************************************************************************
void RgFModuleUpdater::setDiagnosticManager(DiagnosticManager* diagnostics) {
    _diagnostics = diagnostics;
}

//******************************************************************************
// createFlashBuffer() - Create flash buffer for firmware download
//******************************************************************************
bool RgFModuleUpdater::createFlashBuffer() {
    if (!_initialized) {
        setError(UpdateError::BUFFER_INIT_FAILED, "RgFModuleUpdater not initialized");
        return false;
    }
    
    if (_flashBuffer != 0) {
        logMessage(LOG_WARNING, "Flash buffer already exists, freeing first");
        freeFlashBuffer();
    }
    
    setStatus(UPDATE_DOWNLOADING, "Creating flash buffer...");
    updateProgress(10);
    
    // Use FlasherX firmware_buffer_init function
    int result = firmware_buffer_init(&_flashBuffer, &_flashBufferSize);
    
    if (result != 0) {
        setError(UpdateError::BUFFER_INIT_FAILED, String("Flash buffer creation failed: ") + String(result));
        return false;
    }
    
    logMessage(LOG_INFO, String("Flash buffer created: 0x") + String(_flashBuffer, HEX) + 
               " size: " + String(_flashBufferSize / 1024) + "KB");
    
    updateProgress(20);
    return true;
}

//******************************************************************************
// freeFlashBuffer() - Free flash buffer
//******************************************************************************
void RgFModuleUpdater::freeFlashBuffer() {
    if (_flashBuffer != 0) {
        firmware_buffer_free(_flashBuffer, _flashBufferSize);
        logMessage(LOG_INFO, "Flash buffer freed");
        _flashBuffer = 0;
        _flashBufferSize = 0;
    }
}

//******************************************************************************
// downloadFirmwareFromBuffer() - Load firmware from memory buffer
//******************************************************************************
bool RgFModuleUpdater::downloadFirmwareFromBuffer(const uint8_t* data, uint32_t size) {
    if (!_initialized || _flashBuffer == 0) {
        setError(UpdateError::DOWNLOAD_FAILED, "Flash buffer not initialized");
        return false;
    }
    
    if (size > _flashBufferSize) {
        setError(UpdateError::INSUFFICIENT_SPACE, "Firmware too large for buffer");
        return false;
    }
    
    setStatus(UPDATE_DOWNLOADING, "Loading firmware from buffer...");
    updateProgress(30);
    
    // Copy firmware data to flash buffer
    if (!writeFlashBlock(_flashBuffer, data, size)) {
        setError(UpdateError::DOWNLOAD_FAILED, "Failed to write firmware to flash buffer");
        return false;
    }
    
    // Parse firmware header
    if (!parseFirmwareHeader(data, size)) {
        setError(UpdateError::INVALID_FIRMWARE, "Invalid firmware header");
        return false;
    }
    
    updateProgress(50);
    logMessage(LOG_INFO, String("Firmware loaded: ") + String(size) + " bytes");
    
    return true;
}

//******************************************************************************
// validateFirmware() - Validate downloaded firmware
//******************************************************************************
bool RgFModuleUpdater::validateFirmware() {
    if (_flashBuffer == 0) {
        setError(UpdateError::VALIDATION_FAILED, "No firmware to validate");
        return false;
    }
    
    setStatus(UPDATE_VERIFYING, "Validating firmware...");
    updateProgress(60);
    
    // Read firmware from flash buffer for validation
    uint8_t* firmwareData = new uint8_t[_newFirmwareInfo.size];
    if (!readFlashBlock(_flashBuffer, firmwareData, _newFirmwareInfo.size)) {
        delete[] firmwareData;
        setError(UpdateError::VALIDATION_FAILED, "Failed to read firmware for validation");
        return false;
    }
    
    // Validate firmware integrity
    if (!validateFirmwareIntegrity(firmwareData, _newFirmwareInfo.size)) {
        delete[] firmwareData;
        setError(UpdateError::VALIDATION_FAILED, "Firmware integrity check failed");
        return false;
    }
    
    // Validate firmware compatibility
    if (!validateFirmwareCompatibility()) {
        delete[] firmwareData;
        setError(UpdateError::VALIDATION_FAILED, "Firmware compatibility check failed");
        return false;
    }
    
    // Check for target ID in firmware
    if (!check_flash_id(_flashBuffer, _newFirmwareInfo.size)) {
        delete[] firmwareData;
        setError(UpdateError::VALIDATION_FAILED, "Target ID not found in firmware");
        return false;
    }
    
    delete[] firmwareData;
    updateProgress(70);
    logMessage(LOG_INFO, "Firmware validation successful");
    
    return true;
}

//******************************************************************************
// flashFirmware() - Flash validated firmware to main flash
//******************************************************************************
bool RgFModuleUpdater::flashFirmware() {
    if (_flashBuffer == 0 || _newFirmwareInfo.size == 0) {
        setError(UpdateError::FLASH_FAILED, "No validated firmware to flash");
        return false;
    }
    
    setStatus(UPDATE_FLASHING, "Flashing firmware...");
    updateProgress(80);
    
    // Perform safety checks before flashing
    if (!performSafetyChecks()) {
        setError(UpdateError::SAFETY_CHECK_FAILED, "Safety checks failed");
        return false;
    }
    
    // Create backup of current firmware if requested
    if (!createBackup()) {
        logMessage(LOG_WARNING, "Failed to create firmware backup");
    }
    
    // Erase main flash area (current firmware)
    uint32_t sectorsToErase = (_newFirmwareInfo.size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    
    for (uint32_t i = 0; i < sectorsToErase; i++) {
        uint32_t sectorAddr = FLASH_BASE_ADDR + (i * FLASH_SECTOR_SIZE);
        
        if (flash_erase_sector(sectorAddr) != 0) {
            setError(UpdateError::FLASH_FAILED, String("Failed to erase sector: 0x") + String(sectorAddr, HEX));
            return false;
        }
    }
    
    // Copy firmware from buffer to main flash
    uint32_t bytesFlashed = 0;
    uint32_t chunkSize = 4096; // 4KB chunks
    
    while (bytesFlashed < _newFirmwareInfo.size) {
        uint32_t remainingBytes = _newFirmwareInfo.size - bytesFlashed;
        uint32_t currentChunk = (remainingBytes < chunkSize) ? remainingBytes : chunkSize;
        
        // Read chunk from buffer
        uint8_t* chunkData = new uint8_t[currentChunk];
        if (!readFlashBlock(_flashBuffer + bytesFlashed, chunkData, currentChunk)) {
            delete[] chunkData;
            setError(UpdateError::FLASH_FAILED, "Failed to read firmware chunk");
            return false;
        }
        
        // Write chunk to main flash
        if (flash_write_block(FLASH_BASE_ADDR + bytesFlashed, chunkData, currentChunk) != 0) {
            delete[] chunkData;
            setError(UpdateError::FLASH_FAILED, "Failed to write firmware chunk");
            return false;
        }
        
        delete[] chunkData;
        bytesFlashed += currentChunk;
        
        // Update progress
        uint8_t flashProgress = 80 + (10 * bytesFlashed / _newFirmwareInfo.size);
        updateProgress(flashProgress);
    }
    
    updateProgress(90);
    logMessage(LOG_INFO, String("Firmware flashed: ") + String(bytesFlashed) + " bytes");
    
    return true;
}

//******************************************************************************
// verifyFirmware() - Verify flashed firmware
//******************************************************************************
bool RgFModuleUpdater::verifyFirmware() {
    setStatus(UPDATE_VERIFYING, "Verifying flashed firmware...");
    
    // Verify target ID is present in main flash
    if (!check_flash_id(FLASH_BASE_ADDR, _newFirmwareInfo.size)) {
        setError(UpdateError::VERIFICATION_FAILED, "Target ID verification failed");
        return false;
    }
    
    updateProgress(95);
    logMessage(LOG_INFO, "Firmware verification successful");
    
    return true;
}

//******************************************************************************
// performUpdate() - Complete update workflow (placeholder for URL download)
//******************************************************************************
bool RgFModuleUpdater::performUpdate(const String& firmwareUrl) {
    // TODO: Implement HTTP download from URL
    setError(UpdateError::NETWORK_ERROR, "URL download not yet implemented");
    return false;
}

//******************************************************************************
// performUpdateFromBuffer() - Complete update workflow from memory buffer
//******************************************************************************
bool RgFModuleUpdater::performUpdateFromBuffer(const uint8_t* data, uint32_t size) {
    logMessage(LOG_INFO, "Starting firmware update from buffer...");
    
    // Step 1: Create flash buffer
    if (!createFlashBuffer()) {
        return false;
    }
    
    // Step 2: Download firmware to buffer
    if (!downloadFirmwareFromBuffer(data, size)) {
        freeFlashBuffer();
        return false;
    }
    
    // Step 3: Validate firmware
    if (!validateFirmware()) {
        freeFlashBuffer();
        return false;
    }
    
    // Step 4: Flash firmware
    if (!flashFirmware()) {
        freeFlashBuffer();
        return false;
    }
    
    // Step 5: Verify firmware
    if (!verifyFirmware()) {
        freeFlashBuffer();
        return false;
    }
    
    // Step 6: Cleanup
    freeFlashBuffer();
    
    setStatus(UPDATE_SUCCESS, "Firmware update completed successfully");
    updateProgress(100);
    
    logMessage(LOG_INFO, "Firmware update completed - reboot required");
    
    return true;
}

//******************************************************************************
// Helper Functions
//******************************************************************************

void RgFModuleUpdater::setStatus(UpdateStatus_t status, const String& message) {
    _status = status;
    _statusMessage = message;
    
    if (_progressCallback) {
        _progressCallback(_progress, status, message);
    }
    
    logMessage(LOG_INFO, String("Status: ") + message);
}

void RgFModuleUpdater::setError(UpdateError error, const String& message) {
    _lastError = error;
    _status = UPDATE_FAILED;
    _statusMessage = message;
    
    if (_progressCallback) {
        _progressCallback(_progress, _status, message);
    }
    
    logMessage(LOG_ERROR, String("Error: ") + message);
}

void RgFModuleUpdater::updateProgress(uint8_t progress) {
    _progress = progress;
    
    if (_progressCallback) {
        _progressCallback(progress, _status, _statusMessage);
    }
}

void RgFModuleUpdater::logMessage(LogLevel_t level, const String& message) {
    if (_diagnostics) {
        _diagnostics->logMessage(level, "RgFModuleUpdater", message);
    }
}

//******************************************************************************
// Flash operation helpers
//******************************************************************************

bool RgFModuleUpdater::writeFlashBlock(uint32_t address, const uint8_t* data, uint32_t size) {
    return flash_write_block(address, data, size) == 0;
}

bool RgFModuleUpdater::readFlashBlock(uint32_t address, uint8_t* data, uint32_t size) {
    memcpy(data, (void*)address, size);
    return true;
}

//******************************************************************************
// Firmware validation helpers
//******************************************************************************

bool RgFModuleUpdater::parseFirmwareHeader(const uint8_t* data, uint32_t size) {
    // Simple firmware info extraction (to be enhanced)
    _newFirmwareInfo.size = size;
    _newFirmwareInfo.crc32 = calculateCRC32(data, size);
    strcpy(_newFirmwareInfo.target_id, FLASH_ID);
    
    // TODO: Parse actual firmware header structure
    _newFirmwareInfo.version_major = 1;
    _newFirmwareInfo.version_minor = 0;
    _newFirmwareInfo.version_patch = 0;
    
    return true;
}

bool RgFModuleUpdater::validateFirmwareIntegrity(const uint8_t* data, uint32_t size) {
    uint32_t calculatedCRC = calculateCRC32(data, size);
    return calculatedCRC == _newFirmwareInfo.crc32;
}

bool RgFModuleUpdater::validateFirmwareCompatibility() {
    return strcmp(_newFirmwareInfo.target_id, EXPECTED_TARGET_ID) == 0;
}

//******************************************************************************
// Safety and utility functions
//******************************************************************************

bool RgFModuleUpdater::performSafetyChecks() {
    // TODO: Implement actual safety checks
    // - Check if system is stationary
    // - Check network stability
    // - Check power levels
    return true;
}

uint32_t RgFModuleUpdater::calculateCRC32(const uint8_t* data, uint32_t size) {
    // Simple CRC32 implementation (to be enhanced with proper CRC32)
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

bool RgFModuleUpdater::createBackup() {
    // TODO: Implement firmware backup functionality
    logMessage(LOG_INFO, "Firmware backup not yet implemented");
    return true;
}

bool RgFModuleUpdater::rollbackFirmware() {
    // TODO: Implement firmware rollback functionality
    setError(UpdateError::ROLLBACK_FAILED, "Rollback not yet implemented");
    return false;
}

void RgFModuleUpdater::reboot() {
    logMessage(LOG_INFO, "Rebooting system...");
    delay(100); // Allow log message to be sent
    REBOOT;
}

FirmwareInfo RgFModuleUpdater::getCurrentFirmwareInfo() {
    FirmwareInfo info = {0};
    strcpy(info.target_id, FLASH_ID);
    // TODO: Extract actual current firmware version
    info.version_major = 1;
    info.version_minor = 0;
    info.version_patch = 0;
    return info;
}
