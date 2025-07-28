# ABLS OTA Firmware Update System Plan

## Overview
This document outlines the comprehensive Over-the-Air (OTA) firmware update system for the ABLS project, covering both Teensy module firmware and Toughbook control logic requirements.

## 1. System Architecture

### 1.1 Update Flow Overview
```
User Initiates Update (Toughbook) → Safety Checks → Sequential Module Updates → Version Verification → System Re-enable
```

### 1.2 Key Safety Requirements
- **Machine must be stationary** before updates can begin
- **Sequential updates** - one module at a time, never simultaneous
- **Version synchronization** - all modules must run matching firmware versions
- **Rollback capability** - restore previous version if any module fails
- **Startup verification** - version and status check before operational enable

## 2. Firmware Version Management

### 2.1 Version Information Structure
```cpp
typedef struct {
    uint16_t major;           // Major version (e.g., 1)
    uint16_t minor;           // Minor version (e.g., 2)
    uint16_t patch;           // Patch version (e.g., 3)
    uint32_t buildNumber;     // Build number (timestamp or sequential)
    char buildDate[16];       // Build date string "YYYY-MM-DD"
    char gitHash[8];          // Short git commit hash
} FirmwareVersion_t;

// Example: v1.2.3-build4567 (2024-01-15) [a1b2c3d4]
```

### 2.2 Version Comparison Logic
```cpp
class VersionManager {
public:
    static bool isVersionNewer(const FirmwareVersion_t& current, const FirmwareVersion_t& available);
    static bool areVersionsEqual(const FirmwareVersion_t& v1, const FirmwareVersion_t& v2);
    static String getVersionString(const FirmwareVersion_t& version);
    static FirmwareVersion_t getCurrentVersion();
    
private:
    static const FirmwareVersion_t CURRENT_VERSION;
};
```

### 2.3 Version Storage and Persistence
- Version information stored in EEPROM/Flash for persistence across reboots
- Backup version information maintained for rollback capability
- Version reported in every network packet for monitoring

### 2.4 Firmware Rollback Architecture
The Teensy 4.1's 8MB flash memory enables a **dual-bank firmware storage** approach:

```cpp
class FlashBackupManager {
public:
    static bool backupCurrentFirmware();     // Copy current to backup bank before update
    static bool restoreFromBackup();         // Rollback: copy backup to current bank
    static bool isBackupValid();             // Verify backup firmware integrity
    static FirmwareVersion_t getBackupVersion(); // Get version of backup firmware
    
private:
    static const uint32_t CURRENT_FIRMWARE_BASE = 0x60000000; // Bank A: Current firmware
    static const uint32_t BACKUP_FIRMWARE_BASE  = 0x60400000; // Bank B: Backup firmware  
    static const uint32_t FIRMWARE_MAX_SIZE     = 0x400000;   // 4MB per bank
};
```

**Rollback Process:**
1. **Before any update**: Current firmware is backed up to Bank B
2. **New firmware installed**: Written to Bank A (current)
3. **If rollback needed**: Backup firmware (Bank B) is copied back to Bank A
4. **Module reboots**: Now running previous firmware version

**Benefits:**
- ✅ **Fast rollback** - no network download required (seconds vs minutes)
- ✅ **Network independent** - works even if Toughbook is offline
- ✅ **Reliable recovery** - no dependency on network connectivity
- ✅ **Automatic backup** - previous version always preserved

## 3. Network Protocol Extensions

### 3.1 Version Status Packet (Startup & Periodic)
```cpp
typedef struct {
    String PacketType = "VERSION_STATUS";
    String SenderId;                    // "LEFT", "CENTRE", "RIGHT"
    FirmwareVersion_t CurrentVersion;   // Currently running version
    FirmwareVersion_t BackupVersion;    // Previous version (for rollback)
    String OperationalStatus;           // "READY", "UPDATING", "ERROR", "ROLLBACK"
    uint32_t UptimeSeconds;            // Time since last reboot
    String LastUpdateResult;           // "SUCCESS", "FAILED", "NONE"
    uint32_t LastUpdateTimestamp;      // Unix timestamp of last update
} VersionStatusPacket;
```

