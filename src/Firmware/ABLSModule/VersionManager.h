#ifndef VERSION_MANAGER_H
#define VERSION_MANAGER_H

#include <Arduino.h>

// =================================================================
// Version Management System for ABLS Unified Firmware
// =================================================================
// Handles firmware version tracking, comparison, and OTA updates
// =================================================================

// Firmware version structure
typedef struct {
    uint16_t major;           // Major version (e.g., 1)
    uint16_t minor;           // Minor version (e.g., 2)
    uint16_t patch;           // Patch version (e.g., 3)
    uint32_t buildNumber;     // Build number (timestamp or sequential)
    char buildDate[16];       // Build date string "YYYY-MM-DD"
    char gitHash[8];          // Short git commit hash
} FirmwareVersion_t;

// Update status enumeration
typedef enum {
    UPDATE_IDLE = 0,
    UPDATE_DOWNLOADING,
    UPDATE_VERIFYING,
    UPDATE_FLASHING,
    UPDATE_REBOOTING,
    UPDATE_SUCCESS,
    UPDATE_FAILED,
    UPDATE_ROLLBACK
} UpdateStatus_t;

// Update progress packet for network communication
struct UpdateProgressPacket {
    uint8_t SenderId;                 // Module being updated
    uint32_t Timestamp;               // Update timestamp
    UpdateStatus_t Status;            // Current update status
    uint8_t ProgressPercent;          // 0-100% progress
    uint32_t BytesReceived;           // Bytes downloaded
    uint32_t TotalBytes;              // Total firmware size
    char ErrorMessage[64];            // Error description if failed
};

class VersionManager {
public:
    // Version management
    static FirmwareVersion_t getCurrentVersion();
    static String getVersionString(const FirmwareVersion_t& version);
    static String getCurrentVersionString();
    
    // Version comparison
    static bool isVersionNewer(const FirmwareVersion_t& current, const FirmwareVersion_t& available);
    static bool areVersionsEqual(const FirmwareVersion_t& v1, const FirmwareVersion_t& v2);
    static int compareVersions(const FirmwareVersion_t& v1, const FirmwareVersion_t& v2);
    
    // Update status management
    static void setUpdateStatus(UpdateStatus_t status, uint8_t progress = 0);
    static UpdateStatus_t getUpdateStatus();
    static uint8_t getUpdateProgress();
    static void setUpdateError(const String& error);
    static String getUpdateError();
    
    // Update progress reporting
    static void reportUpdateProgress(UpdateStatus_t status, uint8_t progress, const String& message = "");
    static UpdateProgressPacket getProgressPacket();
    
    // Initialization
    static void initialize();
    
private:
    // Current firmware version (compile-time constants)
    static const FirmwareVersion_t CURRENT_VERSION;
    
    // Update status tracking
    static UpdateStatus_t _updateStatus;
    static uint8_t _updateProgress;
    static String _updateError;
    static uint32_t _lastProgressUpdate;
    
    // Helper functions
    static uint32_t getCurrentBuildNumber();
    static void getCurrentBuildDate(char* buffer, size_t bufferSize);
    static void getCurrentGitHash(char* buffer, size_t bufferSize);
};

// Compile-time version macros (set during build)
#ifndef FIRMWARE_VERSION_MAJOR
#define FIRMWARE_VERSION_MAJOR 1
#endif

#ifndef FIRMWARE_VERSION_MINOR
#define FIRMWARE_VERSION_MINOR 0
#endif

#ifndef FIRMWARE_VERSION_PATCH
#define FIRMWARE_VERSION_PATCH 0
#endif

#ifndef FIRMWARE_BUILD_NUMBER
#define FIRMWARE_BUILD_NUMBER 1
#endif

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "2025-01-29"
#endif

#ifndef FIRMWARE_GIT_HASH
#define FIRMWARE_GIT_HASH "dev"
#endif

#endif // VERSION_MANAGER_H
