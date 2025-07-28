#include "SensorManager.h"
#include <Wire.h>

// Static member initialization
SensorManager* SensorManager::_instance = nullptr;

// --- Constructor ---
SensorManager::SensorManager() {
    // Set static instance pointer for callback access
    _instance = this;
    _freshGpsData = false;
    
    // Initialize RTK quality monitoring
    _rtkStatus = RTK_NONE;
    _horizontalAccuracy = 999.0; // Start with poor accuracy
    _rtkStatusChanged = false;
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
    
    // Configure GPS for callback-based high-precision data processing
    _gps.setAutoHPPOSLLHcallbackPtr(&gpsHPPOSLLHCallback);
    Serial.println("GPS HPPOSLLH callback configured for real-time processing.");

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
    // Check for incoming GPS data and process any pending callbacks
    _gps.checkUblox();     // Check for new GPS data
    _gps.checkCallbacks(); // Process any pending GPS callbacks

    // Run the sensor fusion algorithm (now callback-driven for GPS)
    updateFusion();
}

// --- Helper to populate the outgoing sensor packet ---
void SensorManager::updateFusion() {
    unsigned long currentTime = millis();

    // --- Correct step (with GPS) ---
    // Check if fresh GPS data is available from callback
    if (_freshGpsData) {
        // GPS position data has already been updated by the callback
        // Now update velocity components using current GPS data
        float groundSpeed = _gps.getGroundSpeed() / 1000.0; // Convert from mm/s to m/s
        float heading = _gps.getHeading() / 100000.0; // Convert from 1e-5 degrees to degrees
        float headingRad = heading * PI / 180.0; // Convert to radians
        
        _velocityX = groundSpeed * cos(headingRad); // North velocity component
        _velocityY = groundSpeed * sin(headingRad); // East velocity component
        _velocityZ = 0.0; // Vertical velocity not available from basic GPS data

        // Clear the fresh GPS data flag
        _freshGpsData = false;
        
        Serial.println("Sensor Fusion: GPS correction applied via callback");
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
    
    // RTK Quality Data - now included in data packets for Toughbook decision making
    packet->RTKStatus = (uint8_t)_rtkStatus;           // Current RTK status (0=None, 1=Float, 2=Fixed)
    packet->HorizontalAccuracy = _horizontalAccuracy;   // Current horizontal accuracy in meters
    packet->GPSTimestamp = _gps.getTimeOfWeek();       // GPS time of week for synchronization

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

// --- GPS HPPOSLLH Callback Function ---
// This function is called automatically when new high-precision GPS data arrives
void SensorManager::gpsHPPOSLLHCallback(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct) {
    if (_instance == nullptr) return; // Safety check
    
    // Extract high-precision GPS data following SparkFun example pattern
    long highResLatitude = ubxDataStruct->lat;
    int highResLatitudeHp = ubxDataStruct->latHp;
    long highResLongitude = ubxDataStruct->lon;
    int highResLongitudeHp = ubxDataStruct->lonHp;
    long altitude = ubxDataStruct->hMSL;
    int altitudeHp = ubxDataStruct->hMSLHp;
    
    // Calculate horizontal accuracy for RTK quality assessment
    float horizAccuracy = ((float)ubxDataStruct->hAcc) / 10000.0; // Convert hAcc from mm*0.1 to m
    
    // Update horizontal accuracy tracking
    _instance->_horizontalAccuracy = horizAccuracy;
    
    // Determine RTK status based on horizontal accuracy
    RTKStatus_t newStatus;
    if (horizAccuracy < 0.02) {          // Sub-2cm = RTK Fixed
        newStatus = RTK_FIXED;
    } else if (horizAccuracy < 0.5) {    // 2cm-50cm = RTK Float  
        newStatus = RTK_FLOAT;
    } else {                             // >50cm = Standard GPS
        newStatus = RTK_NONE;
    }
    
    // Check for RTK status changes
    if (newStatus != _instance->_rtkStatus) {
        _instance->_rtkStatus = newStatus;
        _instance->_rtkStatusChanged = true;
        
        // Print RTK status change notifications
        Serial.print(F("RTK Status Change: "));
        switch (newStatus) {
            case RTK_FIXED:
                Serial.println(F("RTK FIXED - High precision mode (<2cm)"));
                break;
            case RTK_FLOAT:
                Serial.println(F("RTK FLOAT - Medium precision mode (2-50cm)"));
                break;
            case RTK_NONE:
                Serial.println(F("RTK NONE - Standard GPS mode (>50cm)"));
                break;
        }
    }
    
    // Convert to high-precision coordinates for sensor fusion
    _instance->_fusedLatitude = ((double)highResLatitude) / 10000000.0 + ((double)highResLatitudeHp) / 1000000000.0;
    _instance->_fusedLongitude = ((double)highResLongitude) / 10000000.0 + ((double)highResLongitudeHp) / 1000000000.0;
    _instance->_fusedAltitude = ((double)altitude + (double)altitudeHp * 0.1) / 1000.0; // Convert mm to meters
    
    // Reset timing for sensor fusion synchronization
    _instance->_lastGpsUpdateTime = millis();
    _instance->_lastImuUpdateTime = _instance->_lastGpsUpdateTime; // Sync IMU timer
    
    // Set flag indicating fresh GPS data is available
    _instance->_freshGpsData = true;
    
    // Print debug information following SparkFun example format
    Serial.println();
    Serial.print(F("Hi Res Lat: "));
    Serial.print(highResLatitude);
    Serial.print(F(" "));
    Serial.print(highResLatitudeHp);
    Serial.print(F(" Hi Res Long: "));
    Serial.print(highResLongitude);
    Serial.print(F(" "));
    Serial.print(highResLongitudeHp);
    Serial.print(F(" Horiz accuracy: "));
    Serial.print(horizAccuracy, 4);
    Serial.println(F(" m"));
}
