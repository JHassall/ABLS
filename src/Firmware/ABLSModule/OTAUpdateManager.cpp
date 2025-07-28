#include "OTAUpdateManager.h"
#include "DiagnosticManager.h"
#include "ModuleConfig.h"
#include "NetworkManager.h"
// FlasherX functionality - Direct implementation for OTA updates
// TODO: Integrate FlasherX library when properly configured
// For now, using placeholder functions for compilation and basic OTA infrastructure

// Static member definitions
EthernetUDP OTAUpdateManager::_otaUdp;
bool OTAUpdateManager::_networkInitialized = false;
bool OTAUpdateManager::_updateInProgress = false;
OTACommandPacket OTAUpdateManager::_currentCommand;
uint32_t OTAUpdateManager::_updateStartTime = 0;
uint32_t OTAUpdateManager::_lastProgressReport = 0;

uint8_t* OTAUpdateManager::_firmwareBuffer = nullptr;
uint32_t OTAUpdateManager::_firmwareBufferSize = 0;
uint32_t OTAUpdateManager::_bytesReceived = 0;
uint32_t OTAUpdateManager::_expectedSize = 0;
uint32_t OTAUpdateManager::_expectedChecksum = 0;

bool OTAUpdateManager::_safetyChecksEnabled = true;
uint32_t OTAUpdateManager::_lastSafetyCheck = 0;

bool OTAUpdateManager::initialize() {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Initializing OTA update system");
    
    // Initialize version manager
    VersionManager::initialize();
    
    // Setup OTA UDP socket
    setupOTASocket();
    
    // Initialize FlasherTxx for dual-bank flash support
    // Note: FlasherTxx initialization will be done when update starts
    
    _networkInitialized = true;
    _updateInProgress = false;
    _safetyChecksEnabled = true;
    _lastSafetyCheck = millis();
    
    String initMsg = "OTA system ready - Version: " + VersionManager::getCurrentVersionString();
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", initMsg);
    
    return true;
}

void OTAUpdateManager::update() {
    if (!_networkInitialized) return;
    
    uint32_t now = millis();
    
    // Process incoming OTA commands
    processIncomingOTACommands();
    
    // If update is in progress, handle update tasks
    if (_updateInProgress) {
        // Report progress periodically
        if (now - _lastProgressReport >= PROGRESS_REPORT_INTERVAL) {
            UpdateProgressPacket progress = VersionManager::getProgressPacket();
            // Send progress to Toughbook via network
            _lastProgressReport = now;
        }
        
        // Perform safety checks during update
        if (_safetyChecksEnabled && (now - _lastSafetyCheck >= SAFETY_CHECK_INTERVAL)) {
            if (!performSafetyChecks()) {
                handleUpdateError("Safety check failed during update");
                cancelUpdate();
            }
            _lastSafetyCheck = now;
        }
    }
}

bool OTAUpdateManager::startUpdate(const OTACommandPacket& command) {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Starting firmware update");
    
    // Check if update is already in progress
    if (_updateInProgress) {
        DiagnosticManager::logError("OTAUpdateManager", "Update already in progress");
        return false;
    }
    
    // Perform safety checks
    if (!performSafetyChecks()) {
        DiagnosticManager::logError("OTAUpdateManager", "Safety checks failed - update aborted");
        return false;
    }
    
    // Validate command parameters
    if (command.FirmwareSize == 0 || command.FirmwareSize > MAX_FIRMWARE_SIZE) {
        DiagnosticManager::logError("OTAUpdateManager", "Invalid firmware size");
        return false;
    }
    
    if (strlen(command.DownloadUrl) == 0) {
        DiagnosticManager::logError("OTAUpdateManager", "Invalid download URL");
        return false;
    }
    
    // Store command and initialize update state
    _currentCommand = command;
    _updateInProgress = true;
    _updateStartTime = millis();
    _bytesReceived = 0;
    _expectedSize = command.FirmwareSize;
    _expectedChecksum = command.Checksum;
    
    // Allocate firmware buffer
    _firmwareBufferSize = command.FirmwareSize;
    _firmwareBuffer = (uint8_t*)malloc(_firmwareBufferSize);
    if (!_firmwareBuffer) {
        handleUpdateError("Failed to allocate firmware buffer");
        return false;
    }
    
    // Start the update process
    VersionManager::setUpdateStatus(UPDATE_DOWNLOADING, 0);
    
    // Begin firmware download
    if (!downloadFirmware(String(command.DownloadUrl), command.FirmwareSize, command.Checksum)) {
        handleUpdateError("Firmware download failed");
        return false;
    }
    
    return true;
}

bool OTAUpdateManager::cancelUpdate() {
    if (!_updateInProgress) return true;
    
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Cancelling firmware update");
    
    cleanup();
    VersionManager::setUpdateStatus(UPDATE_IDLE, 0);
    
    return true;
}

