# RgF Module Updater - WPF User Interface Guide

## Overview

The RgF Module Updater now features a modern WPF (Windows Presentation Foundation) graphical user interface that provides an intuitive, user-friendly experience for managing ABLS firmware updates.

## UI Layout

### Header Section
- **Application Title**: "RgF Module Updater v1.0.0"
- **Subtitle**: "ABLS Firmware Update System"
- **Author**: "RgF Engineering"
- **Professional dark blue header** with white text

### Main Interface (Two-Panel Layout)

#### Left Panel - Control Center
**Module Status Cards**
- **Three status cards** for Left Wing, Centre, and Right Wing modules
- **Real-time status display**: OPERATIONAL, OFFLINE, etc.
- **Version information**: Current firmware version
- **IP addresses**: Network endpoints for each module
- **Color-coded status**: Green (operational), Red (offline), Orange (warning)

**Action Buttons**
- **Refresh Status**: Manual status check for all modules
- **Firmware dropdown**: Select from available .hex files
- **Refresh Firmware List**: Scan firmware directory
- **Update All Modules**: Sequential update of all three modules
- **Update Selected Module**: Individual module updates (coming soon)
- **Open Firmware Folder**: Direct access to firmware directory

#### Right Panel - System Log
**Real-time Logging**
- **Timestamped messages**: All system activities with timestamps
- **Scrollable output**: Auto-scroll to latest messages
- **Clear log button**: Reset log display
- **Monospace font**: Easy-to-read console-style output

**Progress Tracking**
- **Progress bar**: Visual update progress indicator
- **Status label**: Current operation description
- **Percentage completion**: Real-time progress updates

### Status Bar
- **System status**: Current operational state
- **HTTP server indicator**: Green/Red status light
- **Server status text**: Online/Offline indication
- **Port information**: HTTP server port details

## Key Features

### 1. **Real-Time Module Monitoring**
- Automatic status polling every 5 seconds
- Manual refresh capability
- Visual status indicators with color coding
- Network connectivity monitoring

### 2. **Firmware Management**
- Automatic firmware file discovery
- Dropdown selection of available .hex files
- File information display (size, date)
- Direct access to firmware directory

### 3. **Sequential Update Process**
- **Safety confirmation dialog** before updates
- **Step-by-step progress tracking**
- **SHA256 hash calculation and verification**
- **Module-by-module update sequence** (Left → Centre → Right)
- **Automatic rollback** on failures

### 4. **Professional User Experience**
- **Modern, clean interface** with professional color scheme
- **Intuitive button layout** with clear visual hierarchy
- **Comprehensive logging** with timestamps
- **Error handling** with user-friendly messages
- **Progress visualization** with progress bars

## Usage Workflow

### 1. **Initial Setup**
1. Launch the application (requires Administrator privileges)
2. Wait for HTTP server initialization
3. Verify all modules show in status cards
4. Place .hex firmware files in the firmware directory

### 2. **Check Module Status**
1. Click "Refresh Status" or wait for automatic updates
2. Verify all modules show "OPERATIONAL" status
3. Check firmware versions for consistency

### 3. **Prepare Firmware Update**
1. Click "Open Firmware Folder" to add .hex files
2. Click "Refresh Firmware List" to detect new files
3. Select desired firmware from dropdown
4. Review file information in the log

### 4. **Perform Update**
1. Click "Update All Modules" for sequential update
2. Review confirmation dialog with update details
3. Click "Yes" to proceed with update
4. Monitor progress bar and log messages
5. Wait for completion confirmation

### 5. **Verify Update Success**
1. Check progress bar shows 100% completion
2. Review log for success messages
3. Verify all modules show updated version
4. Confirm all modules return to "OPERATIONAL" status

## Safety Features

### Pre-Update Validation
- **Firmware file verification**: Ensures .hex files exist and are readable
- **SHA256 hash calculation**: Cryptographic integrity checking
- **Module connectivity check**: Verifies all modules are reachable
- **Confirmation dialog**: Prevents accidental updates

### During Update
- **Sequential processing**: One module at a time to prevent system instability
- **Progress monitoring**: Real-time status updates
- **Timeout protection**: Prevents hung operations
- **Error detection**: Immediate abort on failures

### Post-Update Verification
- **Version synchronization check**: Ensures all modules match
- **Operational status verification**: Confirms modules are functional
- **Automatic status refresh**: Updates display after completion

## Error Handling

### Common Error Scenarios
- **HTTP server startup failure**: Administrator privileges required
- **Module communication timeout**: Check network connectivity
- **Firmware file not found**: Verify file placement in firmware directory
- **Update failure**: Automatic rollback and error reporting
- **SHA256 mismatch**: Firmware integrity verification failure

### Error Recovery
- **Detailed error messages**: Clear explanation of issues
- **Automatic cleanup**: System returns to ready state
- **Rollback capability**: Previous firmware restoration
- **Retry options**: Manual retry after error resolution

## Technical Specifications

### System Requirements
- **Windows 10/11** or Windows Server 2019+
- **.NET 9.0** runtime
- **Administrator privileges** for HTTP server
- **Local network access** to ABLS modules

### Network Configuration
- **HTTP Server**: Port 8080 (configurable)
- **UDP Commands**: Port 12346
- **UDP Status**: Port 12347
- **Module IPs**: 192.168.1.101-103

### Performance
- **Startup time**: < 5 seconds
- **Status refresh**: 5-second intervals
- **Update timeout**: 30 seconds per module
- **Memory usage**: < 50MB typical

## Future Enhancements

### Planned Features
- **Individual module updates**: Selective module updating
- **Update scheduling**: Automated update timing
- **Backup management**: Firmware backup and restore
- **Remote monitoring**: Network-based status monitoring
- **Authentication**: User access control

### Integration Roadmap
- **ABLS.Core integration**: Embedded in main application
- **Database logging**: Persistent update history
- **Fleet management**: Multi-machine support
- **Advanced diagnostics**: Enhanced troubleshooting tools

## Support

For technical support or feature requests, contact RgF Engineering.

---

**Version**: 1.0.0  
**Last Updated**: July 2025  
**Author**: RgF Engineering
