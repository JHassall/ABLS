#include "NetworkManager.h"

NetworkManager::NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort) :
    _remoteIp(remoteIp),
    _remotePort(remotePort),
    _localPort(localPort)
{
}

void NetworkManager::begin(byte mac[]) {
    Ethernet.begin(mac);
    _commandUdp.begin(HYDRAULIC_COMMAND_PORT);
}

void NetworkManager::beginRtcmListener() {
    _rtcmUdp.begin(RTCM_CORRECTION_PORT);
}

void NetworkManager::sendSensorData(const SensorDataPacket& packet) {
    JsonDocument doc;
    doc["PacketType"] = packet.PacketType;
    doc["SenderId"] = packet.SenderId;
    doc["Latitude"] = packet.Latitude;
    doc["Longitude"] = packet.Longitude;
    doc["Altitude"] = packet.Altitude;
    doc["GpsHeading"] = packet.GpsHeading;
    doc["GpsSpeed"] = packet.GpsSpeed;
    doc["Satellites"] = packet.Satellites;
    doc["Roll"] = packet.Roll;
    doc["Pitch"] = packet.Pitch;
    doc["Yaw"] = packet.Yaw;
    doc["RadarDistance"] = packet.RadarDistance;


    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    _commandUdp.beginPacket(_remoteIp, SENSOR_DATA_PORT);
    _commandUdp.write(jsonBuffer);
    _commandUdp.endPacket();
}

int NetworkManager::readRtcmData(uint8_t* buffer, size_t maxSize) {
    int packetSize = _rtcmUdp.parsePacket();
    if (packetSize) {
        return _rtcmUdp.read(buffer, maxSize);
    }
    return 0;
}

