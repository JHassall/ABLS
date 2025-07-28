#include "NetworkManager.h"
#include "HydraulicController.h"

NetworkManager::NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort) :
    _broadcastIp(255, 255, 255, 255),
    _remoteIp(remoteIp),
    _remotePort(remotePort),
    _localPort(localPort),
    _hydraulicController(nullptr)
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
    doc["RamPosCenterPercent"] = packet.RamPosCenterPercent;
    doc["RamPosLeftPercent"] = packet.RamPosLeftPercent;
    doc["RamPosRightPercent"] = packet.RamPosRightPercent;

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    _commandUdp.beginPacket(_remoteIp, SENSOR_DATA_PORT);
    _commandUdp.write(jsonBuffer);
    _commandUdp.endPacket();
}

int NetworkManager::readCommandPacket(ControlCommandPacket* packet) {
    int packetSize = _commandUdp.parsePacket();
    if (packetSize) {
        int len = _commandUdp.read(_packetBuffer, 512);
        if (len > 0) {
            _packetBuffer[len] = 0;
        }
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, _packetBuffer);
        if (error) {
            return 0;
        }
        packet->TargetId = doc["TargetId"].as<String>();
        packet->Command = doc["Command"].as<String>();
        packet->Value = doc["Value"].as<double>();
        return packetSize;
    }
    return 0;
}

void NetworkManager::broadcastRtcmData(const uint8_t* data, size_t len) {
    _rtcmUdp.beginPacket(_broadcastIp, RTCM_CORRECTION_PORT);
    _rtcmUdp.write(data, len);
    _rtcmUdp.endPacket();
}

int NetworkManager::readRtcmData(uint8_t* buffer, size_t maxSize) {
    int packetSize = _rtcmUdp.parsePacket();
    if (packetSize) {
        return _rtcmUdp.read(buffer, maxSize);
    }
    return 0;
}

void NetworkManager::setHydraulicController(HydraulicController* controller) {
    _hydraulicController = controller;
}

void NetworkManager::update() {
    // Check for incoming command packets and process them
    if (_hydraulicController != nullptr) {
        ControlCommandPacket commandPacket;
        if (readCommandPacket(&commandPacket) > 0) {
            // The ControlCommandPacket structure uses individual commands
            // For now, we'll pass the entire packet to the hydraulic controller
            // The HydraulicController's setSetpoints method will handle the parsing
            _hydraulicController->setSetpoints(commandPacket);
        }
    }
}

