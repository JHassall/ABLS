# ABLS Unified Firmware Implementation Plan

## Overview
This document provides a detailed, step-by-step implementation plan for creating the unified ABLS firmware with OTA update capability. Each step includes compilation, testing, and logic evaluation checkpoints to ensure robust development.

## Implementation Philosophy
- **Incremental development**: Build one feature at a time
- **Compile after each step**: Catch errors early
- **Test functionality**: Verify expected behavior
- **Evaluate logic**: Review implementation against requirements
- **Fix issues**: Address problems before proceeding

## Step-by-Step Implementation

### **PHASE 1: Project Foundation**

#### **Step 1.1: Create New ABLSModule Project Structure**
**Objective**: Establish unified project structure and basic framework

**Tasks:**
```
1. Create new directory: src/Firmware/ABLSModule/
2. Create basic file structure:
   - ABLSModule.ino (main sketch)
   - ModuleConfig.h (role detection header)
   - ModuleConfig.cpp (role detection implementation)
   - DataPackets.h (copy from existing project)
   - README.md (configuration guide)
3. Set up basic Arduino sketch structure
4. Add required library includes
```

**Files to Create:**
- `ABLSModule.ino` - Basic setup() and loop() structure
- `ModuleConfig.h` - GPIO pin definitions and role enumeration
- `ModuleConfig.cpp` - Empty class implementation (to be filled in Step 1.2)
- `DataPackets.h` - Copy from BoomWingModule (latest version with RTK fields)

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Clean compilation with no errors
**Logic Evaluation**: Verify project structure is correct and all includes resolve

---

#### **Step 1.2: Implement ModuleConfig Class for Hardware Role Detection**
**Objective**: Create hardware-based role detection system

**Tasks:**
```
1. Implement GPIO pin detection logic
2. Add role enumeration and detection functions
3. Add error handling for invalid configurations
4. Add debug output for role detection
5. Test with simulated pin states (pullup/pulldown)
```

**ModuleConfig.h Implementation:**
```cpp
#ifndef MODULE_CONFIG_H
#define MODULE_CONFIG_H

#include <Arduino.h>

// Hardware configuration detection pins
#define PIN_LEFT_CONFIG     2   // Tie to GND for left wing module
#define PIN_CENTRE_CONFIG   3   // Tie to GND for centre module  
#define PIN_RIGHT_CONFIG    4   // Tie to GND for right wing module

typedef enum {
    MODULE_LEFT = 0,      // Left wing module
    MODULE_CENTRE = 1,    // Centre module (hydraulic control)
    MODULE_RIGHT = 2,     // Right wing module
    MODULE_UNKNOWN = 3    // Error condition - no valid config detected
} ModuleRole_t;

class ModuleConfig {
public:
    static void detectRole();
    static ModuleRole_t getRole();
    static String getRoleName();
    static bool isCentreModule();
    static bool isWingModule();
    static bool isValidConfiguration();
    
private:
    static ModuleRole_t _moduleRole;
    static bool _roleDetected;
    static void handleConfigurationError();
};

#endif // MODULE_CONFIG_H
```

