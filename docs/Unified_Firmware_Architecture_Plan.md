# ABLS Unified Firmware Architecture Plan

## Overview
This document outlines the detailed architecture for a unified firmware that runs on all three ABLS modules (Left Wing, Centre, Right Wing) with hardware-based role detection.

## 1. Hardware Role Detection System

### 1.1 GPIO Pin Assignment
```cpp
// Hardware configuration detection pins
#define PIN_LEFT_CONFIG     2   // Tie to GND for left wing module
#define PIN_CENTRE_CONFIG   3   // Tie to GND for centre module  
#define PIN_RIGHT_CONFIG    4   // Tie to GND for right wing module
```

### 1.2 Module Role Enumeration
```cpp
typedef enum {
    MODULE_LEFT = 0,      // Left wing module
    MODULE_CENTRE = 1,    // Centre module (hydraulic control)
    MODULE_RIGHT = 2,     // Right wing module
    MODULE_UNKNOWN = 3    // Error condition - no valid config detected
} ModuleRole_t;
```

### 1.3 Role Detection Logic
- All pins configured with `INPUT_PULLUP` at startup
- Exactly one pin should be tied to GND during installation
- Detection occurs once at startup and stored globally
- Error handling for invalid configurations (multiple pins low, no pins low)

## 2. Unified Project Structure

### 2.1 New Project Layout
```
src/Firmware/ABLSModule/
├── ABLSModule.ino              // Main Arduino sketch
├── ModuleConfig.h              // Hardware role detection
├── ModuleConfig.cpp            // Role detection implementation
├── SensorManager.h             // Unified sensor management
├── SensorManager.cpp           // Sensor fusion + GPS callbacks
├── NetworkManager.h            // Unified networking
├── NetworkManager.cpp          // Role-specific network behavior
├── HydraulicController.h       // Centre module only
├── HydraulicController.cpp     // PID control + ADS1115
├── DataPackets.h               // Common data structures
└── README.md                   // Module configuration guide
```

### 2.2 Role-Specific Feature Matrix
| Feature | Left Wing | Centre | Right Wing |
|---------|-----------|--------|------------|
| GPS (High-Precision) | ✅ | ✅ | ✅ |
| IMU (BNO080) | ✅ | ✅ | ✅ |
| Radar (XM125) | ✅ | ✅ | ✅ |
| Sensor Fusion | ✅ | ❌ | ✅ |
| Hydraulic Control | ❌ | ✅ | ❌ |
| RTCM Broadcasting | ❌ | ✅ | ❌ |
| RTCM Receiving | ✅ | ❌ | ✅ |
| GPS Dynamic Model | Airborne1G | Automotive | Airborne1G |
| Data Packet Type | Sensor | Sensor+Control | Sensor |

## 3. Core Architecture Components

### 3.1 ModuleConfig Class
```cpp
class ModuleConfig {
public:
    static void detectRole();
    static ModuleRole_t getRole();
    static String getRoleName();
    static bool isCentreModule();
    static bool isWingModule();
    
private:
    static ModuleRole_t _moduleRole;
    static bool _roleDetected;
};
```

**Responsibilities:**
- Hardware pin detection at startup
- Global role state management
- Helper functions for role-based decisions
- Error handling for invalid configurations

### 3.2 Unified SensorManager Class
```cpp
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
    
    // Wing module sensor fusion state
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
```

### 3.3 Unified NetworkManager Class
```cpp
class NetworkManager {
public:
    NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort);
    void begin(byte mac[]);
    void update();                  // Role-specific network behavior
    void sendSensorData(const SensorDataPacket& packet);
    
    // Role-specific RTCM methods
    void beginRtcmListener();       // Wing modules
    void beginRtcmBroadcaster();    // Centre module
    int readRtcmData(uint8_t* buffer, size_t maxSize);
    void broadcastRtcmData(const uint8_t* data, size_t len);
    
    // Centre module specific
    int readCommandPacket(ControlCommandPacket* packet);
    void setHydraulicController(HydraulicController* controller);
    
private:
    // Role-specific update methods
    void updateCentreModule();      // RTCM broadcast + command processing
    void updateWingModule();        // RTCM receiving only
    
    // Network objects
    EthernetUDP _sensorUdp;
    EthernetUDP _commandUdp;        // Centre module only
    EthernetUDP _rtcmUdp;
    
    // Network configuration
    IPAddress _remoteIp;
    IPAddress _broadcastIp;         // Centre module only
    unsigned int _remotePort, _localPort;
    
    // Centre module specific
    HydraulicController* _hydraulicController;
    char _packetBuffer[512];
};
```

