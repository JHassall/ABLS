#ifndef DATA_PACKETS_H
#define DATA_PACKETS_H

#include <Arduino.h>

// =================================================================
// Data Structures for ABLS Unified Firmware Communication
// =================================================================
// These structures define the communication protocol between
// ABLS modules and the Toughbook control system.
// =================================================================

// UDP Port Definitions
const unsigned int SENSOR_DATA_PORT = 8001;
const unsigned int COMMAND_PORT = 8002;
const unsigned int RTCM_PORT = 8003;
const unsigned int OTA_COMMAND_PORT = 8004;
const unsigned int OTA_RESPONSE_PORT = 8005;

// Sender ID enumeration
typedef enum {
    SENDER_LEFT_WING = 0,
    SENDER_CENTRE = 1,
    SENDER_RIGHT_WING = 2,
    SENDER_UNKNOWN = 255
} SenderId_t;

// --- Outgoing: Sensor Data from ABLS Modules to Toughbook ---
struct SensorDataPacket {
    // Packet Metadata
    uint8_t SenderId = SENDER_UNKNOWN;
    uint32_t Timestamp = 0;
    
    // GPS Data (High-Precision)
    double Latitude = 0.0;
    double Longitude = 0.0;
    double Altitude = 0.0;
    float GpsHeading = 0.0;
    float GpsSpeed = 0.0;
    int Satellites = 0;
    uint8_t GPSFixQuality = 0;       // 0=No fix, 1=GPS fix, 2=DGPS fix
    
    // RTK Quality Data
    uint8_t RTKStatus = 0;           // 0=None, 1=Float, 2=Fixed
    float HorizontalAccuracy = 999.0; // Accuracy in meters
    uint32_t GPSTimestamp = 0;       // iTOW for synchronization

    // IMU Data (Quaternion + Linear)
    float QuaternionW = 1.0;
    float QuaternionX = 0.0;
    float QuaternionY = 0.0;
    float QuaternionZ = 0.0;
    float AccelX = 0.0;
    float AccelY = 0.0;
    float AccelZ = 0.0;
    float GyroX = 0.0;
    float GyroY = 0.0;
    float GyroZ = 0.0;

    // Radar Data
    float RadarDistance = 0.0;
    uint8_t RadarValid = 0;          // 0=Invalid, 1=Valid
    
    // Hydraulic Ram Positions (Centre module only)
    float RamPosCenterPercent = 50.0;
    float RamPosLeftPercent = 50.0;
    float RamPosRightPercent = 50.0;
};

// --- Incoming: Control Commands from Toughbook to Centre Module ---
struct ControlCommandPacket {
    // Command Metadata
    uint32_t CommandId = 0;
    uint32_t Timestamp = 0;
    
    // Hydraulic Ram Setpoints (0-100%)
    float SetpointCenter = 50.0;
    float SetpointLeft = 50.0;
    float SetpointRight = 50.0;
    
    // Command Flags
    uint8_t EmergencyStop = 0;       // 1=Emergency stop
    uint8_t SystemEnable = 1;        // 1=System enabled
};

#endif // DATA_PACKETS_H