**ModuleConfig.cpp Implementation:**
```cpp
#include "ModuleConfig.h"

// Static member initialization
ModuleRole_t ModuleConfig::_moduleRole = MODULE_UNKNOWN;
bool ModuleConfig::_roleDetected = false;

void ModuleConfig::detectRole() {
    // Configure pins with internal pullup resistors
    pinMode(PIN_LEFT_CONFIG, INPUT_PULLUP);
    pinMode(PIN_CENTRE_CONFIG, INPUT_PULLUP);
    pinMode(PIN_RIGHT_CONFIG, INPUT_PULLUP);
    
    // Small delay to allow pins to stabilize
    delay(10);
    
    // Read pin states (LOW = tied to GND = active)
    bool leftActive = (digitalRead(PIN_LEFT_CONFIG) == LOW);
    bool centreActive = (digitalRead(PIN_CENTRE_CONFIG) == LOW);
    bool rightActive = (digitalRead(PIN_RIGHT_CONFIG) == LOW);
    
    // Count active pins
    int activePins = leftActive + centreActive + rightActive;
    
    // Determine role based on pin states
    if (activePins == 1) {
        if (leftActive) {
            _moduleRole = MODULE_LEFT;
        } else if (centreActive) {
            _moduleRole = MODULE_CENTRE;
        } else if (rightActive) {
            _moduleRole = MODULE_RIGHT;
        }
        _roleDetected = true;
        
        Serial.print("Module Role Detected: ");
        Serial.println(getRoleName());
    } else {
        _moduleRole = MODULE_UNKNOWN;
        _roleDetected = false;
        handleConfigurationError();
    }
}

ModuleRole_t ModuleConfig::getRole() {
    return _moduleRole;
}

String ModuleConfig::getRoleName() {
    switch (_moduleRole) {
        case MODULE_LEFT:   return "LEFT_WING";
        case MODULE_CENTRE: return "CENTRE";
        case MODULE_RIGHT:  return "RIGHT_WING";
        default:           return "UNKNOWN";
    }
}

bool ModuleConfig::isCentreModule() {
    return _moduleRole == MODULE_CENTRE;
}

bool ModuleConfig::isWingModule() {
    return _moduleRole == MODULE_LEFT || _moduleRole == MODULE_RIGHT;
}

bool ModuleConfig::isValidConfiguration() {
    return _roleDetected && _moduleRole != MODULE_UNKNOWN;
}

void ModuleConfig::handleConfigurationError() {
    Serial.println("ERROR: Invalid module configuration detected!");
    Serial.println("Exactly one configuration pin must be tied to GND:");
    Serial.println("  Pin 2 (GND) = Left Wing Module");
    Serial.println("  Pin 3 (GND) = Centre Module");
    Serial.println("  Pin 4 (GND) = Right Wing Module");
    Serial.println("System halted - check hardware configuration.");
    
    // Halt execution - configuration must be fixed
    while(1) {
        delay(1000);
    }
}
```

**ABLSModule.ino Update:**
```cpp
#include "ModuleConfig.h"
#include "DataPackets.h"

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Wait for serial or timeout
    
    Serial.println("ABLS Module Starting...");
    
    // Step 1: Detect hardware role
    ModuleConfig::detectRole();
    
    if (!ModuleConfig::isValidConfiguration()) {
        // Error handling already done in detectRole()
        return;
    }
    
    Serial.print("Module Role: ");
    Serial.println(ModuleConfig::getRoleName());
    Serial.println("Module initialization complete");
}

void loop() {
    // Basic heartbeat
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.print("Heartbeat - Role: ");
        Serial.println(ModuleConfig::getRoleName());
        lastHeartbeat = millis();
    }
    
    delay(100);
}
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Clean compilation with no errors
**Logic Evaluation**: 
- Verify role detection logic handles all scenarios correctly
- Test error handling for invalid configurations
- Confirm debug output is clear and informative

---

#### **Step 1.3: Test ModuleConfig Role Detection Logic**
**Objective**: Verify role detection works correctly in all scenarios

**Test Scenarios:**
1. **No pins tied to GND** → Should detect MODULE_UNKNOWN and halt
2. **Multiple pins tied to GND** → Should detect MODULE_UNKNOWN and halt  
3. **Pin 2 tied to GND** → Should detect MODULE_LEFT
4. **Pin 3 tied to GND** → Should detect MODULE_CENTRE
5. **Pin 4 tied to GND** → Should detect MODULE_RIGHT

**Testing Method** (without hardware):
```cpp
// Add to ModuleConfig.cpp for testing
#ifdef TESTING_MODE
void ModuleConfig::simulateConfiguration(bool left, bool centre, bool right) {
    // Simulate pin states for testing
    bool leftActive = left;
    bool centreActive = centre;
    bool rightActive = right;
    
    // Rest of detection logic...
}
#endif
```

**Expected Results:**
- Valid single-pin configurations detected correctly
- Invalid configurations trigger error handling
- Debug output shows correct role names
- System halts appropriately on configuration errors

**Logic Evaluation Checklist:**
- [ ] All five test scenarios work as expected
- [ ] Error messages are clear and helpful
- [ ] Role detection is deterministic and reliable
- [ ] No memory leaks or undefined behavior

---

### **PHASE 2: Core Feature Integration**

#### **Step 2.1: Merge BoomCentreModule and BoomWingModule Features**
**Objective**: Combine existing sensor management code into unified system

**Tasks:**
```
1. Create unified SensorManager class
2. Copy GPS callback code from BoomWingModule
3. Copy hydraulic control code from BoomCentreModule
4. Add conditional compilation based on module role
5. Merge network management code
```

**Files to Create/Update:**
- `SensorManager.h` - Unified sensor management header
- `SensorManager.cpp` - Combined sensor logic with role-based features
- `NetworkManager.h` - Unified network management header  
- `NetworkManager.cpp` - Combined network logic with role-based features
- `HydraulicController.h` - Centre module only (copy from existing)
- `HydraulicController.cpp` - Centre module only (copy from existing)

**SensorManager.h Structure:**
```cpp
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"
#include "ModuleConfig.h"
#include <SparkFun_BNO080_Arduino_Library.h>
#include <SparkFun_Qwiic_XM125_Arduino_Library.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// RTK Status Enumeration (from BoomWingModule)
typedef enum {
    RTK_NONE = 0,     // Standard GPS (>50cm accuracy)
    RTK_FLOAT = 1,    // RTK Float solution (2cm-50cm accuracy)
    RTK_FIXED = 2     // RTK Fixed solution (<2cm accuracy)
} RTKStatus_t;

