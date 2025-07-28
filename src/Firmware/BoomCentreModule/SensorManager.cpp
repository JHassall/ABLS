#include "SensorManager.h"
#include <Wire.h>

// --- Constructor ---
SensorManager::SensorManager() {
    // Constructor can be used for initial setup if needed
}

// --- Initialization ---
void SensorManager::init() {
    Serial.println("Initializing Boom Centre Module Sensor Manager...");
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

    // Set the dynamic model to Automotive for best performance on a ground vehicle
    if (_gps.setDynamicModel(DYN_MODEL_AUTOMOTIVE, VAL_LAYER_RAM_BBR) == false) {
        Serial.println("*** Warning: setDynamicModel failed ***");
    } else {
        Serial.println("Dynamic platform model set to AUTOMOTIVE.");
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
    // The u-blox library automatically parses incoming data in the background.
    // We just need to call checkUblox() to process new messages.
    _gps.checkUblox();
}

// --- Helper to populate the outgoing sensor packet ---
void SensorManager::populatePacket(SensorDataPacket* packet) {
    if (!packet) return;

    // High-Precision GPS Data (for ZED-F9P RTK)
    // Combine high-resolution components for maximum precision
    int32_t lat = _gps.getHighResLatitude();
    int8_t latHp = _gps.getHighResLatitudeHp();
    int32_t lon = _gps.getHighResLongitude();
    int8_t lonHp = _gps.getHighResLongitudeHp();
    int32_t alt = _gps.getMeanSeaLevel();
    int8_t altHp = _gps.getMeanSeaLevelHp();
    
    // Convert to double precision for maximum accuracy
    packet->Latitude = ((double)lat) / 10000000.0 + ((double)latHp) / 1000000000.0;
    packet->Longitude = ((double)lon) / 10000000.0 + ((double)lonHp) / 1000000000.0;
    packet->Altitude = ((double)alt + (double)altHp * 0.1) / 1000.0; // Convert mm to meters
    
    // Standard velocity and satellite data
    packet->GpsHeading = _gps.getHeading();
    packet->GpsSpeed = _gps.getGroundSpeed();
    packet->Satellites = _gps.getSIV();
    
    // RTK Quality Assessment - horizontal accuracy in meters
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