### 3.2 Firmware Update Command Packet
```cpp
typedef struct {
    String PacketType = "FIRMWARE_UPDATE_CMD";
    String TargetModule;               // "LEFT", "CENTRE", "RIGHT", "ALL"
    FirmwareVersion_t NewVersion;      // Version being deployed
    String FirmwareURL;                // HTTP URL for firmware download
    String FirmwareHash;               // SHA256 hash for verification
    uint32_t FirmwareSize;             // File size in bytes
    bool ForceUpdate;                  // Override version checks
    uint32_t UpdateTimeout;            // Timeout in seconds
} FirmwareUpdateCommand;
```

### 3.3 Update Progress Packet
```cpp
typedef struct {
    String PacketType = "UPDATE_PROGRESS";
    String SenderId;                   // Module being updated
    String UpdatePhase;                // "DOWNLOADING", "VERIFYING", "FLASHING", "REBOOTING"
    uint8_t ProgressPercent;           // 0-100%
    String Status;                     // "IN_PROGRESS", "SUCCESS", "ERROR"
    String ErrorMessage;               // Error details if Status == "ERROR"
    uint32_t BytesDownloaded;          // Progress tracking
    uint32_t TotalBytes;               // Total firmware size
} UpdateProgressPacket;
```

## 4. Teensy Module Firmware Requirements

### 4.1 OTA Update Manager Class
```cpp
class OTAUpdateManager {
public:
    OTAUpdateManager();
    void init();
    void handleUpdateCommand(const FirmwareUpdateCommand& cmd);
    void sendVersionStatus();
    void sendUpdateProgress(const String& phase, uint8_t percent, const String& status);
    
    // Update state management
    bool isUpdateInProgress();
    bool canAcceptUpdate();
    void abortUpdate();
    
private:
    // Update state
    enum UpdateState {
        IDLE,
        DOWNLOADING,
        VERIFYING,
        UPDATING,
        REBOOTING,
        ERROR
    };
    
    UpdateState _updateState;
    FirmwareUpdateCommand _currentUpdate;
    TeensyOtaUpdater _otaUpdater;
    
    // Update process methods
    bool downloadFirmware(const String& url, const String& expectedHash);
    bool verifyFirmware(const String& expectedHash);
    bool updateFirmware();
    void rebootModule();
    void handleUpdateError(const String& error);
    
    // Version management
    void saveVersionInfo(const FirmwareVersion_t& version);
    FirmwareVersion_t loadVersionInfo();
    void backupCurrentVersion();
    bool rollbackToPreviousVersion();
};
```

### 4.2 Integration with Unified Firmware
```cpp
// In main ABLSModule.ino
OTAUpdateManager otaManager;

void setup() {
    // Standard initialization...
    ModuleConfig::detectRole();
    sensorManager.init();
    networkManager.begin(mac);
    
    // Initialize OTA update capability
    otaManager.init();
    
    // Send version status on startup
    otaManager.sendVersionStatus();
}

void loop() {
    // Standard operation...
    networkManager.update();
    sensorManager.update();
    
    // Handle OTA updates (non-blocking)
    if (networkManager.hasUpdateCommand()) {
        FirmwareUpdateCommand cmd = networkManager.getUpdateCommand();
        otaManager.handleUpdateCommand(cmd);
    }
    
    // Send periodic version status (every 30 seconds)
    static unsigned long lastVersionReport = 0;
    if (millis() - lastVersionReport > 30000) {
        otaManager.sendVersionStatus();
        lastVersionReport = millis();
    }
}
```

### 4.3 Safety and Error Handling
```cpp
class UpdateSafetyManager {
public:
    static bool isSafeToUpdate();
    static void enterUpdateMode();
    static void exitUpdateMode();
    static bool isSystemStationary();
    
private:
    static bool _updateModeActive;
    static unsigned long _lastMotionTime;
};

// Safety checks before accepting updates
bool OTAUpdateManager::canAcceptUpdate() {
    return UpdateSafetyManager::isSafeToUpdate() && 
           _updateState == IDLE &&
           !hydraulicController.isActive(); // Centre module only
}
```

## 5. Toughbook Control Logic Requirements

