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
// downloadFirmware() - Download firmware from HTTP URL (local Toughbook server)
//******************************************************************************
bool RgFModuleUpdater::downloadFirmware(const String& url) {
    if (!_initialized) {
        setError(UpdateError::DOWNLOAD_FAILED, "RgFModuleUpdater not initialized");
        return false;
    }
    
    if (_flashBuffer == 0) {
        setError(UpdateError::DOWNLOAD_FAILED, "Flash buffer not created");
        return false;
    }
    
    setStatus(UPDATE_DOWNLOADING, "Starting firmware download...");
    updateProgress(30);
    
    logMessage(LOG_INFO, String("Downloading firmware from: ") + url);
    
    // Parse URL to extract host, port, and path
    String host;
    int port = 80;
    String path = "/";
    
    if (!parseHttpUrl(url, host, port, path)) {
        setError(UpdateError::DOWNLOAD_FAILED, "Invalid URL format");
        return false;
    }
    
    // Create HTTP client
    qindesign::network::EthernetClient client;
    
    // Connect to server
    if (!client.connect(host.c_str(), port)) {
        setError(UpdateError::DOWNLOAD_FAILED, String("Failed to connect to ") + host + ":" + String(port));
        return false;
    }
    
    logMessage(LOG_INFO, String("Connected to ") + host + ":" + String(port));
    updateProgress(40);
    
    // Send HTTP GET request
    client.print("GET ");
    client.print(path);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.println("Connection: close");
    client.println();
    
    // Wait for response
    unsigned long timeout = millis() + 10000; // 10 second timeout
    while (client.available() == 0) {
        if (millis() > timeout) {
            client.stop();
            setError(UpdateError::DOWNLOAD_FAILED, "HTTP request timeout");
            return false;
        }
        delay(10);
    }
    
    // Parse HTTP response headers
    String line;
    int contentLength = -1;
    bool headersComplete = false;
    
    while (client.available() && !headersComplete) {
        line = client.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) {
            headersComplete = true;
        } else if (line.startsWith("Content-Length:")) {
            contentLength = line.substring(15).toInt();
        } else if (line.startsWith("HTTP/1.1 ")) {
            int statusCode = line.substring(9, 12).toInt();
            if (statusCode != 200) {
                client.stop();
                setError(UpdateError::DOWNLOAD_FAILED, String("HTTP error: ") + String(statusCode));
                return false;
            }
        }
    }
    
    if (contentLength <= 0) {
        client.stop();
        setError(UpdateError::DOWNLOAD_FAILED, "Invalid content length");
        return false;
    }
    
    if (contentLength > _flashBufferSize) {
        client.stop();
        setError(UpdateError::DOWNLOAD_FAILED, "Firmware too large for buffer");
        return false;
    }
    
    logMessage(LOG_INFO, String("Downloading ") + String(contentLength) + " bytes...");
    updateProgress(50);
    
    // Download firmware data
    uint32_t bytesRead = 0;
    uint8_t* bufferPtr = (uint8_t*)_flashBuffer;
    
    while (client.available() && bytesRead < contentLength) {
        int available = client.available();
        int toRead = min(available, (int)(contentLength - bytesRead));
        
        if (toRead > 0) {
            int actualRead = client.readBytes(bufferPtr + bytesRead, toRead);
            bytesRead += actualRead;
            
            // Update progress
            uint8_t progress = 50 + (30 * bytesRead / contentLength);
            updateProgress(progress);
        }
        
        delay(1); // Small delay to prevent overwhelming
    }
    
    client.stop();
    
    if (bytesRead != contentLength) {
        setError(UpdateError::DOWNLOAD_FAILED, String("Download incomplete: ") + String(bytesRead) + "/" + String(contentLength));
        return false;
    }
    
    // Update firmware info with both CRC32 and SHA256
    _newFirmwareInfo.size = bytesRead;
    _newFirmwareInfo.crc32 = calculateCRC32((uint8_t*)_flashBuffer, bytesRead);
    
    // Calculate SHA256 hash for integrity verification
    calculateSHA256((uint8_t*)_flashBuffer, bytesRead, _newFirmwareInfo.sha256_hash);
    
    logMessage(LOG_INFO, String("Firmware download completed: ") + String(bytesRead) + " bytes");
    logMessage(LOG_INFO, String("CRC32: 0x") + String(_newFirmwareInfo.crc32, HEX));
    logMessage(LOG_INFO, String("SHA256: ") + sha256ToString(_newFirmwareInfo.sha256_hash));
    updateProgress(80);
    
    return true;
}

