# ABLS Unified Module Firmware

## Overview
This unified firmware runs on all three ABLS modules (Left Wing, Centre, Right Wing) with hardware-based role detection. The same firmware binary is flashed to all modules, and the module role is automatically detected at startup based on GPIO pin configuration.

## Hardware Configuration

### Diagnostic Hardware (Development)

**PiicoDev OLED Display (SSD1306, 128x64):**
- **Connection**: I2C bus (shared with IMU and radar sensors)
- **Address**: 0x3C
- **Purpose**: Real-time diagnostics display during development
- **Display Content**: Module role, system status, network info, sensor data, errors
- **Auto-cycling**: 4 pages, changes every 5 seconds

**SD Card Logging:**
- **Hardware**: Teensy 4.1 built-in SD card slot
- **Purpose**: Comprehensive logging for crash analysis and behavior diagnosis
- **Log Location**: `/logs/abls_XXX.log` (daily log files)
- **Log Content**: Startup events, role detection, errors, runtime events, crashes, sensor data
- **Log Levels**: DEBUG, INFO, WARNING, ERROR, CRITICAL

### Module Role Detection
The module role is determined by a 5-way DIP switch configuration using pins 2-6. Only one pin should be tied to GND during installation:

| DIP Position | Pin | GND Connection | Module Role | Features |
|--------------|-----|----------------|-------------|----------|
| 0            | 2   | Tied to GND    | Left Wing   | Sensor fusion, RTCM receiving, Airborne GPS |
| 1            | 3   | Tied to GND    | Centre      | Hydraulic control, RTCM broadcasting, Automotive GPS |
| 2            | 4   | Tied to GND    | Right Wing  | Sensor fusion, RTCM receiving, Airborne GPS |
| 3            | 5   | Tied to GND    | Spare 3     | Future expansion |
| 4            | 6   | Tied to GND    | Spare 4     | Future expansion |

### Installation Instructions
1. Flash the same firmware to all Teensy 4.1 modules
2. Configure the 5-way DIP switch for each module:
   - **Left Wing Module**: Set DIP position 0 (Pin 2 to GND)
   - **Centre Module**: Set DIP position 1 (Pin 3 to GND)
   - **Right Wing Module**: Set DIP position 2 (Pin 4 to GND)
   - **Future modules**: Use positions 3-4 as needed
3. Power on the module - it will automatically detect its role and configure accordingly

### DIP Switch Benefits
- **Professional installation**: Clean, reliable configuration method
- **Future expansion**: Support for up to 5 different module types
- **No UART conflicts**: Avoids Pin 1 (TX1) used for GPS communication
- **Easy troubleshooting**: Clear visual indication of module configuration

## Features by Module Role

### All Modules
- GPS with RTK quality monitoring
- IMU (BNO080) for orientation
- Radar (XM125) for distance measurement
- Ethernet communication with Toughbook
- OTA firmware update capability (future)

### Wing Modules (Left & Right)
- Advanced sensor fusion with dead reckoning
- GPS dynamic model: Airborne 1G (optimized for wing tip motion)
- RTCM correction receiving from Centre module
- 50Hz sensor data output to Toughbook

### Centre Module
- Hydraulic control with PID controllers
- GPS dynamic model: Automotive (optimized for tractor chassis)
- RTCM correction broadcasting to Wing modules
- Control command processing from Toughbook
- Sensor data + hydraulic status output to Toughbook

## Development Status
- [x] Project structure created
- [ ] ModuleConfig role detection implementation
- [ ] Sensor management integration
- [ ] Network communication
- [ ] OTA update system
- [ ] Hardware testing

## Compilation
```bash
arduino-cli compile --fqbn teensy:avr:teensy41 ABLSModule.ino
```

## Dependencies
- SparkFun u-blox GNSS v3 Library
- SparkFun BNO080 Arduino Library
- SparkFun Qwiic XM125 Arduino Library
- QNEthernet Library
- ArduinoJson Library

## Version
Current Version: 1.0.0-dev
Build Date: TBD
Git Hash: TBD