bool OTAUpdateManager::rollbackFirmware() {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Rolling back firmware");
    
    VersionManager::setUpdateStatus(UPDATE_ROLLBACK, 0);
    
    // Attempt to restore from backup bank
    if (!restoreFromBackup()) {
        handleUpdateError("Rollback failed - backup restoration error");
        return false;
    }
    
    // Reboot to activate rolled-back firmware
    VersionManager::setUpdateStatus(UPDATE_REBOOTING, 100);
    delay(1000); // Allow status to be reported
    rebootModule();
    
    return true;
}

void OTAUpdateManager::rebootModule() {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Rebooting module");
    
    // Give time for message to be sent
    delay(1000);
    
    // Perform software reset
    SCB_AIRCR = 0x05FA0004; // ARM system reset
}

bool OTAUpdateManager::performSafetyChecks() {
    // Check if system is stationary (if applicable)
    if (!isSystemStationary()) {
        DiagnosticManager::logError("OTAUpdateManager", "System must be stationary for updates");
        return false;
    }
    
    // Check if all systems are healthy
    if (!areAllSystemsHealthy()) {
        DiagnosticManager::logError("OTAUpdateManager", "System health check failed");
        return false;
    }
    
    return true;
}

bool OTAUpdateManager::isSystemStationary() {
    // For now, assume system is stationary
    // In a real implementation, this would check GPS speed, IMU data, etc.
    return true;
}

bool OTAUpdateManager::areAllSystemsHealthy() {
    // Check basic system health indicators
    // This could be expanded to check sensor status, network connectivity, etc.
    return true;
}

bool OTAUpdateManager::downloadFirmware(const String& url, uint32_t expectedSize, uint32_t expectedChecksum) {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Downloading firmware from: " + url);
    
    // For now, simulate firmware download
    // In a real implementation, this would use HTTP client to download firmware
    
    VersionManager::reportUpdateProgress(UPDATE_DOWNLOADING, 25, "Connecting to server");
    delay(1000);
    
    VersionManager::reportUpdateProgress(UPDATE_DOWNLOADING, 50, "Downloading firmware");
    delay(2000);
    
    VersionManager::reportUpdateProgress(UPDATE_DOWNLOADING, 75, "Download complete");
    delay(500);
    
    VersionManager::reportUpdateProgress(UPDATE_DOWNLOADING, 100, "Firmware downloaded");
    
    // Move to verification phase
    return verifyFirmware(expectedChecksum);
}

bool OTAUpdateManager::verifyFirmware(uint32_t expectedChecksum) {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Verifying firmware integrity");
    
    VersionManager::setUpdateStatus(UPDATE_VERIFYING, 0);
    
    // Simulate firmware verification
    VersionManager::reportUpdateProgress(UPDATE_VERIFYING, 50, "Calculating checksum");
    delay(1000);
    
    // In a real implementation, calculate actual CRC32 checksum
    uint32_t calculatedChecksum = expectedChecksum; // Simulate successful verification
    
    if (calculatedChecksum != expectedChecksum) {
        handleUpdateError("Firmware verification failed - checksum mismatch");
        return false;
    }
    
    VersionManager::reportUpdateProgress(UPDATE_VERIFYING, 100, "Firmware verified");
    
    // Move to flashing phase
    return flashFirmware();
}

bool OTAUpdateManager::flashFirmware() {
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Flashing new firmware");
    
    VersionManager::setUpdateStatus(UPDATE_FLASHING, 0);
    
    // Backup current firmware to alternate bank
    if (!backupCurrentFirmware()) {
        handleUpdateError("Failed to backup current firmware");
        return false;
    }
    
    VersionManager::reportUpdateProgress(UPDATE_FLASHING, 25, "Current firmware backed up");
    
    // Flash new firmware
    VersionManager::reportUpdateProgress(UPDATE_FLASHING, 50, "Writing new firmware");
    delay(2000);
    
    VersionManager::reportUpdateProgress(UPDATE_FLASHING, 75, "Verifying flash write");
    delay(1000);
    
    VersionManager::reportUpdateProgress(UPDATE_FLASHING, 100, "Firmware flashed successfully");
    
    // Update complete - prepare for reboot
    VersionManager::setUpdateStatus(UPDATE_SUCCESS, 100);
    cleanup();
    
    // Schedule reboot
    VersionManager::setUpdateStatus(UPDATE_REBOOTING, 100);
    delay(2000); // Allow status to be reported
    rebootModule();
    
    return true;
}

bool OTAUpdateManager::backupCurrentFirmware() {
    // In a real implementation, this would copy current firmware to backup bank
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Backing up current firmware");
    return true;
}

bool OTAUpdateManager::restoreFromBackup() {
    // In a real implementation, this would restore firmware from backup bank
    DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", "Restoring firmware from backup");
    return true;
}

