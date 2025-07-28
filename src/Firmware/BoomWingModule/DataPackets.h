#ifndef DATA_PACKETS_H
#define DATA_PACKETS_H

// =================================================================
// Data Structures for Teensy <-> Toughbook Communication
// =================================================================
// These structures must be kept in sync with the C# definitions
// in the ABLS.Core application.
// =================================================================

// UDP Port Definitions
const unsigned int SENSOR_DATA_PORT = 4444;
const unsigned int HYDRAULIC_COMMAND_PORT = 4445;
const unsigned int RTCM_CORRECTION_PORT = 4446;

// --- Outgoing: Sensor Data from Central Teensy to Toughbook ---
struct SensorDataPacket {
    // Packet Metadata
    const char* PacketType = "SensorData";
    const char* SenderId = "BoomWingModule";

    // GPS Data
    double Latitude = 0.0;
    double Longitude = 0.0;
    double Altitude = 0.0;
    float GpsHeading = 0.0;
    float GpsSpeed = 0.0;
    int Satellites = 0;

    // IMU Data
    float Roll = 0.0;
    float Pitch = 0.0;
    float Yaw = 0.0;

    // Radar Data
    float RadarDistance = 0.0;
};


// --- Incoming: Control Commands from Toughbook to Central Teensy ---
struct ControlCommandPacket {
    // The ID of the ram to control (e.g., "ram_center", "ram_left")
    String TargetId;

    // The type of command (e.g., "setpoint")
    String Command;

    // The value for the command (e.g., 75.5 for 75.5%)
    double Value = 0.0;
};

#endif // DATA_PACKETS_H