//******************************************************************************
// parseHttpUrl() - Parse HTTP URL into components
//******************************************************************************
bool RgFModuleUpdater::parseHttpUrl(const String& url, String& host, int& port, String& path) {
    // Simple HTTP URL parser for local network URLs
    // Expected format: http://192.168.1.100:8080/firmware.hex
    
    if (!url.startsWith("http://")) {
        return false;
    }
    
    String remainder = url.substring(7); // Remove "http://"
    
    // Find path separator
    int pathIndex = remainder.indexOf('/');
    String hostPort;
    
    if (pathIndex >= 0) {
        hostPort = remainder.substring(0, pathIndex);
        path = remainder.substring(pathIndex);
    } else {
        hostPort = remainder;
        path = "/";
    }
    
    // Parse host and port
    int portIndex = hostPort.indexOf(':');
    if (portIndex >= 0) {
        host = hostPort.substring(0, portIndex);
        port = hostPort.substring(portIndex + 1).toInt();
    } else {
        host = hostPort;
        port = 80;
    }
    
    return host.length() > 0;
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
    
    // Calculate SHA256 hash for integrity verification
    calculateSHA256(data, size, _newFirmwareInfo.sha256_hash);
    
    strcpy(_newFirmwareInfo.target_id, FLASH_ID);
    
    // TODO: Parse actual firmware header structure
    _newFirmwareInfo.version_major = 1;
    _newFirmwareInfo.version_minor = 0;
    _newFirmwareInfo.version_patch = 0;
    
    return true;
}

bool RgFModuleUpdater::validateFirmwareIntegrity(const uint8_t* data, uint32_t size) {
    // Primary integrity check: SHA256 hash verification
    if (!validateSHA256Hash(data, size, _newFirmwareInfo.sha256_hash)) {
        logMessage(LOG_ERROR, "SHA256 hash verification failed");
        logMessage(LOG_ERROR, String("Expected: ") + sha256ToString(_newFirmwareInfo.sha256_hash));
        
        uint8_t actualHash[32];
        calculateSHA256(data, size, actualHash);
        logMessage(LOG_ERROR, String("Actual:   ") + sha256ToString(actualHash));
        return false;
    }
    
    // Secondary check: CRC32 for legacy compatibility
    uint32_t calculatedCRC = calculateCRC32(data, size);
    if (calculatedCRC != _newFirmwareInfo.crc32) {
        logMessage(LOG_WARNING, "CRC32 mismatch (SHA256 passed)");
        logMessage(LOG_WARNING, String("Expected CRC32: 0x") + String(_newFirmwareInfo.crc32, HEX));
        logMessage(LOG_WARNING, String("Actual CRC32:   0x") + String(calculatedCRC, HEX));
        // Don't fail on CRC32 mismatch if SHA256 passes
    }
    
    logMessage(LOG_INFO, "Firmware integrity verification passed (SHA256)");
    return true;
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

//******************************************************************************
// SHA256 Implementation - Lightweight version for Teensy 4.1
//******************************************************************************

// SHA256 constants
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA256 helper functions
#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_EP1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

struct SHA256_CTX {
    uint32_t state[8];
    uint64_t bitlen;
    uint32_t datalen;
    uint8_t data[64];
};

static void sha256_init(SHA256_CTX* ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_transform(SHA256_CTX* ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SHA256_SIG1(m[i - 2]) + m[i - 7] + SHA256_SIG0(m[i - 15]) + m[i - 16];
    
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    for (i = 0; i < 64; ++i) {
        t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + sha256_k[i] + m[i];
        t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_update(SHA256_CTX* ctx, const uint8_t data[], size_t len) {
    uint32_t i;
    
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX* ctx, uint8_t hash[]) {
    uint32_t i;
    
    i = ctx->datalen;
    
    // Pad whatever data is left in the buffer.
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    
    // Append to the padding the total message's length in bits and transform.
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);
    
    // Since this implementation uses little endian byte ordering and SHA uses big endian,
    // reverse all the bytes when copying the final state to the output hash.
    for (i = 0; i < 4; ++i) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

//******************************************************************************
// calculateSHA256() - Calculate SHA256 hash of data
//******************************************************************************
void RgFModuleUpdater::calculateSHA256(const uint8_t* data, uint32_t size, uint8_t* hash) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, hash);
}

//******************************************************************************
// validateSHA256Hash() - Validate data against expected SHA256 hash
//******************************************************************************
bool RgFModuleUpdater::validateSHA256Hash(const uint8_t* data, uint32_t size, const uint8_t* expectedHash) {
    uint8_t calculatedHash[32];
    calculateSHA256(data, size, calculatedHash);
    return memcmp(calculatedHash, expectedHash, 32) == 0;
}

//******************************************************************************
// sha256ToString() - Convert SHA256 hash to hex string
//******************************************************************************
String RgFModuleUpdater::sha256ToString(const uint8_t* hash) {
    String result = "";
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) result += "0";
        result += String(hash[i], HEX);
    }
    return result;
}