### 3.4 HydraulicController Class (Centre Module Only)
```cpp
class HydraulicController {
public:
    HydraulicController();
    void init();                    // Initialize ADS1115 and PID controllers
    void update();                  // PID control loop
    void setTargets(float center, float left, float right);
    void populatePacket(SensorDataPacket* packet);
    
private:
    // PID controllers for each ram
    PIDController _centerPID, _leftPID, _rightPID;
    
    // ADS1115 ADC for ram position feedback
    Adafruit_ADS1115 _ads1115;
    
    // Current positions and targets
    float _centerPos, _leftPos, _rightPos;
    float _centerTarget, _leftTarget, _rightTarget;
    
    // Helper methods
    float readRamPosition(int channel);
    void updatePIDController(PIDController& pid, float current, float target, int outputPin);
};
```

## 4. Initialization Flow

### 4.1 Startup Sequence
```cpp
void setup() {
    Serial.begin(115200);
    
    // Step 1: Detect hardware role
    ModuleConfig::detectRole();
    Serial.print("Module Role: ");
    Serial.println(ModuleConfig::getRoleName());
    
    // Step 2: Initialize sensors (role-aware)
    sensorManager.init();
    
    // Step 3: Initialize networking (role-aware)
    networkManager.begin(mac);
    
    // Step 4: Initialize hydraulics (centre module only)
    if (ModuleConfig::isCentreModule()) {
        hydraulicController.init();
        networkManager.setHydraulicController(&hydraulicController);
    }
    
    Serial.println("Module initialization complete");
}
```

### 4.2 Role-Specific Initialization

#### Centre Module Initialization:
- GPS: Automotive dynamic model, 10Hz, callback registration
- Sensors: IMU, Radar (direct reading, no fusion)
- Network: Command UDP + RTCM broadcast UDP + sensor UDP
- Hydraulics: ADS1115 ADC + 3 PID controllers
- RTCM: Radio modem interface for base station corrections

#### Wing Module Initialization:
- GPS: Airborne1G dynamic model, 10Hz, callback registration
- Sensors: IMU, Radar + sensor fusion initialization
- Network: RTCM receive UDP + sensor UDP
- Sensor Fusion: Dead reckoning state variables
- RTCM: UDP listener for corrections from centre module

## 5. Runtime Behavior

### 5.1 Main Loop Flow
```cpp
void loop() {
    // Step 1: Process network data (role-specific)
    networkManager.update();
    
    // Step 2: Update sensors (role-specific)
    sensorManager.update();
    
    // Step 3: Update hydraulics (centre module only)
    if (ModuleConfig::isCentreModule()) {
        hydraulicController.update();
    }
    
    // Step 4: Send sensor data (all modules)
    SensorDataPacket packet;
    sensorManager.populatePacket(&packet);
    if (ModuleConfig::isCentreModule()) {
        hydraulicController.populatePacket(&packet);
    }
    networkManager.sendSensorData(packet);
    
    delay(20); // 50Hz main loop
}
```

### 5.2 Network Behavior by Role

#### Centre Module Network Behavior:
- **Receive**: Control commands from Toughbook
- **Receive**: RTCM corrections from radio modem
- **Broadcast**: RTCM corrections to wing modules via UDP
- **Send**: Sensor + hydraulic data to Toughbook
- **Forward**: RTCM corrections to local GPS

#### Wing Module Network Behavior:
- **Receive**: RTCM corrections from centre module via UDP
- **Send**: Sensor data to Toughbook
- **Forward**: RTCM corrections to local GPS

## 6. Data Flow Architecture

### 6.1 GPS Data Flow
```
All Modules:
GPS Hardware → HPPOSLLH Callback → RTK Quality Assessment

Centre Module:
Callback Data → Direct Position → Data Packet → Toughbook

Wing Modules:
Callback Data → Sensor Fusion → Fused Position → Data Packet → Toughbook
```

