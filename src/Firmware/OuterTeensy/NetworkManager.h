#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>
#include "DataPackets.h"

class NetworkManager {
public:
    NetworkManager(IPAddress remoteIp, unsigned int remotePort);
    void init(byte mac[], unsigned int localPort);
    void sendPacket(const OuterSensorDataPacket& packet);

private:
    EthernetUDP _udp;
    IPAddress _remoteIp;
    unsigned int _remotePort;
};

#endif // NETWORK_MANAGER_H
