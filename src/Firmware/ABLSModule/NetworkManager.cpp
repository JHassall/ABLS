/*
 * ABLS: Automatic Boom Levelling System
 * Unified Network Manager Implementation
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#include "NetworkManager.h"
#include "DiagnosticManager.h"
#include "HydraulicController.h"
#include "SensorManager.h"
#include "VersionManager.h"
#include "UpdateSafetyManager.h"
#include "RgFModuleUpdater.h"

NetworkManager::NetworkManager() :
    _initialized(false),
    _ethernetInitialized(false),
    _moduleRole(MODULE_UNKNOWN),
    _enableRtcmBroadcast(false),
    _enableRtcmReceive(false),
    _enableCommandReceive(false),
    _hydraulicController(nullptr),
    _sensorManager(nullptr),
    _localIP(0, 0, 0, 0),
    _packetsSent(0),
    _packetsReceived(0),
    _rtcmBytesSent(0),
    _rtcmBytesReceived(0),
    _lastStatsUpdate(0),
    _lastSensorDataSent(0),
    _lastCommandCheck(0),
    _lastRtcmCheck(0)
{
    // Initialize MAC address to zeros - will be configured in initialize()
    memset(_macAddress, 0, sizeof(_macAddress));
}

bool NetworkManager::initialize() {
    DiagnosticManager::logMessage(LOG_INFO, "NetworkManager", "Initializing network...");
    
    // Get module role for conditional initialization
    _moduleRole = ModuleConfig::getRole();
    
    // Configure features based on module role
    switch (_moduleRole) {
        case MODULE_CENTRE:
            _enableRtcmBroadcast = true;
            _enableRtcmReceive = false;
            _enableCommandReceive = true;
            DiagnosticManager::logMessage(LOG_INFO, "NetworkManager", "Centre module: RTCM broadcast, command receive enabled");
            break;
            
        case MODULE_LEFT:
        case MODULE_RIGHT:
            _enableRtcmBroadcast = false;
            _enableRtcmReceive = true;
            _enableCommandReceive = false;
            DiagnosticManager::logMessage(LOG_INFO, "NetworkManager", "Wing module: RTCM receive enabled");
            break;
            
        default:
            DiagnosticManager::logError("NetworkManager", "Unknown module role - using minimal configuration");
            _enableRtcmBroadcast = false;
            _enableRtcmReceive = false;
            _enableCommandReceive = false;
            break;
    }
    
    // Initialize Ethernet
    _ethernetInitialized = initializeEthernet();
    if (!_ethernetInitialized) {
        DiagnosticManager::logError("NetworkManager", "Ethernet initialization failed");
        return false;
    }
    
    // Start UDP sockets
    if (!startUDPSockets()) {
        DiagnosticManager::logError("NetworkManager", "UDP socket initialization failed");
        return false;
    }
    
    _initialized = true;
    
    String statusMsg = "Network initialized - IP: " + String(_localIP);
    DiagnosticManager::logMessage(LOG_INFO, "NetworkManager", statusMsg);
    DiagnosticManager::setNetworkStatus("Connected", String(_localIP));
    
    return true;
}

bool NetworkManager::initializeEthernet() {
    logNetworkEvent("Starting Ethernet initialization");
    
    // Configure MAC address based on module role
    configureMACAddress();
    
    // Start Ethernet with DHCP
    if (!Ethernet.begin(_macAddress)) {
        logNetworkEvent("DHCP failed, trying static IP", LOG_WARNING);
        
        // Fallback to static IP configuration
        configureIPAddress();
        IPAddress subnet(255, 255, 255, 0);
        IPAddress gateway(192, 168, 1, 1);
        Ethernet.begin(_macAddress, _localIP, subnet, gateway);
    }
    
    // Wait for link to come up
    uint32_t linkTimeout = millis() + 10000; // 10 second timeout
    while (!Ethernet.linkStatus() && millis() < linkTimeout) {
        delay(100);
    }
    
    if (!Ethernet.linkStatus()) {
        logNetworkEvent("Ethernet link failed", LOG_ERROR);
        return false;
    }
    
    _localIP = Ethernet.localIP();
    logNetworkEvent("Ethernet link established - IP: " + String(_localIP));
    
    return true;
}

void NetworkManager::configureMACAddress() {
    // Generate unique MAC address based on module role and Teensy serial number
    _macAddress[0] = 0x02; // Locally administered MAC
    _macAddress[1] = 0xAB; // "AB" for ABLS
    _macAddress[2] = 0x4C; // "L" for Levelling (0x4C = 'L')
    _macAddress[3] = 0x53; // "S" for System (0x53 = 'S')
    _macAddress[4] = 0x00;
    _macAddress[5] = (uint8_t)_moduleRole; // Last byte indicates module role
    
    // Add some randomness based on Teensy unique ID
    uint32_t uid = HW_OCOTP_CFG1; // Teensy 4.1 unique ID register
    _macAddress[3] = (uid >> 16) & 0xFF;
    _macAddress[4] = (uid >> 8) & 0xFF;
    
    String macStr = "";
    for (int i = 0; i < 6; i++) {
        if (i > 0) macStr += ":";
        macStr += String(_macAddress[i], HEX);
    }
    
    DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", "MAC Address: " + macStr);
}

void NetworkManager::configureIPAddress() {
    // Static IP configuration based on module role
    switch (_moduleRole) {
        case MODULE_LEFT:
            _localIP = IPAddress(192, 168, 1, 101);
            break;
        case MODULE_CENTRE:
            _localIP = IPAddress(192, 168, 1, 102);
            break;
        case MODULE_RIGHT:
            _localIP = IPAddress(192, 168, 1, 103);
            break;
        default:
            _localIP = IPAddress(192, 168, 1, 199); // Unknown role fallback
            break;
    }
    
    DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", "Static IP configured: " + String(_localIP));
}

bool NetworkManager::startUDPSockets() {
    logNetworkEvent("Starting UDP sockets");
    
    // Start sensor data UDP (all modules)
    if (!_sensorUdp.begin(SENSOR_DATA_PORT)) {
        logNetworkEvent("Failed to start sensor UDP on port " + String(SENSOR_DATA_PORT), LOG_ERROR);
        return false;
    }
    
    // Start command UDP (centre module only)
    if (_enableCommandReceive) {
        if (!_commandUdp.begin(COMMAND_PORT)) {
            logNetworkEvent("Failed to start command UDP on port " + String(COMMAND_PORT), LOG_ERROR);
            return false;
        }
        logNetworkEvent("Command UDP started on port " + String(COMMAND_PORT));
    }
    
    // Start RTCM UDP (all modules, different usage)
    if (!_rtcmUdp.begin(RTCM_PORT)) {
        logNetworkEvent("Failed to start RTCM UDP on port " + String(RTCM_PORT), LOG_ERROR);
        return false;
    }
    
    // Start RgFModuleUpdate command UDP (all modules)
    if (!_updateCommandUdp.begin(OTA_COMMAND_PORT)) {
        logNetworkEvent("Failed to start RgFModuleUpdate command UDP on port " + String(OTA_COMMAND_PORT), LOG_ERROR);
        return false;
    }
    logNetworkEvent("RgFModuleUpdate command UDP started on port " + String(OTA_COMMAND_PORT));
    
    // Start RgFModuleUpdate status UDP (all modules)
    if (!_updateStatusUdp.begin(OTA_RESPONSE_PORT)) {
        logNetworkEvent("Failed to start RgFModuleUpdate status UDP on port " + String(OTA_RESPONSE_PORT), LOG_ERROR);
        return false;
    }
    logNetworkEvent("RgFModuleUpdate status UDP started on port " + String(OTA_RESPONSE_PORT));
    
    logNetworkEvent("All UDP sockets started successfully");
    return true;
}

void NetworkManager::update() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    
    // Process incoming commands (centre module only)
    if (_enableCommandReceive && (now - _lastCommandCheck >= 10)) {
        processIncomingCommands();
        _lastCommandCheck = now;
    }
    
    // Process incoming RTCM data (wing modules only)
    if (_enableRtcmReceive && (now - _lastRtcmCheck >= 50)) {
        processIncomingRtcm();
        _lastRtcmCheck = now;
    }
    
    // Process RgFModuleUpdate commands (all modules)
    processRgFModuleUpdateCommands();
    
    // Update statistics every second
    if (now - _lastStatsUpdate >= 1000) {
        updateStatistics();
        _lastStatsUpdate = now;
    }
}

void NetworkManager::sendSensorData(const SensorDataPacket& packet) {
    if (!_initialized) return;
    
    // Send sensor data to Toughbook
    _sensorUdp.beginPacket(TOUGHBOOK_IP, SENSOR_DATA_PORT);
    _sensorUdp.write((const uint8_t*)&packet, sizeof(SensorDataPacket));
    
    if (_sensorUdp.endPacket()) {
        _packetsSent++;
        _lastSensorDataSent = millis();
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "Sensor data sent to Toughbook (" + String(sizeof(SensorDataPacket)) + " bytes)");
    } else {
        DiagnosticManager::logError("NetworkManager", "Failed to send sensor data to Toughbook");
    }
}

int NetworkManager::readCommandPacket(ControlCommandPacket* packet) {
    if (!_initialized || !_enableCommandReceive || !packet) return 0;
    
    int packetSize = _commandUdp.parsePacket();
    if (packetSize > 0) {
        if (packetSize == sizeof(ControlCommandPacket)) {
            // CRITICAL FIX: Validate actual bytes read to prevent partial/corrupt packets
            int bytesRead = _commandUdp.read((uint8_t*)packet, sizeof(ControlCommandPacket));
            
            if (bytesRead != sizeof(ControlCommandPacket)) {
                DiagnosticManager::logError("NetworkManager", 
                    "Incomplete command packet received: " + String(bytesRead) + "/" + String(sizeof(ControlCommandPacket)) + " bytes");
                return -1; // Error indicator for incomplete packet
            }
            
            _packetsReceived++;
            
            DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
                "Command packet received from Toughbook (" + String(bytesRead) + " bytes)");
            
            return bytesRead;
        } else {
            // ENHANCED LOGGING: Log wrong-size packets for debugging/security monitoring
            DiagnosticManager::logError("NetworkManager", 
                "Invalid command packet size received: " + String(packetSize) + " bytes (expected " + String(sizeof(ControlCommandPacket)) + " bytes)");
            
            // Flush the invalid packet to prevent buffer issues
            _commandUdp.flush();
            return -2; // Error indicator for wrong-size packet
        }
    }
    
    return 0;
}

void NetworkManager::broadcastRtcmData(const uint8_t* data, size_t len) {
    if (!_initialized || !_enableRtcmBroadcast || !data || len == 0) return;
    
    // Broadcast RTCM data to all wing modules
    _rtcmUdp.beginPacket(RTCM_BROADCAST_IP, RTCM_PORT);
    _rtcmUdp.write(data, len);
    
    if (_rtcmUdp.endPacket()) {
        _rtcmBytesSent += len;
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "RTCM data broadcasted (" + String(len) + " bytes)");
    } else {
        DiagnosticManager::logError("NetworkManager", "Failed to broadcast RTCM data");
    }
}

int NetworkManager::readRtcmData(uint8_t* buffer, size_t maxSize) {
    if (!_initialized || !_enableRtcmReceive || !buffer) return 0;
    
    int packetSize = _rtcmUdp.parsePacket();
    if (packetSize > 0 && packetSize <= (int)maxSize) {
        int bytesRead = _rtcmUdp.read(buffer, packetSize);
        
        // ENHANCED RTCM VALIDATION: Validate actual bytes read
        if (bytesRead != packetSize) {
            DiagnosticManager::logError("NetworkManager", 
                "Incomplete RTCM packet received: " + String(bytesRead) + "/" + String(packetSize) + " bytes");
            return -1; // Error indicator
        }
        
        // ENHANCED RTCM VALIDATION: Basic RTCM message format validation
        if (!validateRtcmData(buffer, bytesRead)) {
            DiagnosticManager::logError("NetworkManager", 
                "Invalid RTCM data format detected - discarding " + String(bytesRead) + " bytes");
            return -2; // Invalid format indicator
        }
        
        _rtcmBytesReceived += bytesRead;
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "RTCM data received and validated (" + String(bytesRead) + " bytes)");
        
        return bytesRead;
    } else if (packetSize > (int)maxSize) {
        // ENHANCED RTCM VALIDATION: Log oversized packets
        DiagnosticManager::logError("NetworkManager", 
            "RTCM packet too large: " + String(packetSize) + " bytes (max " + String(maxSize) + " bytes)");
        _rtcmUdp.flush(); // Discard oversized packet
        return -3; // Oversized packet indicator
    }
    
    return 0;
}

void NetworkManager::processIncomingCommands() {
    ControlCommandPacket commandPacket;
    
    if (readCommandPacket(&commandPacket) > 0) {
        // Forward command to hydraulic controller if available
        if (_hydraulicController) {
            _hydraulicController->processCommand(commandPacket);
        }
        
        DiagnosticManager::logMessage(LOG_INFO, "NetworkManager", 
            "Command processed - Centre: " + String(commandPacket.SetpointCenter) + 
            ", Left: " + String(commandPacket.SetpointLeft) + 
            ", Right: " + String(commandPacket.SetpointRight));
    }
}

void NetworkManager::processIncomingRtcm() {
    uint8_t rtcmBuffer[1024]; // Buffer for RTCM data
    
    int bytesReceived = readRtcmData(rtcmBuffer, sizeof(rtcmBuffer));
    if (bytesReceived > 0) {
        // Forward RTCM data to sensor manager for GPS injection
        if (_sensorManager) {
            _sensorManager->forwardRtcmToGps(rtcmBuffer, bytesReceived);
        }
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "RTCM data forwarded to GPS (" + String(bytesReceived) + " bytes)");
    }
}

void NetworkManager::updateStatistics() {
    // Update diagnostic display with current network status
    String status = getNetworkStatusString();
    DiagnosticManager::setNetworkStatus(status, String(_localIP));
}

void NetworkManager::setHydraulicController(HydraulicController* controller) {
    _hydraulicController = controller;
    DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", "Hydraulic controller reference set");
}

void NetworkManager::setSensorManager(SensorManager* sensorManager) {
    _sensorManager = sensorManager;
    DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", "Sensor manager reference set");
}

String NetworkManager::getNetworkStatusString() {
    if (!_initialized) return "Not initialized";
    
    String status = "Connected";
    
    if (_enableRtcmBroadcast) {
        status += " (RTCM TX)";
    } else if (_enableRtcmReceive) {
        status += " (RTCM RX)";
    }
    
    return status;
}

void NetworkManager::processRgFModuleUpdateCommands() {
    // Check for incoming RgFModuleUpdate commands
    RgFModuleUpdateCommandPacket command;
    int result = readRgFModuleUpdateCommand(&command);
    
    if (result > 0) {
        // CRITICAL FIX: Ensure all string fields are null-terminated to prevent buffer overflow
        command.Command[sizeof(command.Command) - 1] = '\0';
        command.FirmwareUrl[sizeof(command.FirmwareUrl) - 1] = '\0';
        command.FirmwareHash[sizeof(command.FirmwareHash) - 1] = '\0';
        
        // Command received successfully - process it
        if (strcmp(command.Command, "STATUS_QUERY") == 0) {
            // Send current module status
            sendModuleStatusResponse();
        }
        else if (strcmp(command.Command, "START_UPDATE") == 0) {
            // Initiate firmware update
            handleStartUpdateCommand(&command);
        }
        else if (strcmp(command.Command, "ABORT_UPDATE") == 0) {
            // Abort any ongoing update
            handleAbortUpdateCommand();
        }
        else {
            logNetworkEvent("RgFModuleUpdate: Unknown command: " + String(command.Command), LOG_WARNING);
        }
    }
}

void NetworkManager::sendModuleStatusResponse() {
    RgFModuleUpdateStatusPacket status;
    
    // Set module identification
    status.SenderId = (uint8_t)_moduleRole;
    status.Timestamp = millis();
    
    // Get current firmware version from VersionManager
    FirmwareVersion_t currentVersion = VersionManager::getCurrentVersion();
    snprintf(status.Version, sizeof(status.Version), "%d.%d.%d", 
             currentVersion.major, currentVersion.minor, currentVersion.patch);
    
    // Set operational status
    UpdateStatus_t updateStatus = VersionManager::getUpdateStatus();
    switch (updateStatus) {
        case UPDATE_IDLE:
            strcpy(status.Status, "OPERATIONAL");
            break;
        case UPDATE_DOWNLOADING:
            strcpy(status.Status, "UPDATING");
            strcpy(status.UpdateStage, "Downloading firmware");
            status.UpdateProgress = VersionManager::getUpdateProgress();
            break;
        case UPDATE_VERIFYING:
            strcpy(status.Status, "UPDATING");
            strcpy(status.UpdateStage, "Verifying firmware");
            status.UpdateProgress = VersionManager::getUpdateProgress();
            break;
        case UPDATE_FLASHING:
            strcpy(status.Status, "UPDATING");
            strcpy(status.UpdateStage, "Flashing firmware");
            status.UpdateProgress = VersionManager::getUpdateProgress();
            break;
        case UPDATE_SUCCESS:
            strcpy(status.Status, "OPERATIONAL");
            strcpy(status.UpdateStage, "Update completed");
            status.UpdateProgress = 100;
            break;
        case UPDATE_REBOOTING:
            strcpy(status.Status, "UPDATING");
            strcpy(status.UpdateStage, "Rebooting");
            status.UpdateProgress = 95;
            break;
        case UPDATE_ROLLBACK:
            strcpy(status.Status, "UPDATING");
            strcpy(status.UpdateStage, "Rolling back");
            status.UpdateProgress = 50;
            break;
        case UPDATE_FAILED:
            strcpy(status.Status, "ERROR");
            strcpy(status.UpdateStage, "Update failed");
            // Note: getLastError method doesn't exist, using generic error message
            strcpy(status.LastError, "Firmware update failed");
            break;
    }
    
    // Set system information
    status.UptimeSeconds = millis() / 1000;
    status.FreeMemory = getFreeMemory();
    status.PacketsSent = _packetsSent;
    status.PacketsReceived = _packetsReceived;
    
    // Send the status response
    sendRgFModuleUpdateStatus(status);
}

void NetworkManager::handleStartUpdateCommand(const RgFModuleUpdateCommandPacket* command) {
    logNetworkEvent("RgFModuleUpdate: START_UPDATE command received", LOG_INFO);
    
    // Validate the update command
    if (strlen(command->FirmwareUrl) == 0) {
        logNetworkEvent("RgFModuleUpdate: Invalid firmware URL", LOG_ERROR);
        return;
    }
    
    if (strlen(command->FirmwareHash) == 0) {
        logNetworkEvent("RgFModuleUpdate: Missing firmware hash", LOG_ERROR);
        return;
    }
    
    if (command->FirmwareSize == 0) {
        logNetworkEvent("RgFModuleUpdate: Invalid firmware size", LOG_ERROR);
        return;
    }
    
    // CRITICAL FIX: Check if update is already in progress to prevent concurrent updates
    UpdateStatus_t currentStatus = VersionManager::getUpdateStatus();
    if (currentStatus != UPDATE_IDLE && currentStatus != UPDATE_SUCCESS && currentStatus != UPDATE_FAILED) {
        logNetworkEvent("RgFModuleUpdate: Update already in progress (status: " + String((int)currentStatus) + ") - rejecting concurrent update request", LOG_WARNING);
        return;
    }
    
    // Check if system is safe for update
    SafetyCheckResult_t safetyResult = UpdateSafetyManager::isSafeToUpdate();
    if (safetyResult != SAFETY_OK) {
        logNetworkEvent("RgFModuleUpdate: System not safe for update - " + String(safetyResultToString(safetyResult)), LOG_WARNING);
        return;
    }
    
    // Start the firmware update process
    String firmwareUrl = String(command->FirmwareUrl);
    String firmwareHash = String(command->FirmwareHash);
    
    // Use RgFModuleUpdater to perform the update
    bool updateStarted = RgFModuleUpdater::performUpdate(firmwareUrl);
    
    if (updateStarted) {
        logNetworkEvent("RgFModuleUpdate: Firmware update started", LOG_INFO);
    } else {
        logNetworkEvent("RgFModuleUpdate: Failed to start firmware update", LOG_ERROR);
    }
}

void NetworkManager::handleAbortUpdateCommand() {
    logNetworkEvent("RgFModuleUpdate: ABORT_UPDATE command received", LOG_INFO);
    
    // TODO: Implement update abort functionality in RgFModuleUpdater
    // For now, just log the command
    logNetworkEvent("RgFModuleUpdate: Update abort not yet implemented", LOG_WARNING);
}

uint32_t NetworkManager::getFreeMemory() {
    // Simple free memory estimation for Teensy 4.1
    char* ramend = (char*)0x20280000;  // 512KB RAM on Teensy 4.1
    char* heapend = (char*)sbrk(0);
    return ramend - heapend;
}

void NetworkManager::logNetworkEvent(const String& event, LogLevel_t level) {
    DiagnosticManager::logMessage(level, "NetworkManager", event);
}

// =================================================================
// RgFModuleUpdate Network Protocol Implementation
// =================================================================

int NetworkManager::readRgFModuleUpdateCommand(RgFModuleUpdateCommandPacket* packet) {
    if (!_initialized || !packet) {
        return 0;
    }
    
    int packetSize = _updateCommandUdp.parsePacket();
    if (packetSize == 0) {
        return 0; // No packet available
    }
    
    if (packetSize != sizeof(RgFModuleUpdateCommandPacket)) {
        logNetworkEvent("RgFModuleUpdate: Invalid command packet size: " + String(packetSize), LOG_WARNING);
        _updateCommandUdp.flush(); // Discard invalid packet
        return -1;
    }
    
    // Read the packet data
    int bytesRead = _updateCommandUdp.read((uint8_t*)packet, sizeof(RgFModuleUpdateCommandPacket));
    
    if (bytesRead == sizeof(RgFModuleUpdateCommandPacket)) {
        _packetsReceived++;
        
        // CRITICAL FIX: Ensure strings are null-terminated before logging
        packet->Command[sizeof(packet->Command) - 1] = '\0';
        packet->FirmwareUrl[sizeof(packet->FirmwareUrl) - 1] = '\0';
        packet->FirmwareHash[sizeof(packet->FirmwareHash) - 1] = '\0';
        
        // Log the received command
        String logMsg = "RgFModuleUpdate command received: " + String(packet->Command);
        if (strcmp(packet->Command, "START_UPDATE") == 0) {
            logMsg += " (URL: " + String(packet->FirmwareUrl) + ", Size: " + String(packet->FirmwareSize) + ")";
        }
        logNetworkEvent(logMsg, LOG_INFO);
        
        return bytesRead;
    }
    
    logNetworkEvent("RgFModuleUpdate: Failed to read command packet", LOG_ERROR);
    return -1;
}

void NetworkManager::sendRgFModuleUpdateStatus(const RgFModuleUpdateStatusPacket& packet) {
    if (!_initialized) {
        logNetworkEvent("RgFModuleUpdate: Cannot send status - network not initialized", LOG_ERROR);
        return;
    }
    
    // Send status packet to Toughbook
    _updateStatusUdp.beginPacket(TOUGHBOOK_IP, OTA_RESPONSE_PORT);
    _updateStatusUdp.write((const uint8_t*)&packet, sizeof(RgFModuleUpdateStatusPacket));
    
    if (_updateStatusUdp.endPacket()) {
        _packetsSent++;
        
        // Log successful transmission
        String logMsg = "RgFModuleUpdate status sent: " + String(packet.Status);
        if (strcmp(packet.Status, "UPDATING") == 0) {
            logMsg += " (" + String(packet.UpdateProgress) + "% - " + String(packet.UpdateStage) + ")";
        }
        logNetworkEvent(logMsg, LOG_DEBUG);
    } else {
        logNetworkEvent("RgFModuleUpdate: Failed to send status packet", LOG_ERROR);
    }
}

// =================================================================
// RTCM Data Validation Implementation
// =================================================================

bool NetworkManager::validateRtcmData(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    
    // RTCM3 messages start with preamble 0xD3
    if (data[0] != 0xD3) {
        DiagnosticManager::logError("NetworkManager", "Invalid RTCM preamble: 0x" + String(data[0], HEX));
        return false;
    }
    
    // Basic length validation - RTCM messages are typically 6-1023 bytes
    if (len < 6 || len > 1023) {
        DiagnosticManager::logError("NetworkManager", "Invalid RTCM message length: " + String(len) + " bytes");
        return false;
    }
    
    // Extract message length from header (bits 14-23 of first 3 bytes)
    if (len >= 3) {
        uint16_t messageLength = ((data[1] & 0x03) << 8) | data[2];
        uint16_t expectedTotalLength = messageLength + 6; // Message + header + CRC
        
        if (len != expectedTotalLength) {
            DiagnosticManager::logError("NetworkManager", 
                "RTCM length mismatch: received " + String(len) + " bytes, expected " + String(expectedTotalLength) + " bytes");
            return false;
        }
    }
    
    // Basic CRC validation would go here, but requires full CRC24Q implementation
    // For now, we'll rely on the above basic format checks
    
    return true;
}
