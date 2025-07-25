#include "SensorManager.h"
#include <Wire.h>

// --- Constructor ---
SensorManager::SensorManager() {
    // Constructor can be used for initial setup if needed
}

// --- Initialization ---
void SensorManager::init() {
    Serial.println("Initializing Central Sensor Manager...");
    Wire.begin();

    // Initialize BNO080 IMU
    if (!_bno080.begin()) {
        Serial.println("Failed to find BNO080 chip. Check wiring.");
        while(1); // Halt execution
    }
    _bno080.enableGameRotationVector(100); // Request updates at 100Hz (10ms)

    // Initialize Acconeer XM125 Radar
    if (!_radar.begin()) {
        Serial.println("Failed to initialize XM125 Radar. Check wiring.");
        while(1); // Halt execution
    }
    _radar.start(); // Start the sensor

    // Initialize u-blox F9P GPS on Serial1
    Serial1.begin(115200);
    if (!_gps.begin(Serial1)) {
        Serial.println("Failed to initialize u-blox F9P GPS. Check wiring.");
        while(1); // Halt execution
    }
    _gps.setNavigationFrequency(10); // Set GPS rate to 10Hz

    Serial.println("Sensor Manager Initialized.");
}

// --- Main Update Loop ---
void SensorManager::update() {
    // The u-blox library automatically parses incoming data in the background.
    // We just need to call checkUblox() to process new messages.
    _gps.checkUblox();
}

// --- Helper to populate the outgoing sensor packet ---
void SensorManager::populatePacket(CentralSensorDataPacket* packet) {
    // This function reads the latest data directly from the sensors.

    // Get GPS data
    // Note the scaling factors: the library returns values as integers.
    packet->Latitude = _gps.getLatitude() / 10000000.0;
    packet->Longitude = _gps.getLongitude() / 10000000.0;
    packet->Altitude = _gps.getAltitude() / 1000.0; // From mm to m
    packet->GpsHeading = _gps.getHeading() / 100000.0; // From degE-5 to deg
    packet->GpsSpeed = _gps.getGroundSpeed() / 1000.0; // From mm/s to m/s
    packet->Satellites = _gps.getSIV();

    // Get IMU data
    if (_bno080.dataAvailable()) {
        packet->Roll = _bno080.getRoll();
        packet->Pitch = _bno080.getPitch();
        packet->Yaw = _bno080.getYaw();
    }

    // Get Radar data
    if (_radar.dataReady()) {
        packet->RadarDistance = _radar.getDistance();
    }
}
