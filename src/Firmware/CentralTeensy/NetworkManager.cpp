#include "NetworkManager.h"

// --- Constructor ---
NetworkManager::NetworkManager(IPAddress remoteIp, unsigned int remotePort, unsigned int localPort) :
    _remoteIp(remoteIp),
    _remotePort(remotePort),
    _localPort(localPort)
{
}

// --- Initialization ---
void NetworkManager::init(byte mac[]) {
    Serial.println("Initializing Network Manager...");
    Ethernet.begin(mac);
    _udp.begin(_localPort);
    Serial.print("Network Manager Initialized. UDP listening on port ");
    Serial.println(_localPort);
}

// --- Send Sensor Packet --- 
void NetworkManager::sendPacket(const SensorDataPacket& packet) {
    StaticJsonDocument<512> doc;

    // Populate JSON document from the packet struct
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

    // Serialize JSON to a buffer and send
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    _udp.beginPacket(_remoteIp, _remotePort);
    _udp.write(jsonBuffer);
    _udp.endPacket();
}

// --- Receive Control Command ---
bool NetworkManager::receiveCommand(ControlCommandPacket& command) {
    int packetSize = _udp.parsePacket();
    if (packetSize) {
        int len = _udp.read(_packetBuffer, 512);
        if (len > 0) {
            _packetBuffer[len] = 0;
        }

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, _packetBuffer);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return false;
        }

        // Populate the command struct from the JSON document
        command.TargetId = doc["TargetId"].as<String>();
        command.Command = doc["Command"].as<String>();
        command.Value = doc["Value"].as<double>();

        return true;
    }
    return false;
}
