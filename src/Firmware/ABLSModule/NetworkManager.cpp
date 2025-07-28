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
    if (packetSize > 0 && packetSize == sizeof(ControlCommandPacket)) {
        _commandUdp.read((uint8_t*)packet, sizeof(ControlCommandPacket));
        _packetsReceived++;
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "Command packet received from Toughbook (" + String(packetSize) + " bytes)");
        
        return packetSize;
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
        _rtcmBytesReceived += bytesRead;
        
        DiagnosticManager::logMessage(LOG_DEBUG, "NetworkManager", 
            "RTCM data received (" + String(bytesRead) + " bytes)");
        
        return bytesRead;
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

void NetworkManager::logNetworkEvent(const String& event, LogLevel_t level) {
    DiagnosticManager::logMessage(level, "NetworkManager", event);
}