### 5.1 Update Orchestration Manager
```cpp
public class FirmwareUpdateOrchestrator {
    // Update workflow management
    public async Task<bool> InitiateSystemUpdate(FirmwareVersion newVersion, string firmwarePath);
    public async Task<bool> UpdateSingleModule(ModuleId moduleId, FirmwareVersion newVersion);
    public async Task<bool> VerifyAllModulesUpdated(FirmwareVersion expectedVersion);
    public async Task<bool> RollbackFailedUpdate();
    
    // Safety and status management
    public bool IsSystemStationary();
    public bool CanInitiateUpdate();
    public Dictionary<ModuleId, VersionStatusPacket> GetModuleVersions();
    public UpdateStatus GetUpdateStatus();
    
    // Event handlers
    public event EventHandler<UpdateProgressEventArgs> UpdateProgressChanged;
    public event EventHandler<ModuleStatusEventArgs> ModuleStatusChanged;
    public event EventHandler<UpdateCompletedEventArgs> UpdateCompleted;
}
```

### 5.2 Update Workflow State Machine
```cpp
public enum UpdateWorkflowState {
    Idle,
    PreUpdateChecks,
    UpdatingLeftModule,
    UpdatingCentreModule,
    UpdatingRightModule,
    VerifyingUpdates,
    UpdateComplete,
    UpdateFailed,
    RollingBack,
    RollbackComplete
}

public class UpdateWorkflowStateMachine {
    public UpdateWorkflowState CurrentState { get; private set; }
    public async Task<bool> ProcessState();
    public void HandleModuleUpdateComplete(ModuleId moduleId, bool success);
    public void HandleUpdateError(ModuleId moduleId, string error);
}
```

### 5.3 User Interface Requirements
```cpp
// Update initiation dialog
public partial class FirmwareUpdateDialog : Form {
    // Display current versions of all modules
    // Show available firmware version
    // Safety status indicators (machine stationary, etc.)
    // Update progress bars for each module
    // Error/success status display
    // Retry/Rollback buttons
}

// System startup version check
public partial class SystemStartupDialog : Form {
    // Display version status of all modules
    // Show operational status of each module
    // Version mismatch warnings
    // "System Ready" indicator
    // Manual override for development/testing
}
```

## 6. Update Workflow Detailed Steps

### 6.1 System Startup Version Check
```
1. Toughbook starts → Display "System Initializing" screen
2. Send version status request to all modules
3. Collect version responses (timeout after 10 seconds)
4. Check version consistency across all modules
5. Verify operational status of all modules
6. If all OK → Enable "System Ready" and proceed to main screen
7. If version mismatch → Display warning, require user action
8. If module offline → Display error, prevent system enable
```

### 6.2 User-Initiated Update Workflow
```
1. User clicks "Update Firmware" → Display update dialog
2. Check system safety conditions:
    -User informed that machine must be stationary for update to proceed.
   - Machine is stationary (GPS speed < 0.1 m/s)
   - No active hydraulic operations
   - All modules responding
3. If safe → Enable "Start Update" button
4. User confirms update → Begin sequential update process
5. Update each module in sequence (Left → Centre → Right)
6. For each module:
   a. Send update command with firmware URL
   b. Monitor download progress
   c. Wait for verification complete
   d. Wait for flash complete
   e. Wait for reboot and version confirmation
   f. If success → proceed to next module
   g. If failure → offer retry or rollback
7. After all modules updated → Verify version consistency
8. If all successful → Display "Update Complete", enable system
9. If any failures → Display error, offer rollback option
```

### 6.3 Error Handling and Recovery
```
Update Failure Scenarios:
1. Download failure → Retry download (up to 3 attempts)
2. Verification failure → Re-download firmware
3. Flash failure → Attempt rollback to previous version
4. Module doesn't reboot → Display error, manual intervention required
5. Version mismatch after update → Force rollback
6. Network timeout → Retry communication, then abort if persistent

Rollback Process:
1. Identify successfully updated modules
2. For each updated module, send rollback command
3. Wait for rollback completion and version confirmation
4. Verify all modules back to original version
5. Display rollback status to user
```

## 7. Network Communication Protocol

### 7.1 HTTP Firmware Server (Toughbook)
```cpp
// Toughbook serves firmware files via HTTP
public class FirmwareHttpServer {
    public void StartServer(int port = 8080);
    public void ServeFirmwareFile(string firmwarePath, string version);
    public void LogDownloadProgress(string moduleId, long bytesTransferred);
    public void StopServer();
}

// Example URLs:
// http://192.168.1.100:8080/firmware/ABLS_v1.2.3.hex
// http://192.168.1.100:8080/firmware/ABLS_v1.2.2.hex (rollback)
```

