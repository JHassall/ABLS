#include "SensorManager.h"
#include <Wire.h>

// --- Constants ---
// Earth radius in meters, used for position calculation
const double EARTH_RADIUS_METERS = 6371000.0;

// --- Constructor ---
SensorManager::SensorManager() {
    // Constructor can be used for initial setup if needed
}

// --- Initialization ---
void SensorManager::init() {
    Serial.println("Initializing Outer Teensy Sensor Manager...");
    Wire.begin();
    
    if (!_bno080.begin()) {
        Serial.println("Failed to find BNO080 chip. Check wiring.");
        while(1); // Halt execution
    }

    // The BNO080's internal processor can provide many types of reports.
    // We request the "Game Rotation Vector", which gives us a stable, fused
    // orientation (Roll, Pitch, Yaw) without magnetic interference.
    _bno080.enableGameRotationVector(10); // Request updates at 100Hz (10ms)

    // TODO: Add initialization for your GPS and Radar sensors here

    _lastImuPredictionTime = micros();
    Serial.println("Sensor Manager Initialized.");
}

// --- Main Update Loop ---
// This should be called as fast as possible in the main loop()
void SensorManager::update() {
    // Check if the BNO080 has new data for us
    if (_bno080.dataAvailable()) {
        onImuUpdate();
    }

    // TODO: Add code here to check for new GPS data and call onGpsUpdate()
    // Example:
    // if (myGNSS.getPVT()) { // If new GPS data is available
    //     onGpsUpdate(/* pass the relevant GPS data here */);
    // }
}

// --- GPS Update (The "Correction" Step) ---
void SensorManager::onGpsUpdate(/* Pass in your GPS data object here */) {
    // =====================================================================
    //  PLACEHOLDER: Replace with your actual GPS library's data
    // =====================================================================
    // This function is the "Correction" step. It snaps our state to the
    // high-accuracy GPS reading.

    // Example using a hypothetical gpsObject:
    // _currentState.latitude = gpsObject.latitude;
    // _currentState.longitude = gpsObject.longitude;
    // _currentState.altitude = gpsObject.altitude;
    // _currentState.speed = gpsObject.groundSpeed;
    // _currentState.heading = gpsObject.heading;
    // _currentState.satellites = gpsObject.satelliteCount;
}

// --- IMU Update (The "Prediction" Step) ---
void SensorManager::onImuUpdate() {
    // Update our orientation with the latest from the BNO080
    _currentState.roll = _bno080.getRoll();
    _currentState.pitch = _bno080.getPitch();
    _currentState.yaw = _bno080.getYaw();

    // Predict our new position based on the last known velocity
    predictNewPosition();
}

// --- Dead Reckoning Prediction ---
void SensorManager::predictNewPosition() {
    unsigned long currentTime = micros();
    double timeDeltaSeconds = (currentTime - _lastImuPredictionTime) / 1000000.0;
    _lastImuPredictionTime = currentTime;

    // Calculate distance traveled since last prediction
    double distanceMeters = _currentState.speed * timeDeltaSeconds;

    // Convert heading to radians for trig functions
    double headingRad = _currentState.heading * DEG_TO_RAD;

    // Calculate change in latitude and longitude
    double latRad = _currentState.latitude * DEG_TO_RAD;
    double deltaLat = (distanceMeters * cos(headingRad)) / EARTH_RADIUS_METERS;
    double deltaLon = (distanceMeters * sin(headingRad)) / (EARTH_RADIUS_METERS * cos(latRad));

    // Apply the change to our state
    _currentState.latitude += deltaLat * RAD_TO_DEG;
    _currentState.longitude += deltaLon * RAD_TO_DEG;
}


// --- Helper to populate the outgoing sensor packet ---
void SensorManager::populatePacket(OuterSensorDataPacket* packet) {
    packet->Latitude = _currentState.latitude;
    packet->Longitude = _currentState.longitude;
    packet->Altitude = _currentState.altitude;
    packet->Heading = _currentState.heading;
    packet->Speed = _currentState.speed;
    packet->Satellites = _currentState.satellites;
    packet->Roll = _currentState.roll;
    packet->Pitch = _currentState.pitch;
    packet->Yaw = _currentState.yaw;

    // TODO: Read the radar sensor and populate the packet
    // packet->RadarDistance = readRadar(); 
}
