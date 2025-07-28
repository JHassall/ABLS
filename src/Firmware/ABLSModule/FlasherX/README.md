# RgFModuleUpdater - FlasherX Integration

## Attribution

**RgFModuleUpdater** is based on the excellent **FlasherX** project by **Joe Pasquariello**.

- **Original Author:** Joe Pasquariello
- **Original Repository:** https://github.com/joepasquariello/FlasherX
- **License:** Open source
- **Purpose:** Over-the-air firmware updates for Teensy microcontrollers

**FlasherX** has been stable and field-tested for 3+ years, providing reliable OTA firmware updates for Teensy LC/3.x/4.x/MicroMod devices. We are grateful to Joe Pasquariello for creating this excellent foundation.

## RgF ABLS Integration

**RgFModuleUpdater** adapts FlasherX for use in the RgF ABLS (Automatic Boom Level System) project with the following enhancements:

### Key Adaptations

1. **Professional API Design**
   - Clean C++ class interface
   - Progress reporting callbacks
   - Comprehensive error handling
   - Status management

2. **Network Integration**
   - HTTP firmware download capability
   - Binary firmware handling (no Intel HEX dependency)
   - Integration with ABLS UDP communication protocol

3. **Agricultural Equipment Safety**
   - Enhanced safety checks for field operation
   - System stationary validation
   - Network stability verification
   - Power level monitoring

4. **Field Reliability**
   - Automatic firmware backup
   - Rollback capability on update failure
   - Comprehensive logging and diagnostics
   - Recovery mechanisms

## File Structure

```
FlasherX/
├── README.md                   # This file - attribution and integration notes
├── FlashTxx.h                  # Original FlasherX flash primitives header
├── FlashTxx.c                  # Original FlasherX flash implementation
├── RgFModuleUpdater.h          # RgF-specific API wrapper
└── RgFModuleUpdater.cpp        # RgF FlasherX integration implementation
```

## Hardware Support

**Optimized for Teensy 4.1:**
- ✅ **8MB Flash** (0x800000 bytes)
- ✅ **4KB Sectors** (0x1000 bytes)
- ✅ **4-byte writes** (32-bit aligned)
- ✅ **Base Address:** 0x60000000 (external QSPI flash)
- ✅ **Dual-bank capability** via flash buffering

## API Overview

```cpp
// Initialize the update system
RgFModuleUpdater::initialize();
RgFModuleUpdater::setDiagnosticManager(&diagnostics);

// Perform complete firmware update
bool success = RgFModuleUpdater::performUpdateFromBuffer(firmwareData, firmwareSize);

// Check status and progress
UpdateStatus status = RgFModuleUpdater::getStatus();
uint8_t progress = RgFModuleUpdater::getProgress();

// Reboot to new firmware
if (success) {
    RgFModuleUpdater::reboot();
}
```

## Integration with ABLS OTA System

**RgFModuleUpdater** integrates seamlessly with the existing ABLS OTA infrastructure:

1. **OTAUpdateManager** calls RgFModuleUpdater for actual firmware flashing
2. **Progress reporting** via UDP to Toughbook
3. **Status synchronization** across all ABLS modules
4. **Sequential updates** with rollback capability

## Safety Features

- **Pre-update safety checks** ensure system is stationary and stable
- **Firmware validation** verifies integrity and compatibility
- **Automatic backup** of current firmware before update
- **Rollback capability** on update failure or boot issues
- **Comprehensive logging** for diagnostics and troubleshooting

## Development Status

### ✅ Completed Features
- [x] FlasherX source integration with proper attribution
- [x] RgFModuleUpdater API design and implementation
- [x] Flash buffer management
- [x] Firmware validation and flashing
- [x] Progress reporting and status management
- [x] Error handling and diagnostics integration

### 🚧 In Progress
- [ ] HTTP firmware download implementation
- [ ] Firmware backup and rollback functionality
- [ ] Enhanced safety checks for agricultural equipment
- [ ] Integration testing with OTAUpdateManager

### 📋 Planned Features
- [ ] Automatic rollback on boot failure
- [ ] Firmware signature verification
- [ ] Delta updates for bandwidth efficiency
- [ ] Field diagnostic tools

## Testing

**Compilation Testing:**
```bash
# Test compilation with Arduino IDE
# Ensure all required libraries are installed:
# - QNEthernet
# - Adafruit GFX
# - Adafruit SSD1306
```

**Integration Testing:**
- Flash buffer creation and management
- Firmware validation and flashing
- Progress reporting callbacks
- Error handling scenarios

## Credits

- **Joe Pasquariello** - Original FlasherX author and maintainer
- **RgF Development Team** - ABLS integration and enhancements
- **Teensy Community** - Hardware support and testing

## License

This integration maintains the original FlasherX license terms while adding RgF-specific enhancements for the ABLS project.

---

**Thank you to Joe Pasquariello for creating FlasherX and enabling reliable OTA updates for Teensy microcontrollers!**
