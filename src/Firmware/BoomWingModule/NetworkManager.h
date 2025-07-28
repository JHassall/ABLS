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


    // RTCM data functions - Wing modules only receive RTCM from Centre module
    void beginRtcmListener();
    int readRtcmData(uint8_t* buffer, size_t maxSize);

private:
    EthernetUDP _udp;
    EthernetUDP _sensorUdp;
    EthernetUDP _commandUdp;
    EthernetUDP _rtcmUdp;     // UDP socket for RTCM data
    IPAddress _remoteIp;
    unsigned int _remotePort;
    unsigned int _localPort;

    // Buffer for incoming packets
    char _packetBuffer[512];
};

#endif // NETWORK_MANAGER_H
