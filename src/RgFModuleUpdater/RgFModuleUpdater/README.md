# RgF Module Updater

**Standalone Windows Application for ABLS Firmware Updates**

## Overview

The RgF Module Updater is a standalone Windows console application that orchestrates firmware updates for ABLS Teensy 4.1 modules. It provides a safe, sequential update process with comprehensive safety checks and version synchronization.

## Features

### Core Functionality
- **Sequential Module Updates**: Updates one module at a time to prevent system instability
- **HTTP Firmware Server**: Serves .hex firmware files to modules over local network
- **SHA256 Verification**: Cryptographic integrity checking of firmware images
- **Status Monitoring**: Real-time module status and version reporting
- **Safety Checks**: Ensures system is stationary before allowing updates
- **Progress Tracking**: Monitors update progress and completion status

### Safety Features
- **Machine Stationary Detection**: Prevents updates during operation
- **Version Synchronization**: Ensures all modules run matching firmware versions
- **Rollback Capability**: Automatic rollback on update failures
- **Timeout Handling**: Prevents hung updates from blocking the system
- **Comprehensive Logging**: Detailed audit trail of all update operations

## System Requirements

- **Operating System**: Windows 10/11 or Windows Server 2019+
- **.NET Runtime**: .NET 6.0 or later
- **Network**: Local Ethernet connection to ABLS modules
- **Permissions**: Administrator privileges for HTTP server binding

## Network Configuration

### Default Network Settings
- **Toughbook IP**: 192.168.1.100
- **Module IPs**:
  - Left Wing: 192.168.1.101
  - Centre: 192.168.1.102
  - Right Wing: 192.168.1.103

### Port Configuration
- **HTTP Server**: Port 8080 (firmware downloads)
- **UDP Command**: Port 12346 (update commands)
- **UDP Status**: Port 12347 (status reporting)

## Installation

1. **Download/Build Application**:
   ```bash
   dotnet build -c Release
   ```

2. **Create Firmware Directory**:
   - Application automatically creates `firmware/` directory
   - Place .hex firmware files in this directory

3. **Run as Administrator**:
   ```bash
   # Run from command prompt as Administrator
   RgFModuleUpdater.exe
   ```

## Usage

### Interactive Menu
The application provides an interactive console menu:

```
=== RgF Module Updater Menu ===
1. Check module status
2. Update all modules
3. Update specific module
4. List firmware files
5. Exit
```

### Update Process

#### 1. Check Module Status
- Queries all modules for current status
- Displays version, uptime, and memory information
- Identifies modules that need updates

#### 2. Update All Modules (Recommended)
- Lists available firmware files
- Calculates SHA256 hash for integrity verification
- Performs sequential updates (Left → Centre → Right)
- Verifies successful completion before proceeding

#### 3. Update Specific Module
- Allows updating individual modules
- Useful for testing or troubleshooting
- Same safety checks as full update

#### 4. List Firmware Files
- Shows all .hex files in firmware directory
- Displays file size and modification date
- Helps verify firmware availability

## Firmware File Management

### Supported Formats
- **Intel HEX Format**: Standard .hex files from Arduino IDE
- **Naming Convention**: Use descriptive names (e.g., `ABLS_v1.2.3.hex`)

### File Placement
1. Place .hex files in the `firmware/` directory
2. Ensure files are not corrupted or truncated
3. Use consistent naming for version tracking

### SHA256 Verification
- Application automatically calculates SHA256 hash
- Hash is sent to modules for integrity verification
- Prevents installation of corrupted firmware

## Safety Protocols

### Pre-Update Checks
- **System Status**: Verifies all modules are operational
- **Network Connectivity**: Confirms communication with all modules
- **Stationary Detection**: Ensures machine is not moving
- **Version Compatibility**: Checks firmware compatibility

### During Update
- **Sequential Processing**: One module at a time
- **Progress Monitoring**: Real-time update status
- **Timeout Protection**: Prevents hung updates
- **Error Handling**: Immediate abort on failures

### Post-Update Verification
- **Version Synchronization**: Confirms all modules match
- **Operational Status**: Verifies modules are functional
- **System Re-enablement**: Only after all modules report ready

## Troubleshooting

### Common Issues

#### HTTP Server Fails to Start
- **Cause**: Port 8080 already in use or insufficient permissions
- **Solution**: Run as Administrator or change port in code

#### Module Not Responding
- **Cause**: Network connectivity or module failure
- **Solution**: Check network cables and module power

#### Update Timeout
- **Cause**: Large firmware file or slow network
- **Solution**: Increase timeout values or check network speed

#### SHA256 Mismatch
- **Cause**: Corrupted firmware file or transmission error
- **Solution**: Re-download firmware file and retry

### Debug Information
- Application provides detailed console output
- Monitor network traffic for communication issues
- Check module OLED displays for status information

## Integration with ABLS.Core

This standalone application serves as a prototype for the full ABLS.Core integration:

### Future Integration Points
- **UI Integration**: Console interface → ABLS.Core GUI
- **Database Integration**: Update history and version tracking
- **Authentication**: User permissions and access control
- **Scheduling**: Automated update scheduling
- **Remote Monitoring**: Integration with fleet management

### Migration Path
1. **Phase 1**: Standalone testing and validation
2. **Phase 2**: API extraction for ABLS.Core integration
3. **Phase 3**: Full GUI integration with ABLS.Core
4. **Phase 4**: Production deployment with enterprise features

## Development Notes

### Architecture
- **Modular Design**: Easy to extract components for integration
- **Async/Await**: Non-blocking operations for responsiveness
- **Error Handling**: Comprehensive exception management
- **Logging**: Structured logging for debugging

### Extension Points
- **Custom Protocols**: Easy to add new communication methods
- **Additional Safety Checks**: Extensible safety validation
- **Progress Reporting**: Pluggable progress notification
- **Authentication**: Ready for security integration

## Version History

- **v1.0.0**: Initial standalone implementation
  - Basic update orchestration
  - HTTP firmware server
  - SHA256 verification
  - Sequential update workflow

## Support

For technical support or feature requests, contact RgF Engineering.

---

**Author**: RgF Engineering  
**Version**: 1.0.0  
**Last Updated**: July 2025
