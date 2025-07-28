#include "VersionManager.h"
#include "DiagnosticManager.h"
#include "ModuleConfig.h"

// Static member definitions
const FirmwareVersion_t VersionManager::CURRENT_VERSION = {
    FIRMWARE_VERSION_MAJOR,
    FIRMWARE_VERSION_MINOR,
    FIRMWARE_VERSION_PATCH,
    FIRMWARE_BUILD_NUMBER,
    FIRMWARE_BUILD_DATE,
    FIRMWARE_GIT_HASH
};

UpdateStatus_t VersionManager::_updateStatus = UPDATE_IDLE;
uint8_t VersionManager::_updateProgress = 0;
String VersionManager::_updateError = "";
uint32_t VersionManager::_lastProgressUpdate = 0;

void VersionManager::initialize() {
    DiagnosticManager::logMessage(LOG_INFO, "VersionManager", "Initializing version management system");
    
    String versionInfo = "Firmware Version: " + getCurrentVersionString();
    DiagnosticManager::logMessage(LOG_INFO, "VersionManager", versionInfo);
    
    // Reset update status
    _updateStatus = UPDATE_IDLE;
    _updateProgress = 0;
    _updateError = "";
    _lastProgressUpdate = millis();
    
    Serial.println("=== FIRMWARE VERSION INFO ===");
    Serial.println("Version: " + getCurrentVersionString());
    Serial.println("Build Date: " + String(CURRENT_VERSION.buildDate));
    Serial.println("Git Hash: " + String(CURRENT_VERSION.gitHash));
    Serial.println("Build Number: " + String(CURRENT_VERSION.buildNumber));
    Serial.println("==============================");
}

FirmwareVersion_t VersionManager::getCurrentVersion() {
    return CURRENT_VERSION;
}

String VersionManager::getVersionString(const FirmwareVersion_t& version) {
    String versionStr = "v" + String(version.major) + "." + 
                       String(version.minor) + "." + 
                       String(version.patch);
    
    if (version.buildNumber > 0) {
        versionStr += "-build" + String(version.buildNumber);
    }
    
    versionStr += " (" + String(version.buildDate) + ")";
    
    if (strlen(version.gitHash) > 0 && strcmp(version.gitHash, "dev") != 0) {
        versionStr += " [" + String(version.gitHash) + "]";
    }
    
    return versionStr;
}

String VersionManager::getCurrentVersionString() {
    return getVersionString(CURRENT_VERSION);
}

bool VersionManager::isVersionNewer(const FirmwareVersion_t& current, const FirmwareVersion_t& available) {
    return compareVersions(current, available) < 0;
}

bool VersionManager::areVersionsEqual(const FirmwareVersion_t& v1, const FirmwareVersion_t& v2) {
    return compareVersions(v1, v2) == 0;
}

int VersionManager::compareVersions(const FirmwareVersion_t& v1, const FirmwareVersion_t& v2) {
    // Compare major version
    if (v1.major != v2.major) {
        return (v1.major < v2.major) ? -1 : 1;
    }
    
    // Compare minor version
    if (v1.minor != v2.minor) {
        return (v1.minor < v2.minor) ? -1 : 1;
    }
    
    // Compare patch version
    if (v1.patch != v2.patch) {
        return (v1.patch < v2.patch) ? -1 : 1;
    }
    
    // Compare build number
    if (v1.buildNumber != v2.buildNumber) {
        return (v1.buildNumber < v2.buildNumber) ? -1 : 1;
    }
    
    // Versions are equal
    return 0;
}

void VersionManager::setUpdateStatus(UpdateStatus_t status, uint8_t progress) {
    _updateStatus = status;
    _updateProgress = progress;
    _lastProgressUpdate = millis();
    
    // Log status changes
    String statusStr;
    switch (status) {
        case UPDATE_IDLE:        statusStr = "IDLE"; break;
        case UPDATE_DOWNLOADING: statusStr = "DOWNLOADING"; break;
        case UPDATE_VERIFYING:   statusStr = "VERIFYING"; break;
        case UPDATE_FLASHING:    statusStr = "FLASHING"; break;
        case UPDATE_REBOOTING:   statusStr = "REBOOTING"; break;
        case UPDATE_SUCCESS:     statusStr = "SUCCESS"; break;
        case UPDATE_FAILED:      statusStr = "FAILED"; break;
        case UPDATE_ROLLBACK:    statusStr = "ROLLBACK"; break;
        default:                 statusStr = "UNKNOWN"; break;
    }
    
    String logMsg = "Update status: " + statusStr + " (" + String(progress) + "%)";
    DiagnosticManager::logMessage(LOG_INFO, "VersionManager", logMsg);
    
    // Update diagnostic display
    if (status != UPDATE_IDLE) {
        String displayMsg = "OTA Update: " + statusStr;
        if (progress > 0) {
            displayMsg += " " + String(progress) + "%";
        }
        DiagnosticManager::setSystemStatus(displayMsg);
    }
}

UpdateStatus_t VersionManager::getUpdateStatus() {
    return _updateStatus;
}

uint8_t VersionManager::getUpdateProgress() {
    return _updateProgress;
}

void VersionManager::setUpdateError(const String& error) {
    _updateError = error;
    DiagnosticManager::logError("VersionManager", "Update error: " + error);
    setUpdateStatus(UPDATE_FAILED, 0);
}

String VersionManager::getUpdateError() {
    return _updateError;
}

void VersionManager::reportUpdateProgress(UpdateStatus_t status, uint8_t progress, const String& message) {
    setUpdateStatus(status, progress);
    
    if (message.length() > 0) {
        DiagnosticManager::logMessage(LOG_INFO, "VersionManager", message);
    }
    
    // Send progress packet via network (if available)
    // This will be handled by NetworkManager when OTA is active
}

UpdateProgressPacket VersionManager::getProgressPacket() {
    UpdateProgressPacket packet;
    
    // Set sender ID based on module role
    switch (ModuleConfig::getRole()) {
        case MODULE_LEFT:   packet.SenderId = 0; break;
        case MODULE_CENTRE: packet.SenderId = 1; break;
        case MODULE_RIGHT:  packet.SenderId = 2; break;
        default:            packet.SenderId = 255; break;
    }
    
    packet.Timestamp = millis();
    packet.Status = _updateStatus;
    packet.ProgressPercent = _updateProgress;
    packet.BytesReceived = 0; // Will be set by OTA handler
    packet.TotalBytes = 0;    // Will be set by OTA handler
    
    // Copy error message
    strncpy(packet.ErrorMessage, _updateError.c_str(), sizeof(packet.ErrorMessage) - 1);
    packet.ErrorMessage[sizeof(packet.ErrorMessage) - 1] = '\0';
    
    return packet;
}

uint32_t VersionManager::getCurrentBuildNumber() {
    return FIRMWARE_BUILD_NUMBER;
}

void VersionManager::getCurrentBuildDate(char* buffer, size_t bufferSize) {
    strncpy(buffer, FIRMWARE_BUILD_DATE, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}

void VersionManager::getCurrentGitHash(char* buffer, size_t bufferSize) {
    strncpy(buffer, FIRMWARE_GIT_HASH, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}
