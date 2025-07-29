# ABLS IMU Calibrator

**Professional IMU Calibration Application for ABLS Agricultural Boom Control System**

## Overview

The ABLS IMU Calibrator is a standalone WPF application designed to provide a user-driven calibration workflow for the Inertial Measurement Units (IMUs) in the ABLS (Automatic Boom Level System) agricultural boom control system. This application ensures precise boom angle measurement by establishing accurate reference frames through controlled calibration positions.

## Features

### 🎯 **Professional Calibration Workflow**
- **3-Point Calibration System**: Level, Maximum Up, and Maximum Down positions
- **User-Guided Process**: Step-by-step instructions with position confirmation
- **Real-time Validation**: Live IMU data display during calibration
- **Multi-Module Support**: Calibrates Left Wing, Centre, and Right Wing modules simultaneously

### 📊 **Real-time Monitoring**
- **Live IMU Data**: Quaternion, linear acceleration, and gyroscope readings
- **Accuracy Assessment**: Real-time accuracy monitoring for all sensor types
- **Boom Angle Calculation**: Live boom angle display from IMU quaternion data
- **Connection Status**: Visual indicators for module connectivity

### 🛡️ **Robust Communication**
- **UDP Network Protocol**: Reliable communication with ABLS modules
- **Error Handling**: Comprehensive error detection and recovery
- **Status Monitoring**: Continuous module health monitoring
- **Timeout Protection**: Network communication timeout handling

### 🔧 **Agricultural-Specific Features**
- **Metal Boom Compensation**: Designed for metal boom mounting (magnetometer disabled)
- **Field Environment**: Optimized for agricultural conditions and EMI
- **Professional UI**: Clean, professional interface suitable for field use
- **Diagnostic Logging**: Comprehensive status and error logging

## System Requirements

- **Operating System**: Windows 10/11
- **Framework**: .NET 8.0
- **Network**: UDP connectivity to ABLS modules (default: port 8888)
- **Hardware**: ABLS Teensy 4.1 modules with BNO080 IMU sensors

## Installation

1. **Build the Application**:
   ```bash
   cd c:\RgF\ABLS\src\ABLSIMUCalibrator
   dotnet build
   ```

2. **Run the Application**:
   ```bash
   dotnet run
   ```

## Usage

### 1. **Connect to ABLS Modules**
- Enter the target IP address of the ABLS system (default: 192.168.1.100)
- Click "Connect" to establish communication with all modules
- Verify all modules show "ONLINE" status

### 2. **Perform Calibration**
The calibration process requires three specific boom positions:

#### **Step 1: Level Position**
- Position the boom completely **LEVEL** (horizontal)
- Ensure all boom sections are aligned horizontally
- Click "Capture Level Position"
- Wait for confirmation from all modules

#### **Step 2: Maximum Up Position**
- Raise the boom to its **MAXIMUM UP** angle
- This should be the highest operational position
- Click "Capture Max Up Position"
- Wait for confirmation from all modules

#### **Step 3: Maximum Down Position**
- Lower the boom to its **MAXIMUM DOWN** angle
- This should be the lowest operational position
- Click "Capture Max Down Position"
- Wait for confirmation from all modules

### 3. **Complete Calibration**
- Once all three positions are captured, click "Complete Calibration"
- The system will calculate and store calibration parameters
- Verify successful completion on all modules

### 4. **Monitor Real-time Data**
- **IMU Accuracy**: Monitor sensor accuracy levels (0=Unreliable, 3=High)
- **Live Readings**: View real-time quaternion, acceleration, and gyroscope data
- **Boom Angles**: See calculated boom angles based on current calibration

## Network Protocol

The application communicates with ABLS modules using UDP packets on port 8888:

### **Commands Sent to Modules**:
- `CONNECT_IMU_CALIBRATOR` - Establish calibration session
- `CAPTURE_LEVEL_POSITION` - Capture level reference position
- `CAPTURE_MAX_UP_POSITION` - Capture maximum up position
- `CAPTURE_MAX_DOWN_POSITION` - Capture maximum down position
- `COMPLETE_IMU_CALIBRATION` - Finalize and save calibration
- `ABORT_IMU_CALIBRATION` - Abort calibration process
- `REQUEST_MODULE_STATUS` - Request module status update

### **Responses from Modules**:
- `MODULE_STATUS:ModuleID:ONLINE/OFFLINE` - Module connectivity status
- `IMU_DATA:ModuleID:qI,qJ,qK,qReal,aX,aY,aZ,gX,gY,gZ,lX,lY,lZ,qAcc,aAcc,gAcc` - Real-time IMU data
- `CALIBRATION_CAPTURED:ModuleID:Position` - Calibration position captured
- `CALIBRATION_COMPLETE:ModuleID:Status` - Calibration completion status

## Technical Details

### **IMU Data Format**
Real-time IMU data is transmitted as comma-separated values:
- **Quaternion**: qI, qJ, qK, qReal (orientation)
- **Raw Acceleration**: aX, aY, aZ (m/s²)
- **Gyroscope**: gX, gY, gZ (deg/s)
- **Linear Acceleration**: lX, lY, lZ (m/s², gravity-compensated)
- **Accuracy Values**: qAcc, aAcc, gAcc (0-3 scale)

### **Calibration Mathematics**
The calibration process establishes:
1. **Level Reference**: Zero-angle reference frame
2. **Angular Range**: Maximum up/down operational angles
3. **Offset Compensation**: Installation and mechanical variation compensation
4. **Accuracy Validation**: Sensor accuracy requirements for reliable operation

## Integration with ABLS.Core

This standalone application is designed for independent development and testing. Once mature, the calibration functionality will be integrated into the main ABLS.Core application for seamless operation.

### **Integration Points**:
- **Network Protocol**: Compatible with existing ABLS UDP communication
- **Data Structures**: Uses same IMU data formats as main system
- **Calibration Storage**: Calibration data stored in module EEPROM/Flash
- **Status Reporting**: Integrates with existing module status system

## Troubleshooting

### **Connection Issues**
- Verify network connectivity to ABLS modules
- Check firewall settings for UDP port 8888
- Ensure modules are powered and operational
- Verify IP address configuration

### **Calibration Issues**
- Ensure boom is completely stationary during position capture
- Verify IMU accuracy is adequate (accuracy ≥ 2 recommended)
- Check for magnetic interference near modules
- Ensure hydraulic system is not under pressure during calibration

### **Data Quality Issues**
- Monitor IMU accuracy values continuously
- Look for consistent quaternion readings
- Verify linear acceleration shows expected gravity vector
- Check for communication timeouts or data corruption

## Development Notes

This application serves as a prototype for IMU calibration functionality that will eventually be integrated into the main ABLS.Core system. The standalone approach allows for:

- **Independent Development**: Focused development without affecting main system
- **Field Testing**: Easy deployment for calibration testing
- **User Interface Refinement**: UI/UX optimization before integration
- **Protocol Validation**: Network communication protocol testing

## Support

For technical support or questions about the ABLS IMU Calibrator, please refer to the main ABLS project documentation or contact the development team.

---

**ABLS IMU Calibrator v1.0**  
*Professional Agricultural Boom Control System*  
*Copyright © 2025 RgF Agricultural Systems*
