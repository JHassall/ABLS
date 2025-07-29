# ABLS Firmware Technical Reference - Part 1
## System Overview, Architecture, and Sensors

**Version:** 1.0  
**Date:** July 29, 2025  
**Authors:** ABLS Development Team  
**Document Type:** Technical Reference Manual  

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Architecture](#2-hardware-architecture)
3. [Firmware Architecture](#3-firmware-architecture)
4. [Sensor Systems](#4-sensor-systems)

---

## 1. System Overview

### 1.1 ABLS System Purpose

The **Automatic Boom Level System (ABLS)** is a precision agricultural control system designed for John Deere 4030 boomsprays. The system maintains optimal boom height above crops using a combination of:

- **Digital Elevation Model (DEM)** terrain data
- **Real-time GPS positioning** (RTK-corrected)
- **Inertial Measurement Units (IMU)** for boom orientation
- **Radar sensors** for ground/crop height measurement
- **Hydraulic ram potentiometers** for mechanical position feedback

### 1.2 System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    ABLS System Architecture                 │
├─────────────────────────────────────────────────────────────┤
│  Panasonic Toughbook (Cabin)                              │
│  ├── ABLS.Core Application                                 │
│  ├── Unity UI (Future)                                     │
│  ├── DEM Processing                                        │
│  └── Network Management                                    │
│                                                            │
│  Network (Ethernet/UDP)                                    │
│  ├── Sensor Data (50Hz from modules)                      │
│  ├── Control Commands (to Centre module)                  │
│  ├── RTCM Corrections (GPS)                               │
│  └── Firmware Updates                                     │
│                                                            │
│  ABLS Modules (Teensy 4.1)                               │
│  ├── Left Wing Module                                     │
│  ├── Centre Module (Master)                               │
│  └── Right Wing Module                                    │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 Key Design Principles

- **Unified Firmware**: Single codebase for all modules with role detection
- **Multi-Sensor Fusion**: IMU + GPS + Radar + Ram potentiometers
- **Agricultural Robustness**: EMI resistance, dust/moisture protection
- **Field Serviceability**: Remote firmware updates, comprehensive diagnostics
- **Safety-First**: Redundant sensors, graceful degradation, fail-safe operation

---

## 2. Hardware Architecture

### 2.1 Module Hardware Specifications

#### 2.1.1 Teensy 4.1 Microcontroller
- **Processor**: 600 MHz ARM Cortex-M7
- **Memory**: 1024KB RAM, 8MB Flash (dual-bank for updates)
- **Connectivity**: Ethernet, multiple UART/I2C/SPI interfaces
- **Storage**: MicroSD card slot for logging
- **GPIO**: Sufficient pins for sensors and role detection

#### 2.1.2 Sensor Hardware per Module

**All Modules:**
- **GPS**: u-blox ZED-F9P (RTK-capable, UART connection)
- **IMU**: SparkFun BNO080 (I2C, 400kHz)
- **Radar**: SparkFun Acconeer XM125 (I2C, distance detection firmware)
- **Display**: PiicoDev OLED SSD1306 128x64 (I2C, diagnostics)

**Centre Module Only:**
- **ADC**: Adafruit ADA1085 (ADS1115, 4-channel 16-bit, I2C)
- **Ram Potentiometers**: 3x analog inputs (Centre, Left, Right rams)

#### 2.1.3 Role Detection Hardware
- **DIP Switch**: 5-way DIP switch on pins 2-6
- **Configuration**: Only one pin tied to GND per module
- **Roles**: Left (pin 2), Centre (pin 3), Right (pin 4), Spare1 (pin 5), Spare2 (pin 6)

### 2.2 Network Architecture

```
Internet/Base Station
         │
    RTCM Corrections
         │
         ▼
┌─────────────────┐    UDP 8888     ┌──────────────────┐
│  Toughbook      │◄──────────────► │  Centre Module   │
│  (ABLS.Core)    │                 │  (Master)        │
└─────────────────┘                 └──────────────────┘
         ▲                                   │
         │                                   │ RTCM Broadcast
         │ Sensor Data (50Hz)                ▼
         │                          ┌──────────────────┐
         └──────────────────────────►│  Left Module     │
                                    │  Right Module    │
                                    └──────────────────┘
```

---

## 3. Firmware Architecture

### 3.1 Unified Firmware Design

The ABLS system uses a **single unified firmware** (`ABLSModule`) that runs on all three Teensy 4.1 modules. Module-specific functionality is enabled based on hardware role detection at startup.

```cpp
// Role detection at startup
ModuleRole detectedRole = ModuleConfig::detectRole();
switch (detectedRole) {
    case ModuleRole::LEFT_WING:
        // Enable wing-specific features
        break;
    case ModuleRole::CENTRE:
        // Enable hydraulic control + sensor fusion
        break;
    case ModuleRole::RIGHT_WING:
        // Enable wing-specific features
        break;
}
```

### 3.2 Main Firmware Loop

```cpp
void loop() {
    // 1. Update all sensors (100Hz base rate)
    sensorManager.update();
    
    // 2. Process network communications
    networkManager.processIncomingPackets();
    
    // 3. Role-specific processing
    if (moduleRole == ModuleRole::CENTRE) {
        hydraulicController.update();
        rtcmManager.distributeCorrections();
    }
    
    // 4. Send sensor data (50Hz)
    if (shouldSendSensorData()) {
        networkManager.sendSensorData();
    }
    
    // 5. Update diagnostics and logging
    diagnosticManager.update();
    
    // 6. Handle firmware updates
    if (updateManager.hasUpdateRequest()) {
        updateManager.processUpdate();
    }
}
```

### 3.3 Module Roles and Responsibilities

#### 3.3.1 Left Wing Module
- **Primary Function**: Sensor fusion and data transmission
- **Sensors**: GPS (10Hz), IMU (100Hz), Radar
- **Processing**: Dead reckoning sensor fusion
- **Output**: Sensor data packet at 50Hz to Toughbook
- **GPS Model**: AIRBORNE1G (for wing tip dynamics)

#### 3.3.2 Centre Module (Master)
- **Primary Function**: Hydraulic control + sensor fusion
- **Sensors**: GPS (10Hz), IMU (100Hz), Radar, Ram potentiometers (3x)
- **Processing**: PID hydraulic control, RTCM distribution
- **Input**: Control commands from Toughbook
- **Output**: Sensor data + ram positions at 50Hz
- **GPS Model**: AUTOMOTIVE (for tractor chassis)

#### 3.3.3 Right Wing Module
- **Primary Function**: Sensor fusion and data transmission
- **Sensors**: GPS (10Hz), IMU (100Hz), Radar
- **Processing**: Dead reckoning sensor fusion
- **Output**: Sensor data packet at 50Hz to Toughbook
- **GPS Model**: AIRBORNE1G (for wing tip dynamics)

---

## 4. Sensor Systems

### 4.1 GPS System (u-blox ZED-F9P)

#### 4.1.1 Configuration
```cpp
// GPS initialization parameters
_gps.setNavigationFrequency(10);  // 10Hz update rate
_gps.setDynamicModel(moduleRole == ModuleRole::CENTRE ? 
                     DYN_MODEL_AUTOMOTIVE : DYN_MODEL_AIRBORNE1g);
_gps.setUART1Output(COM_TYPE_UBX);  // UBX protocol
_gps.saveConfiguration();
```

#### 4.1.2 High-Precision Data Access
```cpp
// High-precision position data (sub-centimeter accuracy)
if (_gps.getHPPOSLLH()) {
    double latitude = _gps.getHighResLatitude() + _gps.getHighResLatitudeHp();
    double longitude = _gps.getHighResLongitude() + _gps.getHighResLongitudeHp();
    double altitude = _gps.getElipsoid() + _gps.getElipsoidHp();
    uint32_t accuracy = _gps.getHorizontalAccuracy();  // mm
}
```

#### 4.1.3 RTK Corrections
- **Source**: Centralized radio modem to Centre module
- **Distribution**: Centre module broadcasts RTCM via UDP to all modules
- **Injection**: Each module forwards RTCM to local GPS via UART
- **Accuracy**: Sub-centimeter when RTK fixed

### 4.2 IMU System (SparkFun BNO080)

#### 4.2.1 Agricultural-Specific Configuration
```cpp
// IMU sensor enables (magnetometer disabled for metal boom)
_imu.enableRotationVector(20);        // 50Hz quaternion
_imu.enableAccelerometer(10);         // 100Hz raw acceleration
_imu.enableGyro(10);                  // 100Hz angular velocity
_imu.enableLinearAccelerometer(20);   // 50Hz gravity-compensated
_imu.enableGameRotationVector(50);    // 20Hz backup orientation

// Dynamic calibration (no magnetometer due to metal interference)
_imu.enableDynamicCalibration(true);
```

#### 4.2.2 Data Processing with Accuracy Monitoring
```cpp
// Quaternion orientation (primary)
if (_imu.dataAvailable()) {
    float qI = _imu.getQuatI();
    float qJ = _imu.getQuatJ(); 
    float qK = _imu.getQuatK();
    float qReal = _imu.getQuatReal();
    byte accuracy = _imu.getQuatAccuracy();  // 0=unreliable, 3=high
    
    // Backup orientation when primary accuracy poor
    if (accuracy < 2) {
        // Use game rotation vector (no magnetometer dependency)
        qI = _imu.getGameQuatI();
        // ... etc
    }
}
```

### 4.3 Radar System (SparkFun Acconeer XM125)

#### 4.3.1 Agricultural Configuration
```cpp
// Range optimized for boom heights (10cm to 3m)
_radar.setStart(100);    // 100mm minimum
_radar.setEnd(3000);     // 3000mm maximum

// Threshold tuning for agricultural environment
_radar.setThresholdSensitivity(200);  // Reduce noise from spray/dust
_radar.setFixedAmpThreshold(150);     // Signal strength filtering
```

#### 4.3.2 Multi-Peak Detection for Agriculture
```cpp
// Agricultural-specific processing
if (_radar.dataReady()) {
    uint8_t numDistances = _radar.getNumberDistances();
    
    for (uint8_t i = 0; i < numDistances; i++) {
        uint16_t distance;
        uint16_t strength;
        
        if (_radar.getPeakDistance(i, &distance) == 0) {
            _radar.getPeakStrength(i, &strength);
            
            // Filter weak signals (spray droplets, dust)
            if (strength >= 100) {
                // Valid ground/crop reflection
                processRadarPeak(distance, strength);
            }
        }
    }
}
```

### 4.4 Hydraulic Ram Potentiometers (Centre Module Only)

#### 4.4.1 ADC Configuration
```cpp
// ADS1115 16-bit ADC configuration
_adc.setGain(GAIN_TWOTHIRDS);  // +/- 6.144V range
_adc.setDataRate(RATE_ADS1115_860SPS);  // 860 samples/second
_adc.setMode(MODE_CONTINUOUS);  // Continuous conversion
```

#### 4.4.2 Ram Position Calculation
```cpp
// Convert ADC reading to ram position percentage
float calculateRamPosition(uint8_t channel) {
    int16_t adcValue = _adc.readADC_SingleEnded(channel);
    float voltage = adcValue * 0.1875 / 1000.0;  // Convert to volts
    
    // Linear calibration: 0.5V = 0%, 4.5V = 100%
    float percentage = (voltage - 0.5) / 4.0 * 100.0;
    
    // Clamp to valid range
    return constrain(percentage, 0.0, 100.0);
}
```

#### 4.4.3 Ram-to-Angle Conversion
```cpp
// Convert ram position to boom angle (geometry-dependent)
double calculateBoomAngleFromRam(float ramPercent) {
    // Simplified linear model (would be enhanced with actual boom geometry)
    // 50% = level (0°), 0% = max down (-15°), 100% = max up (+15°)
    return (ramPercent - 50.0) * 0.3;  // 0.3 degrees per percent
}
```

---

**Continue to Part 2 for Network Communication, Data Structures, and Classes...**
