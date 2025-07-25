#ifndef DATA_PACKETS_H
#define DATA_PACKETS_H

// =================================================================
// Data Structures for Outer Teensy -> Toughbook Communication
// =================================================================
// These structures must be kept in sync with the C# definitions
// in the ABLS.Core application.
// =================================================================

// --- Outgoing: Fused Sensor Data from an Outer Teensy to the Toughbook ---
struct OuterSensorDataPacket {
    // Packet Metadata
    const char* PacketType = "OuterSensorData";
    char SenderId[16]; // Will be set to "OuterLeft" or "OuterRight"

    // Fused GPS/IMU Data (output from dead reckoning)
    double Latitude = 0.0;
    double Longitude = 0.0;
    double Altitude = 0.0;
    float Heading = 0.0;
    float Speed = 0.0;
    int Satellites = 0;

    // Fused IMU Data (from BNO080)
    float Roll = 0.0;
    float Pitch = 0.0;
    float Yaw = 0.0;

    // Radar Data
    float RadarDistance = 0.0;
};

#endif // DATA_PACKETS_H
