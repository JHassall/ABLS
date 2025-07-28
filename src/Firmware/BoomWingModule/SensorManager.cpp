#include "SensorManager.h"
#include <Wire.h>

// --- Constructor ---
SensorManager::SensorManager() {
    // Constructor can be used for initial setup if needed
}

// --- Initialization ---
void SensorManager::init() {
    // Initialize fusion state variables
    _fusedLatitude = 0.0; _fusedLongitude = 0.0; _fusedAltitude = 0.0;
    _velocityX = 0.0; _velocityY = 0.0; _velocityZ = 0.0;
    _lastGpsUpdateTime = 0; _lastImuUpdateTime = 0;
    Serial.println("Initializing Boom Wing Module Sensor Manager...");
    Wire.begin();

    // Initialize BNO080 IMU
    if (!_bno080.begin()) {
        Serial.println("Failed to find BNO080 chip. Check wiring.");
        while(1); // Halt execution
    }
    _bno080.enableRotationVector(10); // 10ms = 100Hz
    _bno080.enableLinearAccelerometer(10); // 10ms = 100Hz

    // Initialize Acconeer XM125 Radar
    if (!_radar.begin()) {
        Serial.println("Failed to initialize XM125 Radar. Check wiring.");
        while(1); // Halt execution
    }
    if (_radar.distanceSetup(500, 5000) != 0) { // 0.5m to 5m range
        Serial.println("Radar failed to set up distance mode!");
        return;
    }

    // Initialize u-blox F9P GPS on Serial1
    Serial1.begin(115200);
    if (!_gps.begin(Serial1)) {
        Serial.println("Failed to initialize u-blox F9P GPS. Check wiring.");
        while(1); // Halt execution
    }

    // Configure the UART port to output UBX and accept RTCM input
    if (_gps.setUART1Output(COM_TYPE_UBX | COM_TYPE_RTCM3) == false) {
        Serial.println("*** Warning: setUART1Output failed ***");
    }

    // Set the navigation rate to 10Hz, which is optimal for the F9P
    _gps.setNavigationFrequency(10);

    // Set the dynamic model to Airborne1G for best performance on the boomspray's outer wings
    if (!_gps.setDynamicModel(DYN_MODEL_AIRBORNE1g, VAL_LAYER_RAM_BBR)) {
        Serial.println("*** Warning: setDynamicModel failed ***");
    } else {
        Serial.println("Dynamic platform model set to AIRBORNE1G.");
    }

    // Save the new navigation settings to flash
    if (_gps.saveConfigSelective(VAL_CFG_SUBSEC_NAVCONF)) {
        Serial.println("GPS NAV settings saved to flash.");
    } else {
        Serial.println("*** Warning: saveConfigSelective failed ***");
    }

    Serial.println("Sensor Manager Initialized.");
}

// --- Main Update Loop ---
void SensorManager::update() {
    // Always check for new data from sensors first.
    _gps.checkUblox();

    // Now, run the fusion algorithm with the latest available data.
    updateFusion();
}