class SensorManager {
public:
    SensorManager();
    void init();                    // Role-aware initialization
    void update();                  // Role-specific update logic
    void populatePacket(SensorDataPacket* packet);
    void forwardRtcmToGps(const uint8_t* data, size_t len);
    
    // GPS callback (unified for all modules)
    static void gpsHPPOSLLHCallback(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct);
    
private:
    // Role-specific initialization methods
    void initGPSForRole();
    void initSensorFusion();        // Wing modules only
    void initBasicSensors();        // All modules
    
    // Role-specific update methods
    void updateWingModule();        // Sensor fusion logic
    void updateCentreModule();      // Direct sensor reading
    
    // Common sensor objects
    BNO080 _bno080;
    SparkFunXM125Distance _radar;
    SFE_UBLOX_GNSS_SERIAL _gps;
    
    // Wing module sensor fusion state (only used by wing modules)
    float _fusedLatitude, _fusedLongitude, _fusedAltitude;
    float _velocityX, _velocityY, _velocityZ;
    unsigned long _lastGpsUpdateTime, _lastImuUpdateTime;
    bool _freshGpsData;
    
    // RTK quality monitoring (all modules)
    RTKStatus_t _rtkStatus;
    float _horizontalAccuracy;
    bool _rtkStatusChanged;
    
    static SensorManager* _instance;
};

#endif // SENSOR_MANAGER_H
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Clean compilation with all sensor libraries included
**Logic Evaluation**: Verify all existing sensor code is properly integrated

---

#### **Step 2.2: Implement Conditional Initialization and Runtime Logic**
**Objective**: Add role-based feature activation

**Tasks:**
```
1. Implement role-specific initialization in SensorManager::init()
2. Add conditional GPS dynamic model setting
3. Implement role-specific update logic
4. Add hydraulic controller integration for centre module
5. Test each role configuration
```

