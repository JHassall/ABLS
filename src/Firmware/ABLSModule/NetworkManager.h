/*
 * ABLS: Automatic Boom Levelling System
 * Unified Network Manager
 * 
 * Handles UDP communication for all module roles:
 * - Centre Module: RTCM broadcasting, hydraulic command receiving, sensor data sending
 * - Wing Modules: RTCM receiving, sensor data sending
 * - All Modules: Toughbook communication, OTA update support
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "DataPackets.h"
#include "ModuleConfig.h"
#include "DiagnosticManager.h"

using namespace qindesign::network;

// Forward declarations
class HydraulicController;
class SensorManager;

// Network configuration
#define TOUGHBOOK_IP        IPAddress(192, 168, 1, 100)
#define SENSOR_DATA_PORT    8001
#define COMMAND_PORT        8002
#define RTCM_PORT          8003
#define RTCM_BROADCAST_IP   IPAddress(192, 168, 1, 255)

class NetworkManager {
public:
    NetworkManager();
    
    // Initialization and lifecycle
    bool initialize();
    void update();
    bool isInitialized() { return _initialized; }
    
    // Sensor data transmission (all modules)
    void sendSensorData(const SensorDataPacket& packet);
    
    // Command reception (centre module only)
    int readCommandPacket(ControlCommandPacket* packet);
    
    // RTCM correction handling
    void broadcastRtcmData(const uint8_t* data, size_t len);  // Centre module only
    int readRtcmData(uint8_t* buffer, size_t maxSize);        // Wing modules only
    
    // Component integration
    void setHydraulicController(HydraulicController* controller);
    void setSensorManager(SensorManager* sensorManager);
    
    // Network status
    IPAddress getLocalIP() { return Ethernet.localIP(); }
    String getNetworkStatusString();
    
    // Statistics
    uint32_t getPacketsSent() { return _packetsSent; }
    uint32_t getPacketsReceived() { return _packetsReceived; }
    uint32_t getRtcmBytesSent() { return _rtcmBytesSent; }
    uint32_t getRtcmBytesReceived() { return _rtcmBytesReceived; }

private:
    // Initialization state
    bool _initialized;
    bool _ethernetInitialized;
    
    // Module role-specific configuration
    ModuleRole_t _moduleRole;
    bool _enableRtcmBroadcast;  // Centre module only
    bool _enableRtcmReceive;    // Wing modules only
    bool _enableCommandReceive; // Centre module only
    
    // Network objects
    EthernetUDP _sensorUdp;     // For sending sensor data to Toughbook
    EthernetUDP _commandUdp;    // For receiving commands from Toughbook (centre only)
    EthernetUDP _rtcmUdp;       // For RTCM correction data
    
    // Component references
    HydraulicController* _hydraulicController;
    SensorManager* _sensorManager;
    
    // Network configuration
    IPAddress _localIP;
    uint8_t _macAddress[6];
    
    // Statistics
    uint32_t _packetsSent;
    uint32_t _packetsReceived;
    uint32_t _rtcmBytesSent;
    uint32_t _rtcmBytesReceived;
    uint32_t _lastStatsUpdate;
    
    // Timing
    uint32_t _lastSensorDataSent;
    uint32_t _lastCommandCheck;
    uint32_t _lastRtcmCheck;
    
    // Internal methods
    bool initializeEthernet();
    void configureMACAddress();
    void configureIPAddress();
    bool startUDPSockets();
    void processIncomingCommands();
    void processIncomingRtcm();
    void updateStatistics();
    void logNetworkEvent(const String& event, LogLevel_t level = LOG_INFO);
};

#endif // NETWORK_MANAGER_H
