#include "SensorManager.h"

// --- Constructor ---
SensorManager::SensorManager() {
    // Constructor can be used for initial setup if needed
}

// --- Initialization ---
void SensorManager::init() {
    Serial.println("Initializing Sensor Manager...");
    // TODO: Add initialization code for your specific sensors here.
    // Example: myGNSS.begin();
    // Example: bno.begin();
    Serial.println("Sensor Manager Initialized.");
}

// --- Main Update Loop ---
void SensorManager::update() {
    // ===============================================================
    //  PLACEHOLDER: Replace with actual sensor reading code
    // ===============================================================
    
    // Simulate GPS data
    _currentData.Latitude = -33.8688 + (millis() / 10000000.0);
    _currentData.Longitude = 151.2093 + (millis() / 10000000.0);
    _currentData.Altitude = 58.0;
    _currentData.GpsHeading = 123.45;
    _currentData.GpsSpeed = 25.0;
    _currentData.Satellites = 12;

    // Simulate IMU data
    _currentData.Roll = 1.5 * sin(millis() / 1000.0);
    _currentData.Pitch = -2.0 * cos(millis() / 1500.0);
    _currentData.Yaw = 0.0; // Yaw is often relative, starting at 0

    // Simulate Radar data
    _currentData.RadarDistance = 1.2 + 0.1 * sin(millis() / 500.0);
}

// --- Helper to populate the outgoing sensor packet ---
void SensorManager::populatePacket(SensorDataPacket* packet) {
    // This function copies the most recent sensor readings from our
    // internal storage (_currentData) into the packet being sent out.
    packet->Latitude = _currentData.Latitude;
    packet->Longitude = _currentData.Longitude;
    packet->Altitude = _currentData.Altitude;
    packet->GpsHeading = _currentData.GpsHeading;
    packet->GpsSpeed = _currentData.GpsSpeed;
    packet->Satellites = _currentData.Satellites;
    packet->Roll = _currentData.Roll;
    packet->Pitch = _currentData.Pitch;
    packet->Yaw = _currentData.Yaw;
    packet->RadarDistance = _currentData.RadarDistance;
}
