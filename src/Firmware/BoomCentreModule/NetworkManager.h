#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include "DataPackets.h"
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>

// Forward declaration
class HydraulicController;

class NetworkManager {
public:
    NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort);
    void begin(byte mac[]);
    void sendSensorData(const SensorDataPacket& packet);
    int readCommandPacket(ControlCommandPacket* packet);
    
    // Hydraulic controller integration
    void setHydraulicController(HydraulicController* controller);
    void update(); // Process incoming command packets and update hydraulic controller

    // New functions for RTCM data
    void beginRtcmListener();
    void broadcastRtcmData(const uint8_t* data, size_t len);
    int readRtcmData(uint8_t* buffer, size_t maxSize);

private:
    EthernetUDP _udp;
    EthernetUDP _sensorUdp;
    EthernetUDP _commandUdp;
    EthernetUDP _rtcmUdp;     // UDP socket for RTCM data
    IPAddress _broadcastIp;
    IPAddress _remoteIp;
    unsigned int _remotePort;
    unsigned int _localPort;
    
    // Hydraulic controller reference
    HydraulicController* _hydraulicController;

    // Buffer for incoming packets
    char _packetBuffer[512];
};

#endif // NETWORK_MANAGER_H
