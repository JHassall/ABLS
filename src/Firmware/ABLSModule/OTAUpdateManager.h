#ifndef OTA_UPDATE_MANAGER_H
#define OTA_UPDATE_MANAGER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "VersionManager.h"
#include "DataPackets.h"

using namespace qindesign::network;

// =================================================================
// OTA Update Manager for ABLS Unified Firmware
// =================================================================
// Handles Over-The-Air firmware updates using FlasherX approach
// with Teensy 4.1 dual-bank flash for rollback capability
// =================================================================

// OTA Update command packet from Toughbook
struct OTACommandPacket {
    uint32_t CommandId;               // Unique command identifier
    uint32_t Timestamp;               // Command timestamp
    uint8_t TargetModuleId;           // Module to update (0=Left, 1=Centre, 2=Right, 255=All)
    uint8_t Command;                  // OTA command type
    FirmwareVersion_t NewVersion;     // Version information for new firmware
    uint32_t FirmwareSize;            // Size of firmware binary in bytes
    char DownloadUrl[128];            // HTTP URL for firmware download
    uint32_t Checksum;                // CRC32 checksum for verification
};

// OTA command types
typedef enum {
    OTA_CMD_CHECK_VERSION = 1,        // Query current version
    OTA_CMD_START_UPDATE = 2,         // Begin update process
    OTA_CMD_CANCEL_UPDATE = 3,        // Cancel ongoing update
    OTA_CMD_ROLLBACK = 4,             // Rollback to previous version
    OTA_CMD_REBOOT = 5                // Reboot module
} OTACommand_t;

// OTA response packet to Toughbook
struct OTAResponsePacket {
    uint32_t CommandId;               // Original command ID
    uint32_t Timestamp;               // Response timestamp
    uint8_t SenderId;                 // Responding module ID
    uint8_t ResponseCode;             // Response status code
    FirmwareVersion_t CurrentVersion; // Current firmware version
    UpdateProgressPacket Progress;   // Current update progress
    char Message[64];                 // Status/error message
};

// OTA response codes
typedef enum {
    OTA_RESPONSE_OK = 0,              // Command accepted/successful
    OTA_RESPONSE_BUSY = 1,            // Update already in progress
    OTA_RESPONSE_ERROR = 2,           // Command failed
    OTA_RESPONSE_INVALID = 3,         // Invalid command/parameters
    OTA_RESPONSE_NOT_READY = 4        // System not ready for update
} OTAResponseCode_t;

class OTAUpdateManager {
public:
    // Initialization
    static bool initialize();
    static void update();
    
    // Update control
    static bool startUpdate(const OTACommandPacket& command);
    static bool cancelUpdate();
    static bool rollbackFirmware();
    static void rebootModule();
    
    // Status and version reporting
    static FirmwareVersion_t getCurrentVersion();
    static UpdateStatus_t getUpdateStatus();
    static bool isUpdateInProgress();
    static bool isSystemReady();
    
    // Network communication
    static void processOTACommand(const OTACommandPacket& command);
    static void sendOTAResponse(const OTACommandPacket& originalCommand, 
                               OTAResponseCode_t responseCode, 
                               const String& message = "");
    
    // Safety checks
    static bool performSafetyChecks();
    static bool isSystemStationary();
    static bool areAllSystemsHealthy();
    
    // Firmware download and verification
    static bool downloadFirmware(const String& url, uint32_t expectedSize, uint32_t expectedChecksum);
    static bool verifyFirmware(uint32_t expectedChecksum);
    static bool flashFirmware();
    
    // Dual-bank flash management
    static bool backupCurrentFirmware();
    static bool restoreFromBackup();
    static bool validateFirmwareBank(uint8_t bankNumber);
    
    // Progress reporting
    static void reportProgress(UpdateStatus_t status, uint8_t progress, const String& message = "");
    static UpdateProgressPacket getProgressPacket();
    
    // Error handling
    static void handleUpdateError(const String& error);
    static void cleanup();
    
private:
    // Network components
    static EthernetUDP _otaUdp;
    static bool _networkInitialized;
    
    // Update state
    static bool _updateInProgress;
    static OTACommandPacket _currentCommand;
    static uint32_t _updateStartTime;
    static uint32_t _lastProgressReport;
    
    // Firmware buffer management
    static uint8_t* _firmwareBuffer;
    static uint32_t _firmwareBufferSize;
    static uint32_t _bytesReceived;
    static uint32_t _expectedSize;
    static uint32_t _expectedChecksum;
    
    // Safety state
    static bool _safetyChecksEnabled;
    static uint32_t _lastSafetyCheck;
    
    // HTTP download helpers
    static bool initializeHttpClient();
    static bool downloadChunk(const String& url, uint32_t offset, uint32_t chunkSize);
    static uint32_t calculateCRC32(const uint8_t* data, uint32_t length);
    
    // Flash management helpers
    static bool eraseFlashBank(uint8_t bankNumber);
    static bool writeFlashBank(uint8_t bankNumber, const uint8_t* data, uint32_t size);
    static bool switchToBank(uint8_t bankNumber);
    
    // Network helpers
    static void setupOTASocket();
    static void processIncomingOTACommands();
    static IPAddress getToughbookIP();
    
    // Constants
    static const uint16_t OTA_COMMAND_PORT = 8004;
    static const uint16_t OTA_RESPONSE_PORT = 8005;
    static const uint32_t MAX_FIRMWARE_SIZE = 2 * 1024 * 1024; // 2MB max
    static const uint32_t DOWNLOAD_CHUNK_SIZE = 1024;          // 1KB chunks
    static const uint32_t SAFETY_CHECK_INTERVAL = 5000;       // 5 seconds
    static const uint32_t PROGRESS_REPORT_INTERVAL = 1000;    // 1 second
};

#endif // OTA_UPDATE_MANAGER_H