**SensorManager::init() Implementation:**
```cpp
void SensorManager::init() {
    Serial.println("Initializing Sensor Manager...");
    
    // Common initialization for all modules
    initBasicSensors();
    initGPSForRole();
    
    // Role-specific initialization
    if (ModuleConfig::isWingModule()) {
        Serial.println("Initializing wing module features...");
        initSensorFusion();
    } else if (ModuleConfig::isCentreModule()) {
        Serial.println("Initializing centre module features...");
        // Centre module uses direct sensor readings (no fusion)
    }
    
    Serial.println("Sensor Manager initialization complete");
}

void SensorManager::initGPSForRole() {
    // GPS initialization common to all modules
    Serial1.begin(115200);
    if (!_gps.begin(Serial1)) {
        Serial.println("Failed to initialize GPS");
        while(1);
    }
    
    // Configure GPS output and navigation rate
    _gps.setUART1Output(COM_TYPE_UBX | COM_TYPE_RTCM3);
    _gps.setNavigationFrequency(10);
    
    // Role-specific GPS configuration
    if (ModuleConfig::isWingModule()) {
        // Wing modules: Airborne dynamic model for wing tip motion
        if (!_gps.setDynamicModel(DYN_MODEL_AIRBORNE1g, VAL_LAYER_RAM_BBR)) {
            Serial.println("Warning: Failed to set Airborne dynamic model");
        } else {
            Serial.println("GPS dynamic model set to AIRBORNE1G");
        }
    } else if (ModuleConfig::isCentreModule()) {
        // Centre module: Automotive dynamic model for tractor chassis
        if (!_gps.setDynamicModel(DYN_MODEL_AUTOMOTIVE, VAL_LAYER_RAM_BBR)) {
            Serial.println("Warning: Failed to set Automotive dynamic model");
        } else {
            Serial.println("GPS dynamic model set to AUTOMOTIVE");
        }
    }
    
    // Configure GPS callback for all modules
    _gps.setAutoHPPOSLLHcallbackPtr(&gpsHPPOSLLHCallback);
    Serial.println("GPS HPPOSLLH callback configured");
    
    // Save configuration
    if (_gps.saveConfigSelective(VAL_CFG_SUBSEC_NAVCONF)) {
        Serial.println("GPS configuration saved to flash");
    }
}
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Clean compilation with role-specific initialization
**Logic Evaluation**: Verify correct GPS models are set for each role

---

#### **Step 2.3: Test Basic Unified Firmware (No OTA)**
**Objective**: Verify unified firmware works correctly for all roles

**Test Plan:**
1. **Compile for each role configuration**
2. **Test role-specific initialization**
3. **Verify GPS configuration per role**
4. **Test sensor data collection**
5. **Verify network communication**

**Testing Checklist:**
- [ ] Left wing module: Airborne GPS model, sensor fusion enabled
- [ ] Centre module: Automotive GPS model, hydraulic control enabled
- [ ] Right wing module: Airborne GPS model, sensor fusion enabled
- [ ] All modules: GPS callbacks working, RTK monitoring active
- [ ] Network communication functional for each role

**Expected Results:**
- Each role initializes with correct features
- GPS dynamic models set appropriately
- Sensor data collection works for all roles
- Debug output clearly shows role-specific behavior

**Logic Evaluation Checklist:**
- [ ] Role detection drives correct feature activation
- [ ] No unused code running on any module
- [ ] Memory usage appropriate for each role
- [ ] Performance meets requirements (50Hz sensor data)

---

### **PHASE 3: OTA Update Integration**

#### **Step 3.1: Integrate FlasherX-Ethernet_Support**
**Objective**: Add OTA update capability to unified firmware

**Tasks:**
```
1. Add FlasherX-Ethernet_Support library to project
2. Create OTAUpdateManager class
3. Implement basic version management
4. Add HTTP firmware download capability
5. Test single module update process
```

**Files to Create:**
- `OTAUpdateManager.h` - OTA update management header
- `OTAUpdateManager.cpp` - OTA update implementation
- `VersionManager.h` - Firmware version management
- `VersionManager.cpp` - Version comparison and storage

**Library Dependencies:**
```cpp
#include <QNEthernet.h>
#include <AsyncWebServer_Teensy41.h>
#include <FlasherX.h>
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Clean compilation with OTA libraries included
**Logic Evaluation**: Verify OTA infrastructure integrates without conflicts

---

#### **Step 3.2: Implement Version Management and Reporting**
**Objective**: Add firmware version tracking and network reporting

**Tasks:**
```
1. Implement VersionManager class
2. Add version information to data packets
3. Implement version comparison logic
4. Add EEPROM storage for version persistence
5. Test version reporting functionality
```

