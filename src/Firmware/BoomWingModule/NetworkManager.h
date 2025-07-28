#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include "DataPackets.h"
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>

class NetworkManager {
public:
    NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort);
    void begin(byte mac[]);
    void sendSensorData(const SensorDataPacket& packet);


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

    // Buffer for incoming packets
    char _packetBuffer[512];
};

#endif // NETWORK_MANAGER_H