// --- Helper to populate the outgoing sensor packet ---
void SensorManager::updateFusion() {
    unsigned long currentTime = millis();

    // --- Correct step (with GPS) ---
    // getHPPOSLLH provides high-precision position data for RTK applications
    if (_gps.getHPPOSLLH() && (_gps.getFixType() == 3) && (_gps.getCarrierSolutionType() == 2)) { // New, valid, 3D RTK Fixed solution
        // Use high-precision GPS data for maximum accuracy
        int32_t lat = _gps.getHighResLatitude();
        int8_t latHp = _gps.getHighResLatitudeHp();
        int32_t lon = _gps.getHighResLongitude();
        int8_t lonHp = _gps.getHighResLongitudeHp();
        int32_t alt = _gps.getMeanSeaLevel();
        int8_t altHp = _gps.getMeanSeaLevelHp();
        
        // Convert to high-precision coordinates
        _fusedLatitude = ((double)lat) / 10000000.0 + ((double)latHp) / 1000000000.0;
        _fusedLongitude = ((double)lon) / 10000000.0 + ((double)lonHp) / 1000000000.0;
        _fusedAltitude = ((double)alt + (double)altHp * 0.1) / 1000.0; // Convert mm to meters

        // Get velocity in North-East-Down (NED) frame from GPS
        // Calculate velocity components from ground speed and heading
        float groundSpeed = _gps.getGroundSpeed() / 1000.0; // Convert from mm/s to m/s
        float heading = _gps.getHeading() / 100000.0; // Convert from 1e-5 degrees to degrees
        float headingRad = heading * PI / 180.0; // Convert to radians
        
        _velocityX = groundSpeed * cos(headingRad); // North velocity component
        _velocityY = groundSpeed * sin(headingRad); // East velocity component
        _velocityZ = 0.0; // Vertical velocity not available from basic GPS data

        _lastGpsUpdateTime = currentTime;
        _lastImuUpdateTime = currentTime; // Reset IMU timer to sync
    }
    // --- Predict step (with IMU) ---
    else if (_bno080.dataAvailable()) {
        if (_lastImuUpdateTime == 0) { // First IMU reading, do nothing
            _lastImuUpdateTime = currentTime;
            return;
        }

        float dt = (currentTime - _lastImuUpdateTime) / 1000.0f; // Time delta in seconds

        // Get linear (gravity-compensated) acceleration in the sensor's body frame
        float ax = _bno080.getLinAccelX();
        float ay = _bno080.getLinAccelY();
        float az = _bno080.getLinAccelZ();

        // Get the rotation quaternion from the IMU
        float qx = _bno080.getQuatI();
        float qy = _bno080.getQuatJ();
        float qz = _bno080.getQuatK();
        float qw = _bno080.getQuatReal();

        // Rotate the acceleration vector from the body frame to the world (NED) frame
        // using the quaternion. This is a standard quaternion-vector rotation formula.
        float world_ax = (1 - 2*qy*qy - 2*qz*qz) * ax + (2*qx*qy - 2*qz*qw) * ay + (2*qx*qz + 2*qy*qw) * az;
        float world_ay = (2*qx*qy + 2*qz*qw) * ax + (1 - 2*qx*qx - 2*qz*qz) * ay + (2*qy*qz - 2*qx*qw) * az;
        float world_az = (2*qx*qz - 2*qy*qw) * ax + (2*qy*qz + 2*qx*qw) * ay + (1 - 2*qx*qx - 2*qy*qy) * az;

        // Integrate the world-frame acceleration to update velocity
        _velocityX += world_ax * dt;
        _velocityY += world_ay * dt;
        _velocityZ += world_az * dt;

        // Integrate velocity to update position (using simple approximation)
        const float earthRadius = 6371000.0; // meters
        _fusedLatitude += (_velocityX * dt) / earthRadius * (180.0 / PI);
        _fusedLongitude += (_velocityY * dt) / (earthRadius * cos(_fusedLatitude * PI / 180.0)) * (180.0 / PI);
        _fusedAltitude -= _velocityZ * dt; // Altitude is usually 'up', NED is 'down'

        _lastImuUpdateTime = currentTime;
    }
}

void SensorManager::populatePacket(SensorDataPacket* packet) {
    if (!packet) return;

    // GPS Data
    // Populate with the fused position data (already high-precision from sensor fusion)
    packet->Latitude = _fusedLatitude;
    packet->Longitude = _fusedLongitude;
    packet->Altitude = _fusedAltitude;

    // Heading and other data can still come directly from sensors
    packet->GpsHeading = _bno080.getYaw();
    packet->GpsSpeed = _gps.getGroundSpeed();
    packet->Satellites = _gps.getSIV();
    
    // RTK Quality Assessment - monitor horizontal accuracy for RTK performance
    uint32_t accuracy = _gps.getHorizontalAccuracy();
    float horizontalAccuracy = accuracy / 10000.0; // Convert from mm * 10^-1 to meters
    // Note: Could add accuracy to packet structure for monitoring RTK quality
    (void)horizontalAccuracy; // Suppress unused variable warning

    // IMU Data
    if (_bno080.dataAvailable()) {
        packet->Roll = _bno080.getRoll();
        packet->Pitch = _bno080.getPitch();
        packet->Yaw = _bno080.getYaw();
    }

    // Radar Data
    if (_radar.detectorReadingSetup() == 0) {
        uint32_t numDistances = 0;
        _radar.getNumberDistances(numDistances);
        if (numDistances > 0) {
            uint32_t distance = 0;
            // Get the first and strongest peak
            if (_radar.getPeakDistance(0, distance) == 0) {
                packet->RadarDistance = distance;
            }
        }
    }
}

// --- Forward RTCM Data to GPS ---
void SensorManager::forwardRtcmToGps(const uint8_t* data, size_t len) {
    // Forward the received RTCM data directly to the GPS module
    _gps.pushRawData(const_cast<uint8_t*>(data), len);
}