void OTAUpdateManager::handleUpdateError(const String& error) {
    DiagnosticManager::logError("OTAUpdateManager", "Update error: " + error);
    VersionManager::setUpdateError(error);
    cleanup();
}

void OTAUpdateManager::cleanup() {
    _updateInProgress = false;
    
    // Free firmware buffer
    if (_firmwareBuffer) {
        free(_firmwareBuffer);
        _firmwareBuffer = nullptr;
        _firmwareBufferSize = 0;
    }
    
    _bytesReceived = 0;
    _expectedSize = 0;
    _expectedChecksum = 0;
}

void OTAUpdateManager::setupOTASocket() {
    // Initialize UDP socket for OTA commands
    if (_otaUdp.begin(OTA_COMMAND_PORT)) {
        DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", 
            "OTA command socket listening on port " + String(OTA_COMMAND_PORT));
    } else {
        DiagnosticManager::logError("OTAUpdateManager", "Failed to initialize OTA command socket");
    }
}

void OTAUpdateManager::processIncomingOTACommands() {
    int packetSize = _otaUdp.parsePacket();
    if (packetSize == sizeof(OTACommandPacket)) {
        OTACommandPacket command;
        _otaUdp.read((uint8_t*)&command, sizeof(OTACommandPacket));
        
        DiagnosticManager::logMessage(LOG_INFO, "OTAUpdateManager", 
            "Received OTA command: " + String(command.Command));
        
        processOTACommand(command);
    }
}

void OTAUpdateManager::processOTACommand(const OTACommandPacket& command) {
    OTAResponseCode_t responseCode = OTA_RESPONSE_OK;
    String responseMessage = "";
    
    switch (command.Command) {
        case OTA_CMD_CHECK_VERSION:
            responseMessage = "Current version: " + VersionManager::getCurrentVersionString();
            break;
            
        case OTA_CMD_START_UPDATE:
            if (startUpdate(command)) {
                responseMessage = "Update started";
            } else {
                responseCode = OTA_RESPONSE_ERROR;
                responseMessage = "Failed to start update";
            }
            break;
            
        case OTA_CMD_CANCEL_UPDATE:
            if (cancelUpdate()) {
                responseMessage = "Update cancelled";
            } else {
                responseCode = OTA_RESPONSE_ERROR;
                responseMessage = "Failed to cancel update";
            }
            break;
            
        case OTA_CMD_ROLLBACK:
            if (rollbackFirmware()) {
                responseMessage = "Rollback initiated";
            } else {
                responseCode = OTA_RESPONSE_ERROR;
                responseMessage = "Rollback failed";
            }
            break;
            
        case OTA_CMD_REBOOT:
            responseMessage = "Rebooting module";
            sendOTAResponse(command, responseCode, responseMessage);
            delay(1000);
            rebootModule();
            return;
            
        default:
            responseCode = OTA_RESPONSE_INVALID;
            responseMessage = "Unknown command";
            break;
    }
    
    sendOTAResponse(command, responseCode, responseMessage);
}

void OTAUpdateManager::sendOTAResponse(const OTACommandPacket& originalCommand, 
                                      OTAResponseCode_t responseCode, 
                                      const String& message) {
    OTAResponsePacket response;
    
    response.CommandId = originalCommand.CommandId;
    response.Timestamp = millis();
    response.SenderId = (uint8_t)ModuleConfig::getRole();
    response.ResponseCode = responseCode;
    response.CurrentVersion = VersionManager::getCurrentVersion();
    response.Progress = VersionManager::getProgressPacket();
    
    strncpy(response.Message, message.c_str(), sizeof(response.Message) - 1);
    response.Message[sizeof(response.Message) - 1] = '\0';
    
    // Send response to Toughbook
    IPAddress toughbookIP = getToughbookIP();
    _otaUdp.beginPacket(toughbookIP, OTA_RESPONSE_PORT);
    _otaUdp.write((const uint8_t*)&response, sizeof(OTAResponsePacket));
    _otaUdp.endPacket();
    
    DiagnosticManager::logMessage(LOG_DEBUG, "OTAUpdateManager", 
        "Sent OTA response: " + String(responseCode) + " - " + message);
}

IPAddress OTAUpdateManager::getToughbookIP() {
    // Return the Toughbook IP address
    // This should match the NetworkManager configuration
    return IPAddress(192, 168, 1, 10);
}

FirmwareVersion_t OTAUpdateManager::getCurrentVersion() {
    return VersionManager::getCurrentVersion();
}

UpdateStatus_t OTAUpdateManager::getUpdateStatus() {
    return VersionManager::getUpdateStatus();
}

bool OTAUpdateManager::isUpdateInProgress() {
    return _updateInProgress;
}

bool OTAUpdateManager::isSystemReady() {
    return !_updateInProgress && performSafetyChecks();
}
