# ABLS Firmware Technical Reference - Part 2
## Network Communication, Data Structures, and Classes

**Version:** 1.0  
**Date:** July 29, 2025  
**Authors:** ABLS Development Team  
**Document Type:** Technical Reference Manual  

---

## Table of Contents

5. [Network Communication](#5-network-communication)
6. [Data Structures](#6-data-structures)
7. [Module Classes and Relationships](#7-module-classes-and-relationships)
8. [Calibration System](#8-calibration-system)

---

## 5. Network Communication

### 5.1 UDP Protocol Architecture

#### 5.1.1 Port Assignments
```cpp
// Network port definitions
#define ABLS_SENSOR_DATA_PORT    8888    // Sensor data to Toughbook
#define ABLS_COMMAND_PORT        8889    // Commands from Toughbook  
#define ABLS_RTCM_PORT          8890    // RTCM corrections
#define ABLS_UPDATE_PORT        8891    // Firmware updates
#define ABLS_DIAGNOSTIC_PORT    8892    // Diagnostic data
```

#### 5.1.2 Network Packet Validation
```cpp
// Robust packet processing with validation
void processUDPPacket() {
    int packetSize = _udp.parsePacket();
    if (packetSize > 0) {
        // Validate packet size
        if (packetSize > MAX_PACKET_SIZE) {
            DiagnosticManager::logError("Network", "Oversized packet rejected");
            return;
        }
        
        // Read with validation
        int bytesRead = _udp.read(_buffer, packetSize);
        if (bytesRead != packetSize) {
            DiagnosticManager::logError("Network", "Incomplete packet read");
            return;
        }
        
        // Null-terminate strings for safety
        _buffer[bytesRead] = '\0';
        
        // Process validated packet
        processCommand(_buffer, bytesRead);
    }
}
```

### 5.2 Data Packet Structures

#### 5.2.1 Sensor Data Packet (Module → Toughbook)
```cpp
struct SensorDataPacket {
    // Packet Metadata
    uint32_t PacketId;           // Incremental packet counter
    uint32_t Timestamp;          // Milliseconds since boot
    uint8_t SenderId;            // Module ID (1=Left, 2=Centre, 3=Right)
    
    // GPS Data (High-Precision)
    double Latitude;             // Degrees (high-res + hp combined)
    double Longitude;            // Degrees (high-res + hp combined) 
    double Altitude;             // Meters above ellipsoid
    uint32_t HorizontalAccuracy; // Millimeters
    uint8_t FixType;            // 0=No fix, 3=3D, 4=RTK
    uint8_t SatellitesUsed;     // Number of satellites
    
    // IMU Data
    float QuaternionI, QuaternionJ, QuaternionK, QuaternionReal;
    float AccelX, AccelY, AccelZ;           // m/s² raw acceleration
    float GyroX, GyroY, GyroZ;              // deg/s angular velocity
    float LinearAccelX, LinearAccelY, LinearAccelZ;  // m/s² gravity-compensated
    uint8_t QuaternionAccuracy;             // 0-3 scale
    uint8_t AccelAccuracy;                  // 0-3 scale
    uint8_t GyroAccuracy;                   // 0-3 scale
    
    // Radar Data
    float RadarDistance;         // Meters to ground/crop
    uint8_t RadarValid;         // 0=Invalid, 1=Valid
    
    // Hydraulic Ram Positions (Centre module only)
    float RamPosCenterPercent;   // 0-100%
    float RamPosLeftPercent;     // 0-100%  
    float RamPosRightPercent;    // 0-100%
};
```

#### 5.2.2 Control Command Packet (Toughbook → Centre Module)
```cpp
struct ControlCommandPacket {
    // Command Metadata
    uint32_t CommandId;          // Unique command identifier
    uint32_t Timestamp;          // Command timestamp
    
    // Hydraulic Ram Setpoints
    float SetpointCenter;        // 0-100% target position
    float SetpointLeft;          // 0-100% target position
    float SetpointRight;         // 0-100% target position
    
    // Control Parameters
    uint8_t ControlMode;         // 0=Manual, 1=Auto, 2=Emergency
    float MaxRate;               // Maximum change rate (%/sec)
    uint8_t Priority;            // Command priority level
};
```

### 5.3 RTCM Correction Distribution

#### 5.3.1 Centralized Distribution Model
```cpp
// Centre module receives and redistributes RTCM
void NetworkManager::handleRTCMData(const uint8_t* data, size_t length) {
    // Validate RTCM data integrity
    if (!validateRTCMData(data, length)) {
        DiagnosticManager::logError("RTCM", "Invalid correction data");
        return;
    }
    
    // Forward to local GPS
    _gps.pushRawData(data, length);
    
    // Broadcast to other modules
    broadcastRTCMToModules(data, length);
}

bool NetworkManager::validateRTCMData(const uint8_t* data, size_t length) {
    // Check RTCM preamble (0xD3)
    if (length < 3 || data[0] != 0xD3) return false;
    
    // Validate length field
    uint16_t messageLength = ((data[1] & 0x03) << 8) | data[2];
    if (messageLength + 6 != length) return false;  // +6 for header/CRC
    
    return true;
}
```

### 5.4 Network Error Handling

#### 5.4.1 Timeout Detection and Recovery
```cpp
// Monitor network health
class NetworkHealthMonitor {
private:
    uint32_t _lastPacketTime = 0;
    static const uint32_t TIMEOUT_MS = 5000;  // 5 second timeout
    
public:
    void update() {
        if (millis() - _lastPacketTime > TIMEOUT_MS) {
            DiagnosticManager::logWarning("Network", "Communication timeout");
            // Trigger reconnection attempt
            attemptReconnection();
        }
    }
    
    void attemptReconnection() {
        // Reset network interface
        _ethernet.end();
        delay(1000);
        
        // Reinitialize with DHCP fallback
        if (!_ethernet.begin()) {
            DiagnosticManager::logError("Network", "Reconnection failed");
        }
    }
};
```

---

## 6. Data Structures

### 6.1 Core Data Types

#### 6.1.1 Module Configuration
```cpp
enum class ModuleRole : uint8_t {
    UNKNOWN = 0,
    LEFT_WING = 1,
    CENTRE = 2, 
    RIGHT_WING = 3,
    SPARE_1 = 4,
    SPARE_2 = 5
};

struct ModuleConfig {
    ModuleRole role;
    String serialNumber;
    String firmwareVersion;
    uint32_t bootTime;
    bool diagnosticsEnabled;
    
    static ModuleRole detectRole() {
        // Read DIP switch configuration (INPUT_PULLUP, tied to GND)
        if (!digitalRead(2)) return ModuleRole::LEFT_WING;
        if (!digitalRead(3)) return ModuleRole::CENTRE;
        if (!digitalRead(4)) return ModuleRole::RIGHT_WING;
        if (!digitalRead(5)) return ModuleRole::SPARE_1;
        if (!digitalRead(6)) return ModuleRole::SPARE_2;
        return ModuleRole::UNKNOWN;
    }
};
```

#### 6.1.2 Sensor Status Structures
```cpp
struct GPSStatus {
    bool initialized;
    uint8_t fixType;            // 0=No fix, 3=3D, 4=RTK
    uint8_t satellitesUsed;
    uint32_t horizontalAccuracy; // mm
    uint32_t lastUpdateTime;
    bool rtcmActive;            // Receiving corrections
    uint32_t rtcmPacketsReceived;
};

struct IMUStatus {
    bool initialized;
    uint8_t quaternionAccuracy;  // 0-3 (0=unreliable, 3=high)
    uint8_t accelAccuracy;       // 0-3
    uint8_t gyroAccuracy;        // 0-3
    bool calibrationComplete;
    uint32_t lastUpdateTime;
    uint32_t dataRate;          // Hz
    uint32_t performanceCounter;
};

struct RadarStatus {
    bool initialized;
    bool dataValid;
    float distance;             // meters
    uint16_t signalStrength;
    uint8_t errorStatus;
    uint32_t lastUpdateTime;
    uint32_t recalibrationCount;
};
```

### 6.2 Calibration Data Structures

#### 6.2.1 Multi-Sensor Calibration Data
```cpp
struct MultiSensorCalibrationPoint {
    // IMU Data
    float quaternionI, quaternionJ, quaternionK, quaternionReal;
    float linearAccelX, linearAccelY, linearAccelZ;
    uint8_t imuAccuracy;
    
    // Ram Data (Centre module only)
    float ramCenterPercent, ramLeftPercent, ramRightPercent;
    
    // Radar Data  
    float radarDistance;
    bool radarValid;
    
    // Calculated Values
    double imuBoomAngle;        // Degrees from IMU
    double ramBoomAngle;        // Degrees from ram position
    double radarHeight;         // Height from radar
    
    // Metadata
    uint32_t captureTime;
    bool dataValid;
};

struct CalibrationData {
    MultiSensorCalibrationPoint levelReference;  // Manual level setting
    MultiSensorCalibrationPoint levelPosition;   // Sensor level capture
    MultiSensorCalibrationPoint maxUpPosition;   // Maximum up angle
    MultiSensorCalibrationPoint maxDownPosition; // Maximum down angle
    
    // Correlation tables for sensor cross-validation
    std::map<float, float> ramToIMUCorrelation;
    std::map<float, float> radarToIMUCorrelation;
    
    // Calibration metadata
    bool isComplete;
    uint32_t calibrationTime;
    String calibratedBy;        // Operator/installer ID
    String calibrationNotes;    // Field conditions, etc.
};
```

### 6.3 Diagnostic Data Structures

#### 6.3.1 System Health Monitoring
```cpp
struct SystemHealth {
    // Performance metrics
    uint32_t uptime;            // Seconds since boot
    uint32_t freeMemory;        // Bytes available
    float cpuUsage;             // Percentage
    float temperature;          // Celsius (if available)
    
    // Sensor health
    GPSStatus gpsStatus;
    IMUStatus imuStatus;
    RadarStatus radarStatus;
    
    // Network health
    uint32_t packetsReceived;
    uint32_t packetsSent;
    uint32_t networkErrors;
    uint32_t lastNetworkActivity;
    bool networkConnected;
    
    // Error counters
    uint32_t totalErrors;
    uint32_t totalWarnings;
    uint32_t criticalErrors;
    uint32_t lastErrorTime;
};
```

---

## 7. Module Classes and Relationships

### 7.1 Class Hierarchy Overview

```
ABLSModule (main)
├── ModuleConfig (role detection, configuration)
├── SensorManager (sensor coordination)
│   ├── GPS (u-blox ZED-F9P)
│   ├── IMU (SparkFun BNO080)
│   └── Radar (SparkFun XM125)
├── NetworkManager (UDP communication)
├── HydraulicController (Centre module only)
│   ├── PIDController (3x instances)
│   └── ADC (ADS1115)
├── DiagnosticManager (logging, health monitoring)
├── UpdateManager (firmware updates)
│   ├── VersionManager
│   ├── FlashBackupManager
│   └── UpdateSafetyManager
└── DisplayManager (OLED diagnostics)
```

### 7.2 Core Classes

#### 7.2.1 SensorManager Class
```cpp
class SensorManager {
private:
    // Sensor instances
    SFE_UBLOX_GNSS _gps;
    BNO080 _imu;
    SparkFunXM125Distance _radar;
    
    // Status tracking
    bool _gpsInitialized, _imuInitialized, _radarInitialized;
    uint32_t _lastGPSUpdate, _lastIMUUpdate, _lastRadarUpdate;
    
    // Performance monitoring
    uint32_t _imuDataRate, _gpsDataRate;
    uint32_t _lastPerformanceReport;
    uint32_t _sensorUpdateCounter;
    
    // Agricultural-specific tuning
    static const uint32_t IMU_TIMEOUT_MS = 1000;
    static const uint32_t RADAR_TIMEOUT_MS = 5000;
    static const uint32_t GPS_TIMEOUT_MS = 2000;
    
public:
    bool initialize();
    void update();                    // Main sensor update loop
    void populateDataPacket(SensorDataPacket* packet);
    
    // Individual sensor methods
    bool initializeGPS();
    bool initializeIMU(); 
    bool initializeRadar();
    
    void updateGPS();
    void updateIMU();
    void updateRadar();
    
    // Status and diagnostics
    String getStatusString();
    GPSStatus getGPSStatus();
    IMUStatus getIMUStatus();
    RadarStatus getRadarStatus();
    
    // Error recovery
    void resetSensors();
    void recalibrateIMU();
    void recalibrateRadar();
};
```

#### 7.2.2 NetworkManager Class
```cpp
class NetworkManager {
private:
    QNEthernet _ethernet;
    EthernetUDP _udpSensor, _udpCommand, _udpRTCM;
    
    // Packet buffers with safety margins
    uint8_t _receiveBuffer[MAX_PACKET_SIZE + 16];
    uint8_t _sendBuffer[MAX_PACKET_SIZE + 16];
    
    // Network state
    bool _connected;
    IPAddress _toughbookIP;
    uint32_t _lastActivity;
    uint32_t _packetCounter;
    
    // Concurrent update prevention
    bool _updateInProgress;
    uint32_t _updateStartTime;
    
public:
    bool initialize();
    void update();
    
    // Packet transmission
    void sendSensorData(const SensorDataPacket& packet);
    void sendDiagnosticData(const SystemHealth& health);
    void sendVersionInfo();
    
    // Packet reception with validation
    void processIncomingPackets();
    void handleControlCommand(const ControlCommandPacket& command);
    void handleRTCMData(const uint8_t* data, size_t length);
    void handleUpdateCommand(const String& command);
    void handleCalibrationCommand(const String& command);
    
    // RTCM distribution (Centre module only)
    void broadcastRTCMToModules(const uint8_t* data, size_t length);
    bool validateRTCMData(const uint8_t* data, size_t length);
    
    // Network health and safety
    bool isConnected() const { return _connected; }
    uint32_t getLastActivity() const { return _lastActivity; }
    bool isUpdateInProgress() const { return _updateInProgress; }
    void preventConcurrentUpdates();
};
```

#### 7.2.3 HydraulicController Class (Centre Module Only)
```cpp
class HydraulicController {
private:
    // ADC for ram position feedback
    Adafruit_ADS1115 _adc;
    
    // PID controllers for each ram
    PIDController _ramCenter, _ramLeft, _ramRight;
    
    // Current state
    float _currentPositions[3];     // Current ram positions (%)
    float _targetPositions[3];      // Target ram positions (%)
    uint32_t _lastUpdate;
    bool _emergencyStop;
    
    // Safety limits and parameters
    static const float MAX_RATE = 10.0;     // Maximum change rate (%/sec)
    static const float MIN_POS = 0.0;       // Minimum position (%)
    static const float MAX_POS = 100.0;     // Maximum position (%)
    static const float DEADBAND = 0.5;      // Position deadband (%)
    
    // PID parameters (tunable)
    float _pidKp = 2.0, _pidKi = 0.1, _pidKd = 0.05;
    
public:
    bool initialize();
    void update();
    
    // Control interface
    void setTargetPositions(float center, float left, float right);
    void getCurrentPositions(float& center, float& left, float& right);
    bool isAtTarget(float tolerance = 1.0);
    
    // Safety and limits
    void emergencyStop();
    void resumeOperation();
    bool isWithinLimits(float position);
    bool isEmergencyStopped() const { return _emergencyStop; }
    
    // Ram position sensing
    float readRamPosition(uint8_t channel);
    void populateRamPositions(SensorDataPacket* packet);
    void calibrateRamSensors();
    
    // PID tuning
    void setPIDParameters(float kp, float ki, float kd);
    void resetPIDControllers();
    void autoTunePID();  // Future enhancement
};
```

### 7.3 Class Relationships and Data Flow

#### 7.3.1 Initialization Sequence
```cpp
void setup() {
    // 1. Hardware initialization
    Serial.begin(115200);
    pinMode(2, INPUT_PULLUP);  // Role detection pins
    pinMode(3, INPUT_PULLUP);
    pinMode(4, INPUT_PULLUP);
    pinMode(5, INPUT_PULLUP);
    pinMode(6, INPUT_PULLUP);
    
    // 2. Module configuration
    ModuleConfig::initialize();
    ModuleRole role = ModuleConfig::detectRole();
    
    // 3. Core systems
    DiagnosticManager::initialize();
    DisplayManager::initialize();
    
    // 4. Sensor systems
    if (!sensorManager.initialize()) {
        DiagnosticManager::logCritical("System", "Sensor initialization failed");
        // Continue with degraded functionality
    }
    
    // 5. Network systems
    if (!networkManager.initialize()) {
        DiagnosticManager::logCritical("System", "Network initialization failed");
        // Continue in standalone mode
    }
    
    // 6. Role-specific systems
    if (role == ModuleRole::CENTRE) {
        if (!hydraulicController.initialize()) {
            DiagnosticManager::logCritical("System", "Hydraulic controller failed");
        }
    }
    
    // 7. Update system
    updateManager.initialize();
    
    DiagnosticManager::logInfo("System", "Initialization complete");
}
```

#### 7.3.2 Data Flow Architecture
```
┌─────────────────┐    update()     ┌─────────────────┐
│  SensorManager  │────────────────►│  DataPacket     │
│                 │                 │  Population     │
└─────────────────┘                 └─────────────────┘
         │                                   │
         │ sensor data                       │ populated packet
         ▼                                   ▼
┌─────────────────┐                 ┌─────────────────┐
│  Diagnostic     │                 │  NetworkManager │
│  Manager        │                 │                 │
└─────────────────┘                 └─────────────────┘
         │                                   │
         │ health data                       │ UDP transmission
         ▼                                   ▼
┌─────────────────┐                 ┌─────────────────┐
│  DisplayManager │                 │   Toughbook     │
│  (OLED)         │                 │   (ABLS.Core)   │
└─────────────────┘                 └─────────────────┘
```

---

## 8. Calibration System

### 8.1 Multi-Sensor Calibration Architecture

The ABLS calibration system integrates three sensor types for maximum accuracy and reliability:

#### 8.1.1 Calibration Workflow
```cpp
enum class CalibrationStep {
    NOT_STARTED = 0,
    LEVEL_REFERENCE_SET = 1,    // Manual level measurement
    LEVEL_CAPTURED = 2,         // Sensor level data
    MAX_UP_CAPTURED = 3,        // Maximum up position
    MAX_DOWN_CAPTURED = 4,      // Maximum down position
    COMPLETE = 5
};

class CalibrationManager {
private:
    CalibrationStep _currentStep;
    CalibrationData _calibrationData;
    bool _calibrationInProgress;
    
public:
    // Calibration workflow
    bool startCalibration();
    bool setLevelReference();           // Manual level setting
    bool captureLevelPosition();        // Sensor data at level
    bool captureMaxUpPosition();        // Maximum up angle
    bool captureMaxDownPosition();      // Maximum down angle
    bool completeCalibration();         // Finalize and save
    
    // Data correlation
    void buildCorrelationTables();
    double correlateIMUToRam(float imuAngle);
    double correlateRadarToIMU(float radarDistance);
    
    // Validation
    bool validateCalibrationData();
    double calculateSensorAgreement();
};
```

#### 8.1.2 Manual Level Reference Setting
```cpp
bool CalibrationManager::setLevelReference() {
    // Capture current sensor readings as level reference
    MultiSensorCalibrationPoint reference;
    
    // Get current IMU data
    if (_sensorManager->getIMUStatus().initialized) {
        // Capture quaternion at manually-leveled position
        reference.quaternionI = _sensorManager->getCurrentQuaternionI();
        reference.quaternionJ = _sensorManager->getCurrentQuaternionJ();
        reference.quaternionK = _sensorManager->getCurrentQuaternionK();
        reference.quaternionReal = _sensorManager->getCurrentQuaternionReal();
        reference.imuAccuracy = _sensorManager->getCurrentQuaternionAccuracy();
    }
    
    // Get current ram positions (Centre module only)
    if (_moduleRole == ModuleRole::CENTRE) {
        _hydraulicController->getCurrentPositions(
            reference.ramCenterPercent,
            reference.ramLeftPercent, 
            reference.ramRightPercent
        );
    }
    
    // Get current radar data
    reference.radarDistance = _sensorManager->getCurrentRadarDistance();
    reference.radarValid = _sensorManager->isRadarDataValid();
    
    // Calculate reference angles (should be 0° for level)
    reference.imuBoomAngle = 0.0;      // Level reference
    reference.ramBoomAngle = 0.0;      // Level reference
    reference.radarHeight = reference.radarDistance;  // Ground clearance
    
    // Store reference data
    _calibrationData.levelReference = reference;
    _currentStep = CalibrationStep::LEVEL_REFERENCE_SET;
    
    DiagnosticManager::logInfo("Calibration", "Level reference captured");
    return true;
}
```

#### 8.1.3 Sensor Correlation Analysis
```cpp
double CalibrationManager::calculateSensorAgreement() {
    if (!_calibrationData.isComplete) return -1.0;
    
    double totalDifference = 0.0;
    int comparisonCount = 0;
    
    // Compare IMU vs Ram angles at each calibration point
    std::vector<MultiSensorCalibrationPoint> points = {
        _calibrationData.levelPosition,
        _calibrationData.maxUpPosition,
        _calibrationData.maxDownPosition
    };
    
    for (const auto& point : points) {
        if (point.dataValid && _moduleRole == ModuleRole::CENTRE) {
            double imuAngle = point.imuBoomAngle;
            double ramAngle = point.ramBoomAngle;
            double difference = abs(imuAngle - ramAngle);
            
            totalDifference += difference;
            comparisonCount++;
        }
    }
    
    // Return average difference in degrees
    return comparisonCount > 0 ? totalDifference / comparisonCount : -1.0;
}
```

### 8.2 Calibration Data Persistence

#### 8.2.1 EEPROM Storage
```cpp
class CalibrationStorage {
private:
    static const uint16_t EEPROM_BASE_ADDR = 0x100;
    static const uint16_t CALIBRATION_SIGNATURE = 0xABCD;
    
public:
    bool saveCalibration(const CalibrationData& data) {
        // Write signature
        EEPROM.put(EEPROM_BASE_ADDR, CALIBRATION_SIGNATURE);
        
        // Write calibration data
        EEPROM.put(EEPROM_BASE_ADDR + 2, data);
        
        // Calculate and write checksum
        uint16_t checksum = calculateChecksum(data);
        EEPROM.put(EEPROM_BASE_ADDR + sizeof(data) + 2, checksum);
        
        return true;
    }
    
    bool loadCalibration(CalibrationData& data) {
        // Verify signature
        uint16_t signature;
        EEPROM.get(EEPROM_BASE_ADDR, signature);
        if (signature != CALIBRATION_SIGNATURE) return false;
        
        // Load data
        EEPROM.get(EEPROM_BASE_ADDR + 2, data);
        
        // Verify checksum
        uint16_t storedChecksum, calculatedChecksum;
        EEPROM.get(EEPROM_BASE_ADDR + sizeof(data) + 2, storedChecksum);
        calculatedChecksum = calculateChecksum(data);
        
        return storedChecksum == calculatedChecksum;
    }
};
```

---

**Continue to Part 3 for Firmware Update System, Logic Flow, and Integration Guidelines...**
