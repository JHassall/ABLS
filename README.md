# ABLS - Automatic Agricultural Boomspray Levelling System

**Advanced Precision Agriculture Boom Control for John Deere 4030 Boomspray**  
**Integrated with AgOpenGPS Development Framework**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Teensy%204.1-blue.svg)](https://www.pjrc.com/store/teensy41.html)
[![Framework](https://img.shields.io/badge/Framework-AgOpenGPS-purple.svg)](https://github.com/farmerbriantee/AgOpenGPS)
[![Status](https://img.shields.io/badge/Status-Active%20Development-green.svg)](https://github.com/JHassall/ABLS)

## Overview

ABLS is a comprehensive multi-sensor boom leveling system designed for precision agricultural applications, integrated with the AgOpenGPS development framework. The system uses advanced sensor fusion combining GPS RTK, IMU, radar, and hydraulic ram feedback to maintain optimal boom height and level across varying terrain conditions.

**🔗 AgOpenGPS Integration**: This project builds upon the proven AgOpenGPS platform to provide boom control capabilities while maintaining compatibility with the broader AgOpenGPS ecosystem and contributing developments back to the community.

### Key Features

🎯 **Multi-Sensor Fusion**: GPS RTK + IMU + Radar + Hydraulic Ram Potentiometers  
🌐 **RTK Precision**: Sub-centimeter accuracy with ZED-F9P GPS modules  
🔄 **Real-Time Control**: 100Hz hydraulic control with PID optimization  
📡 **Network Architecture**: Robust UDP communication with error recovery  
🔧 **Professional Calibration**: Multi-point sensor correlation with manual level setting  
🚀 **Network Updates**: Enterprise-grade firmware update system with rollback  
📊 **Comprehensive Diagnostics**: Real-time health monitoring and error logging  
🤝 **Community Integration**: Built on AgOpenGPS framework for ecosystem compatibility

## System Architecture

### Hardware Configuration

- **3x Teensy 4.1 Modules**: Left Wing, Centre (Master), Right Wing
- **GPS**: u-blox ZED-F9P RTK receivers (10Hz, sub-cm accuracy)
- **IMU**: SparkFun BNO080 9-DOF sensors (100Hz, magnetometer disabled for metal boom)
- **Radar**: SparkFun Acconeer XM125 distance sensors (agricultural range/threshold tuning)
- **Hydraulics**: 3x PID-controlled rams with potentiometer feedback via ADS1115 ADC
- **Network**: Ethernet to Panasonic Toughbook with QNEthernet library
- **Diagnostics**: PiicoDev OLED displays + SD card logging per module

### Software Stack

- **Unified Firmware**: Single codebase with GPIO role detection (DIP switch configuration)
- **Sensor Fusion**: Dead reckoning with GPS/IMU integration at 50Hz output
- **Network Protocol**: UDP-based with packet validation and error recovery
- **Update System**: Network-based dual-bank flash with SHA256 verification and automatic rollback
- **Calibration**: Multi-sensor correlation with manual level reference setting

## Current Development Status

### ✅ Completed Features

#### Firmware Architecture
- [x] Unified firmware for all three modules with role detection
- [x] Complete sensor integration (GPS, IMU, Radar, ADC)
- [x] Network communication with robust error handling
- [x] PID hydraulic control system
- [x] RTCM correction distribution from centre to wings
- [x] Comprehensive diagnostic logging and OLED display
- [x] SD card logging for crash diagnostics

#### Multi-Sensor Calibration System
- [x] **Professional WPF Calibration Application** (.NET 8.0)
- [x] **Manual Level Setting Workflow** (installer measures boom tip heights)
- [x] **Triple-Sensor Integration** (IMU + Ram Potentiometers + Radar)
- [x] **Real-Time Multi-Sensor Correlation** with agreement analysis
- [x] **4-Step Calibration Process** (Manual Level → Level → Max Up → Max Down)
- [x] **Agricultural-Specific Features** (vegetation warnings, field-appropriate UI)

#### Firmware Update System (RgFModuleUpdate)
- [x] **Dual-Bank Flash Architecture** with automatic rollback
- [x] **Network-Based Updates** via HTTP download and UDP control
- [x] **SHA256 Integrity Verification** for firmware images
- [x] **Safety Checks** (machine stationary, hydraulics idle)
- [x] **Version Management** with persistent storage
- [x] **Update Safety Manager** with timeout and error recovery

#### Technical Documentation
- [x] **Comprehensive 3-Part Technical Reference** (100+ pages)
  - Part 1: System Architecture & Hardware Specifications
  - Part 2: Network Communication & Data Structures
  - Part 3: Firmware Updates & Integration
- [x] **Complete API Documentation** with code examples
- [x] **Field Deployment Procedures** and troubleshooting guides
- [x] **Integration Guidelines** for ABLS.Core and Toughbook

### 🚧 In Progress

- [ ] **Sequential Module Update Workflow** (one module at a time)
- [ ] **Version Synchronization** across all modules at startup
- [ ] **Hardware Testing** with actual Teensy modules and sensors
- [ ] **Field Validation** of multi-sensor calibration accuracy

### 📋 Planned Features

- [ ] **AgOpenGPS Integration** (Toughbook application)
- [ ] **DEM Terrain Integration** for predictive boom control

- [ ] **Advanced Control Algorithms** with machine learning optimization

## Quick Start

### Prerequisites

- **Hardware**: 3x Teensy 4.1 with sensor modules
- **Software**: Arduino IDE with required libraries
- **Network**: Ethernet connection to Toughbook
- **Calibration**: Windows PC with .NET 8.0 runtime

### Installation

1. **Flash Firmware**:
   ```bash
   # Configure DIP switches for module roles
   # Left Wing: Pin 2 to GND
   # Centre: Pin 3 to GND  
   # Right Wing: Pin 4 to GND
   
   # Upload unified firmware to all modules
   arduino --upload src/Firmware/ABLSModule/ABLSModule.ino
   ```

2. **Network Setup**:
   ```bash
   # Configure Toughbook Ethernet
   # DHCP or static IP (192.168.1.x range)
   # Verify UDP ports 8888-8892 are open
   ```

3. **Sensor Calibration**:
   ```bash
   # Build and run calibration app
   cd src/ABLSIMUCalibrator
   dotnet build
   dotnet run
   
   # Follow 4-step calibration workflow
   # 1. Manual level reference setting
   # 2. Level position capture
   # 3. Maximum up position
   # 4. Maximum down position
   ```

### Configuration

**Module Role Detection** (DIP Switch):
- Pin 2 → GND: Left Wing Module
- Pin 3 → GND: Centre Module
- Pin 4 → GND: Right Wing Module
- Pin 5 → GND: Spare Module 1
- Pin 6 → GND: Spare Module 2

**Network Ports**:
- 8888: Sensor data (Module → Toughbook)
- 8889: Control commands (Toughbook → Centre)
- 8890: RTCM corrections
- 8891: Firmware updates
- 8892: Diagnostic data

## Technical Documentation

Comprehensive technical documentation is available in the `docs/` directory:

- **[Part 1: System Architecture](docs/ABLS_Firmware_Technical_Reference_Part1.md)**
- **[Part 2: Network Communication & Data Structures](docs/ABLS_Firmware_Technical_Reference_Part2.md)**
- **[Part 3: Firmware Updates & Integration](docs/ABLS_Firmware_Technical_Reference_Part3.md)**

## Project Structure

```
ABLS/
├── docs/                           # Technical documentation
│   ├── ABLS_Firmware_Technical_Reference_Part1.md
│   ├── ABLS_Firmware_Technical_Reference_Part2.md
│   └── ABLS_Firmware_Technical_Reference_Part3.md
├── src/
│   ├── Firmware/
│   │   └── ABLSModule/             # Unified Teensy firmware
│   │       ├── ABLSModule.ino      # Main firmware file
│   │       ├── SensorManager.cpp   # Multi-sensor integration
│   │       ├── NetworkManager.cpp  # UDP communication
│   │       ├── HydraulicController.cpp # PID control
│   │       ├── DiagnosticManager.cpp   # Logging & health
│   │       └── FlasherX/           # Network update system
│   ├── ABLSIMUCalibrator/          # WPF calibration application
│   │   ├── MainWindow.xaml         # Professional UI
│   │   ├── MainWindow.xaml.cs      # Multi-sensor calibration logic
│   │   └── ABLSIMUCalibrator.csproj
│   ├── ABLS.Core/                  # AgOpenGPS integration (planned)
│   └── RgFModuleUpdater/           # Firmware update utility
└── README.md
```

## Hardware Requirements

### Teensy 4.1 Modules (3x)
- **MCU**: 600 MHz ARM Cortex-M7
- **Memory**: 8MB Flash (dual-bank), 1MB RAM
- **Connectivity**: Ethernet, I2C, UART, SPI
- **Storage**: SD card slot for logging

### Sensors Per Module
- **GPS**: u-blox ZED-F9P (UART, RTK capable)
- **IMU**: SparkFun BNO080 (I2C, 9-DOF with sensor fusion)
- **Radar**: SparkFun Acconeer XM125 (I2C, distance detection)
- **Display**: PiicoDev OLED SSD1306 (I2C, diagnostics)

### Centre Module Additional
- **ADC**: Adafruit ADS1115 (I2C, 16-bit, 4-channel)
- **Hydraulic Rams**: 3x with potentiometer feedback
- **RTCM Radio**: For GPS corrections (UART)

## Contributing

Contributions are welcome!

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

### Open Source Libraries and Hardware

- **Joe Pasquariello** for the excellent [FlasherX](https://github.com/joepasquariello/FlasherX) network update system
- **SparkFun Electronics** ([GitHub](https://github.com/sparkfun)) for excellent sensor modules and Arduino libraries
- **Nathan Seidle** ([GitHub](https://github.com/nseidle)) for SparkFun library development and open hardware leadership
- **Paul Z Clark** ([GitHub](https://github.com/PaulZC)) for SparkFun u-blox library contributions and GPS expertise
- **PJRC (Teensy)** for the powerful Teensy 4.1 platform and ecosystem
- **Adafruit Industries** for the ADS1115 ADC library and hardware modules


### Specific Library Credits

- **SparkFun u-blox GNSS Arduino Library** - High-precision GPS integration
- **SparkFun BNO080 Arduino Library** - IMU sensor fusion and calibration
- **SparkFun Qwiic XM125 Arduino Library** - Radar distance detection
- **QNEthernet Library** - Teensy 4.1 Ethernet networking
- **Adafruit ADS1115 Library** - 16-bit ADC for hydraulic ram sensing

### AI-Assisted Development

- **Simtheory** ([simtheory.ai](https://simtheory.ai/)) and the creators of "Very Average Podcast" - Chris and Michael Sharkey ([This Day in AI](https://podcast.thisdayinai.com/)) for their inspiration and advocacy for practical AI applications in Australian innovation
- **Claude Sonnet** - AI assistance for rapid prototyping, code generation, and technical documentation
- **Windsurf** - AI-powered development environment enabling accelerated professional development

*This project demonstrates the power of AI-assisted development in creating sophisticated agricultural technology solutions quickly and professionally.*

## Support

For questions, issues, or contributions:

- **GitHub Issues**: [Report bugs or request features](https://github.com/JHassall/ABLS/issues)
- **Documentation**: See `docs/` directory for technical details


---

**ABLS - Precision Agriculture Through Advanced Sensor Fusion** 🚜🌾