**VersionManager Implementation:**
```cpp
class VersionManager {
public:
    static FirmwareVersion_t getCurrentVersion();
    static bool isVersionNewer(const FirmwareVersion_t& current, const FirmwareVersion_t& available);
    static String getVersionString(const FirmwareVersion_t& version);
    static void saveVersionInfo(const FirmwareVersion_t& version);
    static FirmwareVersion_t loadVersionInfo();
    
private:
    static const FirmwareVersion_t CURRENT_VERSION;
    static const int EEPROM_VERSION_ADDRESS = 0;
};
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Version information included in all data packets
**Logic Evaluation**: Verify version comparison logic works correctly

---

#### **Step 3.3: Test OTA Update Infrastructure (Single Module)**
**Objective**: Verify OTA update process works for individual modules

**Test Plan:**
1. **Create test firmware with incremented version**
2. **Set up HTTP server on development machine**
3. **Test firmware download process**
4. **Verify firmware verification (hash checking)**
5. **Test firmware flashing and reboot**

**Testing Checklist:**
- [ ] Firmware download completes successfully
- [ ] SHA256 hash verification works
- [ ] Firmware flashing process completes
- [ ] Module reboots with new firmware
- [ ] Version information updates correctly

**Expected Results:**
- Single module can be updated via network
- Update process is reliable and repeatable
- Error handling works for network failures
- Version information persists across reboots

**Logic Evaluation Checklist:**
- [ ] Update state machine transitions correctly
- [ ] Error handling covers all failure modes
- [ ] Network timeouts handled appropriately
- [ ] Flash memory operations are safe

---

### **PHASE 4: Advanced Features**

#### **Step 4.1: Implement Dual-Bank Flash Rollback Logic**
**Objective**: Add firmware rollback capability for safety

**Tasks:**
```
1. Implement FlashBackupManager class
2. Add firmware backup before updates
3. Implement rollback functionality
4. Test rollback scenarios
5. Verify backup integrity checking
```

**FlashBackupManager Implementation:**
```cpp
class FlashBackupManager {
public:
    static bool backupCurrentFirmware();
    static bool restoreFromBackup();
    static bool isBackupValid();
    static FirmwareVersion_t getBackupVersion();
    
private:
    static const uint32_t CURRENT_FIRMWARE_BASE = 0x60000000;
    static const uint32_t BACKUP_FIRMWARE_BASE  = 0x60400000;
    static const uint32_t FIRMWARE_MAX_SIZE     = 0x400000;
};
```

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Rollback functionality compiles and integrates
**Logic Evaluation**: Verify rollback process is safe and reliable

---

#### **Step 4.2: Test Rollback and Recovery**
**Objective**: Verify rollback functionality works in failure scenarios

**Test Scenarios:**
1. **Successful update** → Backup created, new firmware runs
2. **Failed update** → Automatic rollback to previous version
3. **Corrupted firmware** → Manual rollback capability
4. **Power failure during update** → Recovery to backup version

**Testing Checklist:**
- [ ] Backup creation before updates
- [ ] Automatic rollback on update failure
- [ ] Manual rollback command functionality
- [ ] Backup integrity verification
- [ ] Recovery from power failure scenarios

**Expected Results:**
- Rollback completes in under 30 seconds
- Previous firmware version restored correctly
- System returns to operational state
- No data loss during rollback process

**Logic Evaluation Checklist:**
- [ ] Rollback process is atomic and safe
- [ ] Flash memory operations are protected
- [ ] Version information correctly restored
- [ ] System state consistent after rollback

---

### **PHASE 5: System Integration**

#### **Step 5.1: Implement Network Protocol Extensions**
**Objective**: Add update commands and status reporting to network protocol

**Tasks:**
```
1. Extend DataPackets.h with update-related packets
2. Add update command processing to NetworkManager
3. Implement progress reporting during updates
4. Add version status reporting
5. Test network protocol extensions
```

**New Packet Types:**
- `VersionStatusPacket` - Startup and periodic version reporting
- `FirmwareUpdateCommand` - Update instructions from Toughbook
- `UpdateProgressPacket` - Real-time update progress

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Network protocol supports all update operations
**Logic Evaluation**: Verify packet serialization/deserialization works

---

#### **Step 5.2: Test Per-Module Update Sequencing and Status Reporting**
**Objective**: Verify sequential update workflow and status synchronization

**Test Plan:**
1. **Set up three modules with different roles**
2. **Test sequential update process (Left → Centre → Right)**
3. **Verify status reporting during updates**
4. **Test update failure and recovery scenarios**
5. **Verify version synchronization across modules**

**Testing Checklist:**
- [ ] Sequential updates work correctly
- [ ] Status reporting provides real-time feedback
- [ ] Update failures handled gracefully
- [ ] Version synchronization maintained
- [ ] Network communication stable during updates

**Expected Results:**
- Updates proceed in correct sequence
- Status information accurate and timely
- Failures trigger appropriate recovery actions
- All modules maintain version consistency

**Logic Evaluation Checklist:**
- [ ] Update orchestration logic is robust
- [ ] Status reporting is comprehensive
- [ ] Error handling covers all scenarios
- [ ] Network protocol is reliable

---

### **PHASE 6: Final Integration and Testing**

#### **Step 6.1: Implement Version/Operational Status Reporting at System Startup**
**Objective**: Add startup version verification and system readiness checks

**Tasks:**
```
1. Add startup version status reporting
2. Implement system readiness verification
3. Add version mismatch detection
4. Test startup sequence with multiple modules
5. Verify system enable/disable logic
```

**Startup Sequence:**
1. Module boots → Detect role → Initialize sensors
2. Send version status to Toughbook
3. Wait for all modules to report status
4. Verify version consistency across modules
5. Enable system operation if all checks pass

**Compilation Test:**
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

**Expected Result**: Startup sequence ensures system consistency
**Logic Evaluation**: Verify startup checks prevent inconsistent operation

---

#### **Step 6.2: Test Full Update Workflow (Sequential Updates, Status Sync)**
**Objective**: Comprehensive testing of complete OTA update system

**Test Scenarios:**
1. **Normal update workflow** → All modules updated successfully
2. **Single module failure** → Rollback and retry logic
3. **Network interruption** → Recovery and continuation
4. **Power failure during update** → System recovery
5. **Version mismatch detection** → Corrective action

**Testing Checklist:**
- [ ] Complete update workflow functions correctly
- [ ] All failure scenarios handled appropriately
- [ ] System remains stable throughout process
- [ ] User interface provides clear feedback
- [ ] Documentation matches actual behavior

**Expected Results:**
- Full update workflow completes successfully
- All error scenarios result in safe system state
- Performance meets requirements
- System is ready for field deployment

**Logic Evaluation Checklist:**
- [ ] Update workflow is robust and reliable
- [ ] Error handling is comprehensive
- [ ] Performance is acceptable
- [ ] System meets all requirements

---

## Implementation Schedule

### **Week 1: Foundation**
- Steps 1.1 - 1.3: Project structure and role detection
- Milestone: Basic unified firmware with role detection

### **Week 2: Core Integration**
- Steps 2.1 - 2.3: Merge existing features and test
- Milestone: Unified firmware with all sensor capabilities

### **Week 3: OTA Infrastructure**
- Steps 3.1 - 3.3: Add OTA update capability
- Milestone: Single module OTA updates working

### **Week 4: Advanced Features**
- Steps 4.1 - 4.2: Rollback and recovery
- Milestone: Robust update system with rollback

### **Week 5: System Integration**
- Steps 5.1 - 5.2: Network protocol and sequencing
- Milestone: Multi-module update orchestration

### **Week 6: Final Testing**
- Steps 6.1 - 6.2: Startup checks and comprehensive testing
- Milestone: Production-ready OTA update system

## Success Criteria

### **Technical Requirements:**
- [ ] All modules compile without errors
- [ ] Role detection works reliably
- [ ] OTA updates complete successfully
- [ ] Rollback functionality is safe and fast
- [ ] Network protocol is robust
- [ ] System startup verification works

### **Performance Requirements:**
- [ ] Sensor data at 50Hz maintained
- [ ] Update download completes in <5 minutes
- [ ] Rollback completes in <30 seconds
- [ ] Network latency <100ms
- [ ] Memory usage within limits

### **Safety Requirements:**
- [ ] Invalid configurations detected and handled
- [ ] Update failures result in safe state
- [ ] Rollback always available
- [ ] Version consistency maintained
- [ ] System halts on critical errors

This implementation plan provides a structured approach to building the unified ABLS firmware with OTA capability, ensuring each step is thoroughly tested and validated before proceeding to the next phase.