### 7.2 UDP Command/Status Protocol
```cpp
// Toughbook → Module: Update command
FirmwareUpdateCommand cmd = {
    PacketType: "FIRMWARE_UPDATE_CMD",
    TargetModule: "LEFT",
    NewVersion: {1, 2, 3, 4567, "2024-01-15", "a1b2c3d4"},
    FirmwareURL: "http://192.168.1.100:8080/firmware/ABLS_v1.2.3.hex",
    FirmwareHash: "sha256:abc123...",
    FirmwareSize: 524288,
    ForceUpdate: false,
    UpdateTimeout: 300
};

// Module → Toughbook: Progress updates
UpdateProgressPacket progress = {
    PacketType: "UPDATE_PROGRESS",
    SenderId: "LEFT",
    UpdatePhase: "DOWNLOADING",
    ProgressPercent: 45,
    Status: "IN_PROGRESS",
    ErrorMessage: "",
    BytesDownloaded: 235929,
    TotalBytes: 524288
};
```

## 8. Security Considerations

### 8.1 Firmware Verification
- **SHA256 hash verification** of downloaded firmware
- **Digital signatures** (future enhancement)
- **Secure HTTP** (HTTPS) for firmware downloads
- **Version authenticity** checks

### 8.2 Access Control
- **User authentication** required for firmware updates
- **Update logging** for audit trail
- **Emergency abort** capability
- **Manual override** for development/testing

## 9. Testing Strategy

### 9.1 Unit Testing
- Version comparison logic
- Update state machine transitions
- Error handling scenarios
- Network protocol parsing

### 9.2 Integration Testing
- Single module update process
- Multi-module sequential updates
- Rollback functionality
- Network timeout handling

### 9.3 Field Testing
- Real-world update scenarios
- Network reliability testing
- Power failure during update
- User interface usability

## 10. Implementation Phases

### 10.1 Phase 1: Basic OTA Infrastructure
- Integrate FlasherX-Ethernet_Support into unified firmware
- Implement version management and reporting
- Create basic HTTP firmware server on Toughbook
- Test single module updates

### 10.2 Phase 2: Sequential Update Workflow
- Implement update orchestration logic
- Add safety checks and stationary detection
- Create update progress monitoring
- Test multi-module update sequences

### 10.3 Phase 3: Error Handling and Recovery
- Implement rollback functionality
- Add comprehensive error handling
- Create user interface for update management
- Test failure scenarios and recovery

### 10.4 Phase 4: Production Deployment
- Add security features (signatures, HTTPS)
- Implement audit logging
- Create field deployment procedures
- Comprehensive field testing

## 11. Configuration and Deployment

### 11.1 Firmware Build Process
```bash
# Arduino IDE build with version injection
arduino-cli compile --fqbn teensy:avr:teensy41 \
  --build-property "compiler.cpp.extra_flags=-DFIRMWARE_VERSION_MAJOR=1" \
  --build-property "compiler.cpp.extra_flags=-DFIRMWARE_VERSION_MINOR=2" \
  --build-property "compiler.cpp.extra_flags=-DFIRMWARE_VERSION_PATCH=3" \
  --output-dir ./build ABLSModule.ino
```

### 11.2 Toughbook Deployment
- Firmware files stored in dedicated directory
- HTTP server auto-starts with Toughbook application
- Update logs stored for troubleshooting
- Backup firmware versions maintained

### 11.3 Field Procedures
- **Pre-update checklist**: Machine stationary, all modules responding
- **Update monitoring**: Progress tracking, error detection
- **Post-update verification**: Version consistency, operational testing
- **Emergency procedures**: Manual rollback, factory reset

## 12. Benefits and Impact

### 12.1 Operational Benefits
- **Zero downtime updates** - no physical access required
- **Consistent versioning** - all modules always synchronized
- **Rapid bug fixes** - critical issues resolved immediately
- **Feature deployment** - new capabilities rolled out seamlessly

### 12.2 Maintenance Benefits
- **Reduced service calls** - remote problem resolution
- **Proactive updates** - prevent issues before they occur
- **Seasonal optimization** - firmware tuned for crop conditions
- **Fleet management** - centralized update control

### 12.3 Development Benefits
- **Rapid iteration** - test fixes in real field conditions
- **Remote diagnostics** - gather data and deploy solutions
- **A/B testing** - compare firmware versions in field
- **Continuous improvement** - ongoing optimization based on field data

This comprehensive OTA update system will transform the ABLS platform into a highly maintainable, future-proof agricultural solution with enterprise-grade update capabilities.
