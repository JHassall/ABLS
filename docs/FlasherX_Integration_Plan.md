# RgFModuleUpdater Integration Plan for ABLS OTA System

## Attribution

**FlasherX** is an excellent open-source project by **Joe Pasquariello** that enables over-the-air firmware updates for Teensy microcontrollers.

- **Original Author:** Joe Pasquariello
- **Repository:** https://github.com/joepasquariello/FlasherX
- **License:** Open source
- **Purpose:** OTA firmware updates for Teensy LC/3.x/4.x/MicroMod

**IMPORTANT:** All FlasherX code integrated into ABLS will maintain proper attribution to Joe Pasquariello and reference the original repository.

## FlasherX Architecture Analysis

### Core Components

1. **FlashTxx.h/c** - Low-level flash primitives
   - Platform-specific flash operations
   - Teensy 4.1: 8MB flash, 4KB sectors, 4-byte writes
   - Base address: 0x60000000 (external QSPI flash)

2. **FXUtil.h/cpp** - Intel HEX file processing
   - Parses Intel HEX format firmware files
   - Handles streaming input validation
   - Firmware integrity checking

3. **FlasherX.ino** - Main application framework
   - Update workflow orchestration
   - Flash buffer management
   - User interface

### Teensy 4.1 Flash Layout

```
|<------------------------------ 8MB FLASH ------------------------------>|
^0x60000000
|<------- Current Firmware ------->|<-- Buffer for New -->|<-- Reserved -->|
                                   ^Dynamic Buffer        ^Top 16KB
```

### Update Process Flow

1. **Create flash buffer** in unused space above current firmware
2. **Download firmware** to buffer (Intel HEX or binary)
3. **Validate firmware** integrity and target compatibility
4. **Erase current firmware** from base address
5. **Copy new firmware** from buffer to base address
6. **Erase buffer** (cleanup)
7. **Reboot** to activate new firmware

## Integration Strategy

### Approach: Direct Integration with ABLS Customization

**Why Direct Integration:**
- Full control over update process
- Custom network integration (HTTP vs Intel HEX)
- ABLS-specific validation and safety checks
- Seamless integration with existing OTA infrastructure
- No library dependency issues

### Integration Steps

#### Phase 1: Core FlasherX Integration

1. **Copy FlasherX source files** to ABLS project
   - `FlashTxx.h/c` → `ABLSModule/FlasherX/`
   - `FXUtil.h/cpp` → `ABLSModule/FlasherX/`
   - Maintain original file structure and attribution

2. **Create RgFModuleUpdater wrapper**
   - `RgFModuleUpdater.h/cpp` - RgF-specific API wrapper
   - Integrate with existing `OTAUpdateManager`
   - Network-based firmware download (HTTP)

3. **Adapt for ABLS workflow**
   - Replace Intel HEX with binary firmware download
   - Integrate with UDP command protocol
   - Add ABLS-specific validation (version, target ID)

#### Phase 2: Network Integration

1. **HTTP firmware download**
   - Download firmware from Toughbook web server
   - Progress reporting via UDP
   - Chunked download with resume capability

2. **Binary firmware handling**
   - Direct binary write to flash buffer
   - CRC32 validation
   - Size and compatibility checking

#### Phase 3: Safety and Rollback

1. **Enhanced safety checks**
   - System stationary validation
   - Module health verification
   - Network connectivity confirmation

2. **Rollback capability**
   - Backup current firmware before update
   - Automatic rollback on boot failure
   - Manual rollback command support

### API Design

```cpp
class RgFModuleUpdater {
public:
    // Initialization
    static bool initialize();
    
    // Flash buffer management
    static bool createFlashBuffer(uint32_t* bufferAddr, uint32_t* bufferSize);
    static void freeFlashBuffer(uint32_t bufferAddr, uint32_t bufferSize);
    
    // Firmware operations
    static bool downloadFirmware(const String& url, uint32_t bufferAddr, uint32_t bufferSize);
    static bool validateFirmware(uint32_t bufferAddr, uint32_t size, uint32_t expectedCRC);
    static bool flashFirmware(uint32_t bufferAddr, uint32_t size);
    
    // Backup and rollback
    static bool backupCurrentFirmware();
    static bool rollbackFirmware();
    
    // Progress reporting
    static void setProgressCallback(void (*callback)(uint8_t progress, const String& status));
    
private:
    // FlasherX integration
    static uint32_t _flashBuffer;
    static uint32_t _flashBufferSize;
    static bool _initialized;
};
```

### File Structure

```
ABLSModule/
├── FlasherX/                    # FlasherX integration directory
│   ├── FlashTxx.h              # Original FlasherX flash primitives (Joe Pasquariello)
│   ├── FlashTxx.c              # Original FlasherX flash implementation
│   ├── FXUtil.h                # Original FlasherX utilities header
│   ├── FXUtil.cpp              # Original FlasherX utilities implementation
│   ├── ABLSFlasher.h           # ABLS-specific FlasherX wrapper
│   ├── ABLSFlasher.cpp         # ABLS FlasherX integration
│   └── README.md               # Attribution and integration notes
├── OTAUpdateManager.h          # Updated to use ABLSFlasher
├── OTAUpdateManager.cpp        # Enhanced with actual flashing
└── ... (other ABLS files)
```

### Attribution Requirements

All FlasherX-derived files will include:

```cpp
/*
 * Based on FlasherX by Joe Pasquariello
 * Original repository: https://github.com/joepasquariello/FlasherX
 * 
 * FlasherX provides over-the-air firmware updates for Teensy microcontrollers
 * and has been adapted for use in the ABLS project with permission.
 * 
 * Original FlasherX license and attribution maintained.
 */
```

## Implementation Timeline

### Phase 1: Core Integration (Week 1)
- [ ] Copy FlasherX source files with proper attribution
- [ ] Create ABLSFlasher wrapper class
- [ ] Basic compilation and integration testing
- [ ] Update OTAUpdateManager to use ABLSFlasher

### Phase 2: Network Enhancement (Week 2)
- [ ] Implement HTTP firmware download
- [ ] Add binary firmware handling
- [ ] Progress reporting integration
- [ ] End-to-end update testing

### Phase 3: Production Features (Week 3)
- [ ] Enhanced safety checks
- [ ] Rollback capability
- [ ] Error recovery and retry logic
- [ ] Comprehensive testing and validation

## Testing Strategy

1. **Unit Testing**
   - Flash buffer creation/management
   - Firmware validation functions
   - Network download components

2. **Integration Testing**
   - Complete OTA update workflow
   - Multiple module sequential updates
   - Error handling and recovery

3. **Field Testing**
   - Real hardware validation
   - Network reliability testing
   - Rollback scenario validation

## Success Criteria

- ✅ Successful compilation with FlasherX integration
- ✅ Complete OTA firmware update capability
- ✅ Reliable network-based firmware download
- ✅ Automatic rollback on update failure
- ✅ Sequential multi-module updates
- ✅ Comprehensive error handling and recovery
- ✅ Proper attribution to Joe Pasquariello maintained

## Notes

- FlasherX has been stable for 3+ years with proven field use
- Teensy 4.1 support is mature and well-tested
- Direct integration provides maximum flexibility for ABLS requirements
- Original FlasherX architecture is well-suited for our OTA needs