### 6.2 RTCM Correction Flow
```
Radio Modem → Centre Module → UDP Broadcast → Wing Modules → Local GPS
                    ↓
              Centre Module GPS
```

### 6.3 Control Command Flow
```
Toughbook → Centre Module → PID Controllers → Hydraulic Valves
```

## 7. Configuration and Deployment

### 7.1 Hardware Installation
- **Left Wing**: Tie GPIO pin 2 to GND
- **Centre**: Tie GPIO pin 3 to GND  
- **Right Wing**: Tie GPIO pin 4 to GND
- **All modules**: Flash same unified firmware binary

### 7.2 Network Configuration
```cpp
// IP Address assignment by role
IPAddress getModuleIP() {
    switch (ModuleConfig::getRole()) {
        case MODULE_LEFT:   return IPAddress(192, 168, 1, 10);
        case MODULE_CENTRE: return IPAddress(192, 168, 1, 11);
        case MODULE_RIGHT:  return IPAddress(192, 168, 1, 12);
        default:           return IPAddress(192, 168, 1, 99); // Error
    }
}
```

### 7.3 MAC Address Assignment
```cpp
// MAC address assignment by role
void getModuleMAC(byte mac[]) {
    byte baseMac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x00};
    memcpy(mac, baseMac, 6);
    mac[5] = (byte)ModuleConfig::getRole(); // Last byte = role
}
```

## 8. Error Handling and Diagnostics

### 8.1 Role Detection Errors
- Multiple pins tied to GND → Error state, halt execution
- No pins tied to GND → Error state, halt execution
- Invalid role detected → Error state, halt execution

### 8.2 Runtime Diagnostics
- Serial output includes module role in all debug messages
- LED indicators for role status (optional)
- Network connectivity monitoring
- GPS RTK status monitoring
- Sensor health monitoring

## 9. Testing Strategy

### 9.1 Unit Testing
- Role detection logic with simulated pin states
- Conditional feature activation
- Network behavior by role
- GPS callback processing

### 9.2 Integration Testing
- Three modules with different role configurations
- RTCM distribution from centre to wings
- Sensor data collection from all modules
- Hydraulic control on centre module only

### 9.3 Field Testing
- Deploy all three modules with unified firmware
- Verify role-specific behavior
- Monitor GPS RTK performance
- Test hydraulic control and sensor fusion

## 10. Migration Plan

### 10.1 Phase 1: Create Unified Project
- Create new ABLSModule project structure
- Implement ModuleConfig class with role detection
- Merge common code from existing projects

### 10.2 Phase 2: Implement Role-Specific Features
- Add conditional initialization based on role
- Implement role-specific update logic
- Test role detection and feature activation

### 10.3 Phase 3: Network Integration
- Implement role-specific network behavior
- Test RTCM distribution architecture
- Verify data packet transmission

### 10.4 Phase 4: Hardware Testing
- Deploy unified firmware to test hardware
- Validate role detection with actual GPIO pins
- Test complete system integration

### 10.5 Phase 5: Deprecate Old Projects
- Archive BoomWingModule and BoomCentreModule projects
- Update documentation and deployment procedures
- Establish unified firmware as production standard

## 11. Benefits Summary

### 11.1 Development Benefits
- **Single codebase**: Eliminate version synchronization issues
- **Unified testing**: One comprehensive test suite
- **Consistent behavior**: Same timing and logic across all modules
- **Simplified debugging**: Common codebase for all modules

### 11.2 Deployment Benefits
- **Single binary**: Same firmware for all modules
- **Simplified manufacturing**: No module-specific firmware variants
- **Field updates**: One firmware version to manage
- **Installation flexibility**: Role determined by simple jumper wire

### 11.3 Maintenance Benefits
- **Bug fixes**: Apply once, benefit all modules
- **Feature additions**: Automatic consistency across modules
- **Code reuse**: Maximum sharing of common functionality
- **Documentation**: Single comprehensive guide

This unified architecture represents a significant improvement in maintainability, deployability, and reliability for the ABLS system.
