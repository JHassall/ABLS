#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>
#include "DataPackets.h"

class NetworkManager {
public:
    NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort);
    void init(byte mac[]);
    void sendPacket(const SensorDataPacket& packet);
    bool receiveCommand(ControlCommandPacket& command);

private:
    EthernetUDP _udp;
    IPAddress _remoteIp;
    unsigned int _remotePort;
    unsigned int _localPort;

    // Buffer for incoming packets
    char _packetBuffer[512];
};

#endif // NETWORK_MANAGER_H
