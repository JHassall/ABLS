# ABLS Firmware Technical Reference - Part 3
## Firmware Update System, Logic Flow, and Integration Guidelines

**Version:** 1.0  
**Date:** July 29, 2025  
**Authors:** ABLS Development Team  
**Document Type:** Technical Reference Manual  

---

## Table of Contents

9. [Firmware Update System (RgFModuleUpdate)](#9-firmware-update-system-rgfmoduleupdate)
10. [System Logic Flow and State Management](#10-system-logic-flow-and-state-management)
11. [Integration Guidelines](#11-integration-guidelines)
12. [Troubleshooting and Diagnostics](#12-troubleshooting-and-diagnostics)

---

## 9. Firmware Update System (RgFModuleUpdate)

### 9.1 Architecture Overview

The RgFModuleUpdate system provides enterprise-grade over-the-air (OTA) firmware updates for all ABLS Teensy 4.1 modules, ensuring synchronized operation and field maintainability.

#### 9.1.1 Core Components
```cpp
// Update system architecture
class UpdateManager {
private:
    VersionManager _versionManager;
    FlashBackupManager _flashManager;
    UpdateSafetyManager _safetyManager;
    NetworkManager* _networkManager;
    
    // Update state
    bool _updateInProgress;
    String _pendingVersion;
    uint32_t _updateStartTime;
    
public:
    bool initialize();
    void update();
    
    // Update workflow
    bool startUpdate(const String& firmwareUrl, const String& version);
    bool downloadFirmware(const String& url);
    bool verifyFirmware(const String& filePath);
    bool installFirmware();
    bool rollbackFirmware();
    
    // Status reporting
    String getCurrentVersion();
    bool isUpdateInProgress();
    UpdateStatus getUpdateStatus();
};
```

#### 9.1.2 Dual-Bank Flash Architecture
```cpp
// Teensy 4.1 dual-bank flash management
class FlashBackupManager {
private:
    static const uint32_t FLASH_SIZE = 0x800000;      // 8MB total
    static const uint32_t BANK_SIZE = 0x400000;       // 4MB per bank
    static const uint32_t BANK_A_START = 0x60000000;  // Primary bank
    static const uint32_t BANK_B_START = 0x60400000;  // Backup bank
    
    bool _backupValid;
    String _backupVersion;
    
public:
    // Backup operations
    bool createBackup();
    bool restoreFromBackup();
    bool verifyBackup();
    
    // Flash operations
    bool eraseBank(uint8_t bank);
    bool writeToBank(uint8_t bank, const uint8_t* data, size_t length);
    bool verifyBank(uint8_t bank, const uint8_t* expectedData, size_t length);
    
    // Safety checks
    bool isBackupValid() const { return _backupValid; }
    String getBackupVersion() const { return _backupVersion; }
    
    // Low-level flash interface
    bool flashWrite(uint32_t address, const uint8_t* data, size_t length);
    bool flashErase(uint32_t address, size_t length);
    bool flashVerify(uint32_t address, const uint8_t* data, size_t length);
};
```

### 9.2 Update Workflow and Safety

#### 9.2.1 Sequential Update Protocol
```cpp
// Toughbook-initiated sequential update workflow
enum class UpdatePhase {
    IDLE = 0,
    SAFETY_CHECK = 1,
    BACKUP_CREATION = 2,
    FIRMWARE_DOWNLOAD = 3,
    INTEGRITY_VERIFICATION = 4,
    FLASH_PROGRAMMING = 5,
    VERIFICATION = 6,
    REBOOT = 7,
    VERSION_CONFIRMATION = 8,
    COMPLETE = 9,
    ROLLBACK = 10,
    FAILED = 11
};

class UpdateSafetyManager {
private:
    bool _machineStationary;
    bool _hydraulicsIdle;
    bool _safeToUpdate;
    uint32_t _lastSafetyCheck;
    
public:
    bool performSafetyChecks() {
        // Check machine is stationary (GPS speed < 0.1 m/s)
        if (!checkMachineStationary()) {
            DiagnosticManager::logWarning("Update", "Machine not stationary");
            return false;
        }
        
        // Check hydraulics are idle (Centre module only)
        if (_moduleRole == ModuleRole::CENTRE && !checkHydraulicsIdle()) {
            DiagnosticManager::logWarning("Update", "Hydraulics active");
            return false;
        }
        
        // Check no other updates in progress
        if (_networkManager->isUpdateInProgress()) {
            DiagnosticManager::logWarning("Update", "Update already in progress");
            return false;
        }
        
        _safeToUpdate = true;
        _lastSafetyCheck = millis();
        return true;
    }
    
private:
    bool checkMachineStationary() {
        // Get GPS ground speed
        float groundSpeed = _sensorManager->getGroundSpeed();
        return groundSpeed < 0.1;  // m/s
    }
    
    bool checkHydraulicsIdle() {
        // Check if rams are at target positions
        return _hydraulicController->isAtTarget(1.0);  // 1% tolerance
    }
};
```

### 9.3 Version Management and Reporting

#### 9.3.1 Version Information Structure
```cpp
struct VersionInfo {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint16_t build;
    String gitCommit;       // Git commit hash (8 chars)
    String buildDate;       // ISO 8601 format
    String buildBy;         // Builder identifier
    
    String toString() const {
        return String(major) + "." + String(minor) + "." + 
               String(patch) + "." + String(build) + 
               " (" + gitCommit + ")";
    }
    
    bool isNewerThan(const VersionInfo& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        if (patch != other.patch) return patch > other.patch;
        return build > other.build;
    }
};
```

---

## 10. System Logic Flow and State Management

### 10.1 Main Loop Architecture

#### 10.1.1 Primary System Loop
```cpp
void loop() {
    static uint32_t lastSensorUpdate = 0;
    static uint32_t lastNetworkUpdate = 0;
    static uint32_t lastDiagnosticUpdate = 0;
    static uint32_t lastDisplayUpdate = 0;
    
    uint32_t currentTime = millis();
    
    // High-frequency sensor updates (50Hz)
    if (currentTime - lastSensorUpdate >= 20) {
        sensorManager.update();
        lastSensorUpdate = currentTime;
    }
    
    // Network communication (25Hz)
    if (currentTime - lastNetworkUpdate >= 40) {
        networkManager.update();
        
        // Send sensor data packet
        SensorDataPacket packet;
        sensorManager.populateDataPacket(&packet);
        networkManager.sendSensorData(packet);
        
        lastNetworkUpdate = currentTime;
    }
    
    // Hydraulic control (Centre module, 100Hz)
    if (_moduleRole == ModuleRole::CENTRE && 
        currentTime - lastHydraulicUpdate >= 10) {
        hydraulicController.update();
        lastHydraulicUpdate = currentTime;
    }
    
    // Diagnostic and health monitoring (1Hz)
    if (currentTime - lastDiagnosticUpdate >= 1000) {
        diagnosticManager.update();
        updateManager.update();
        
        // System health reporting
        SystemHealth health = diagnosticManager.getSystemHealth();
        networkManager.sendDiagnosticData(health);
        
        lastDiagnosticUpdate = currentTime;
    }
    
    // Display updates (2Hz)
    if (currentTime - lastDisplayUpdate >= 500) {
        displayManager.update();
        lastDisplayUpdate = currentTime;
    }
    
    // Watchdog reset
    resetWatchdog();
}
```

### 10.2 State Machine Management

#### 10.2.1 System State Definitions
```cpp
enum class SystemState {
    INITIALIZING = 0,
    CALIBRATING = 1,
    OPERATIONAL = 2,
    DEGRADED = 3,      // Some sensors failed
    EMERGENCY = 4,     // Emergency stop active
    UPDATING = 5,      // Firmware update in progress
    FAILED = 6         // Critical failure
};

class SystemStateManager {
private:
    SystemState _currentState;
    SystemState _previousState;
    uint32_t _stateChangeTime;
    String _stateChangeReason;
    
public:
    void initialize() {
        _currentState = SystemState::INITIALIZING;
        _previousState = SystemState::INITIALIZING;
        _stateChangeTime = millis();
        _stateChangeReason = "System startup";
    }
    
    void updateState() {
        SystemState newState = determineSystemState();
        
        if (newState != _currentState) {
            transitionToState(newState);
        }
    }
    
private:
    SystemState determineSystemState() {
        // Check for firmware update in progress
        if (_updateManager->isUpdateInProgress()) {
            return SystemState::UPDATING;
        }
        
        // Check for emergency conditions
        if (_hydraulicController && _hydraulicController->isEmergencyStopped()) {
            return SystemState::EMERGENCY;
        }
        
        // Check for critical failures
        if (hasCriticalFailures()) {
            return SystemState::FAILED;
        }
        
        // Check for calibration mode
        if (_calibrationManager->isCalibrationInProgress()) {
            return SystemState::CALIBRATING;
        }
        
        // Check for degraded operation
        if (hasSensorFailures()) {
            return SystemState::DEGRADED;
        }
        
        // Normal operation
        if (allSystemsOperational()) {
            return SystemState::OPERATIONAL;
        }
        
        // Still initializing
        return SystemState::INITIALIZING;
    }
};
```

---

## 11. Integration Guidelines

### 11.1 ABLS.Core Integration

#### 11.1.1 Toughbook Integration Architecture
```cpp
// ABLS.Core integration points
namespace ABLS {
    namespace Integration {
        
        // Module communication interface
        class ModuleInterface {
        public:
            // Module discovery and status
            virtual std::vector<ModuleInfo> discoverModules() = 0;
            virtual ModuleStatus getModuleStatus(uint8_t moduleId) = 0;
            virtual bool isModuleOnline(uint8_t moduleId) = 0;
            
            // Sensor data reception
            virtual void onSensorDataReceived(const SensorDataPacket& packet) = 0;
            virtual void onDiagnosticDataReceived(const SystemHealth& health) = 0;
            
            // Control command transmission
            virtual bool sendControlCommand(const ControlCommandPacket& command) = 0;
            virtual bool sendCalibrationCommand(const String& command) = 0;
            
            // Firmware update management
            virtual bool initiateModuleUpdate(uint8_t moduleId, const String& firmwareVersion) = 0;
            virtual UpdateStatus getUpdateStatus(uint8_t moduleId) = 0;
        };
    }
}
```

### 11.2 Field Deployment Guidelines

#### 11.2.1 Installation Checklist
```markdown
# ABLS Module Installation Checklist

## Pre-Installation
- [ ] Verify all modules have matching firmware versions
- [ ] Test all modules on bench with calibration app
- [ ] Prepare installation tools and mounting hardware
- [ ] Document boom geometry and hydraulic specifications

## Physical Installation
- [ ] Mount modules in weatherproof enclosures
- [ ] Connect GPS antennas with clear sky view
- [ ] Install IMU sensors aligned with boom axis
- [ ] Mount radar sensors pointing downward
- [ ] Connect hydraulic ram potentiometers
- [ ] Install DIP switches for role configuration
- [ ] Connect Ethernet cables to Toughbook

## DIP Switch Configuration
- [ ] Left Wing Module: Pin 2 to GND
- [ ] Centre Module: Pin 3 to GND  
- [ ] Right Wing Module: Pin 4 to GND
- [ ] Verify only one pin per module is grounded

## Network Configuration
- [ ] Configure Toughbook Ethernet adapter
- [ ] Verify DHCP or static IP assignments
- [ ] Test UDP communication on all ports
- [ ] Verify RTCM correction distribution

## Sensor Calibration
- [ ] Run IMU calibration app for each module
- [ ] Perform manual level reference setting
- [ ] Capture multi-sensor calibration data
- [ ] Verify sensor agreement within tolerance
- [ ] Save calibration data to modules

## System Validation
- [ ] Verify all modules report online status
- [ ] Test hydraulic control response
- [ ] Validate GPS RTK fix acquisition
- [ ] Test emergency stop functionality
- [ ] Perform field operation test
```

---

## 12. Troubleshooting and Diagnostics

### 12.1 Common Issues and Solutions

#### 12.1.1 Network Communication Issues
```cpp
// Network troubleshooting utilities
class NetworkDiagnostics {
public:
    struct NetworkDiagnosticResult {
        bool ethernetLinkUp;
        bool dhcpAssigned;
        IPAddress assignedIP;
        bool canPingToughbook;
        bool udpPortsOpen;
        uint32_t packetLossRate;
        String diagnosis;
        String recommendation;
    };
    
    NetworkDiagnosticResult performNetworkDiagnostics() {
        NetworkDiagnosticResult result = {};
        
        // Check Ethernet link
        result.ethernetLinkUp = Ethernet.linkStatus() == LinkON;
        if (!result.ethernetLinkUp) {
            result.diagnosis = "No Ethernet link detected";
            result.recommendation = "Check cable connections and network switch";
            return result;
        }
        
        // Check IP assignment
        result.assignedIP = Ethernet.localIP();
        result.dhcpAssigned = (result.assignedIP != IPAddress(0, 0, 0, 0));
        if (!result.dhcpAssigned) {
            result.diagnosis = "No IP address assigned";
            result.recommendation = "Check DHCP server or configure static IP";
            return result;
        }
        
        return result;
    }
};
```

### 12.2 Performance Monitoring

#### 12.2.1 System Performance Metrics
```cpp
struct PerformanceMetrics {
    // Timing metrics
    uint32_t loopFrequency;        // Hz
    uint32_t maxLoopTime;          // microseconds
    uint32_t avgLoopTime;          // microseconds
    
    // Memory usage
    uint32_t freeRAM;              // bytes
    uint32_t maxRAMUsage;          // bytes
    
    // Network performance
    uint32_t packetsPerSecond;
    uint32_t bytesPerSecond;
    uint32_t networkLatency;       // milliseconds
    
    // Sensor performance
    uint32_t gpsUpdateRate;        // Hz
    uint32_t imuUpdateRate;        // Hz
    uint32_t radarUpdateRate;      // Hz
    
    // System health
    float cpuUsage;                // percentage
    float temperature;             // Celsius
    uint32_t uptime;               // seconds
};
```

---

**This completes the comprehensive ABLS Firmware Technical Reference documentation. The three-part document provides complete coverage of the system architecture, implementation details, and integration guidelines for the ABLS agricultural boom control system.**
