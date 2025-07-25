#include "NetworkManager.h"

// --- Constructor ---
NetworkManager::NetworkManager(IPAddress remoteIp, unsigned int remotePort) :
    _remoteIp(remoteIp),
    _remotePort(remotePort)
{
}

// --- Initialization ---
void NetworkManager::init(byte mac[], unsigned int localPort) {
    Serial.println("Initializing Network Manager...");
    Ethernet.begin(mac);
    _udp.begin(localPort);
    Serial.print("Network Manager Initialized. UDP sending to ");
    Serial.print(_remoteIp);
    Serial.print(":");
    Serial.println(_remotePort);
}

// --- Send Sensor Packet --- 
void NetworkManager::sendPacket(const OuterSensorDataPacket& packet) {
    StaticJsonDocument<512> doc;

    // Populate JSON document from the packet struct
    doc["PacketType"] = packet.PacketType;
    doc["SenderId"] = packet.SenderId;
    doc["Latitude"] = packet.Latitude;
    doc["Longitude"] = packet.Longitude;
    doc["Altitude"] = packet.Altitude;
    doc["Heading"] = packet.Heading;
    doc["Speed"] = packet.Speed;
    doc["Satellites"] = packet.Satellites;
    doc["Roll"] = packet.Roll;
    doc["Pitch"] = packet.Pitch;
    doc["Yaw"] = packet.Yaw;
    doc["RadarDistance"] = packet.RadarDistance;

    // Serialize JSON to a buffer and send
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    _udp.beginPacket(_remoteIp, _remotePort);
    _udp.write(jsonBuffer);
    _udp.endPacket();
}
