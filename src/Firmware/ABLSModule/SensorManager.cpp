/*
 * ABLS: Automatic Boom Levelling System
 * Unified Sensor Manager Implementation
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#include "SensorManager.h"
#include "DiagnosticManager.h"

// Static member initialization
SensorManager* SensorManager::_instance = nullptr;

SensorManager::SensorManager() :
    _initialized(false),
    _gpsInitialized(false),
    _imuInitialized(false),
    _radarInitialized(false),
    _moduleRole(MODULE_UNKNOWN),
    _gpsDynamicModel(GPS_MODEL_AUTOMOTIVE),
    _enableDeadReckoning(false),
    _freshGpsData(false),
    _gpsLatitude(0.0),
    _gpsLongitude(0.0),
    _gpsAltitude(0),
    _gpsHorizontalAccuracy(999999),
    _gpsVerticalAccuracy(999999),
    _gpsTimeOfWeek(0),
    _gpsValidFix(false),
    _rtkStatus(RTK_NONE),
    _horizontalAccuracy(99.9f),
    _rtkStatusChanged(false),
    _lastRTKStatusChange(0),
    _fusedLatitude(0.0f),
    _fusedLongitude(0.0f),
    _fusedAltitude(0.0f),
    _velocityX(0.0f),
    _velocityY(0.0f),
    _velocityZ(0.0f),
    _lastGpsUpdateTime(0),
    _lastImuUpdateTime(0),
    _lastFusionUpdate(0),
    _imuQuatI(0.0f),
    _imuQuatJ(0.0f),
    _imuQuatK(0.0f),
    _imuQuatReal(1.0f),
    _imuAccelX(0.0f),
    _imuAccelY(0.0f),
    _imuAccelZ(0.0f),
    _imuGyroX(0.0f),
    _imuGyroY(0.0f),
    _imuGyroZ(0.0f),
    _imuDataValid(false),
    _radarDistance(0.0f),
    _radarDataValid(false),
    _lastRadarUpdate(0)
{
    // Set static instance for callback access
    _instance = this;
}

bool SensorManager::initialize() {
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "Initializing sensors...");
    
    // Get module role for conditional initialization
    _moduleRole = ModuleConfig::getRole();
    
    // Configure based on module role
    switch (_moduleRole) {
        case MODULE_CENTRE:
            _gpsDynamicModel = GPS_MODEL_AUTOMOTIVE;
            _enableDeadReckoning = false;
            DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "Centre module: Automotive GPS, no dead reckoning");
            break;
            
        case MODULE_LEFT:
        case MODULE_RIGHT:
            _gpsDynamicModel = GPS_MODEL_AIRBORNE1G;
            _enableDeadReckoning = true;
            DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "Wing module: Airborne GPS, dead reckoning enabled");
            break;
            
        default:
            DiagnosticManager::logError("SensorManager", "Unknown module role - using default configuration");
            _gpsDynamicModel = GPS_MODEL_AUTOMOTIVE;
            _enableDeadReckoning = false;
            break;
    }
    
    // Initialize all sensors
    _gpsInitialized = initializeGPS();
    _imuInitialized = initializeIMU();
    _radarInitialized = initializeRadar();
    
    _initialized = (_gpsInitialized && _imuInitialized && _radarInitialized);
    
    if (_initialized) {
        DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "All sensors initialized successfully");
        DiagnosticManager::setSensorData(
            getGPSStatusString(),
            getIMUStatusString(),
            getRadarStatusString()
        );
    } else {
        String failedSensors = "";
        if (!_gpsInitialized) failedSensors += "GPS ";
        if (!_imuInitialized) failedSensors += "IMU ";
        if (!_radarInitialized) failedSensors += "Radar ";
        
        DiagnosticManager::logError("SensorManager", "Sensor initialization failed: " + failedSensors);
    }
    
    return _initialized;
}

bool SensorManager::initializeGPS() {
    logSensorStatus("GPS", false); // Starting initialization
    
    // Initialize GPS on Serial1 (UART)
    if (!_gps.begin(Serial1)) {
        logSensorStatus("GPS", false);
        return false;
    }
    
    // Configure GPS for role-specific operation
    configureGPSForRole();
    
    // Enable high-precision position data with callback
    _gps.setAutoHPPOSLLH(true);
    _gps.setAutoHPPOSLLHcallbackPtr(&gpsHPPOSLLHCallback);
    
    // Set navigation frequency to 10Hz
    _gps.setNavigationFrequency(10);
    
    // Turn off NMEA output to reduce noise
    _gps.setI2COutput(COM_TYPE_UBX);
    
    logSensorStatus("GPS", true);
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "GPS configured for " + String(_gpsDynamicModel == GPS_MODEL_AUTOMOTIVE ? "Automotive" : "Airborne") + " mode");
    
    return true;
}

bool SensorManager::initializeIMU() {
    logSensorStatus("IMU", false); // Starting initialization
    
    // Initialize IMU on I2C
    if (!_bno080.begin()) {
        logSensorStatus("IMU", false);
        return false;
    }
    
    // Enable rotation vector (quaternion) at 100Hz
    _bno080.enableRotationVector(10); // 10ms = 100Hz
    
    // Enable accelerometer at 100Hz
    _bno080.enableAccelerometer(10);
    
    // Enable gyroscope at 100Hz
    _bno080.enableGyro(10);
    
    logSensorStatus("IMU", true);
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "IMU configured for 100Hz operation");
    
    return true;
}

bool SensorManager::initializeRadar() {
    logSensorStatus("Radar", false); // Starting initialization
    
    // Initialize radar on I2C
    if (!_radar.begin()) {
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Configure radar for distance detection
    if (!_radar.distanceSetup()) {
        logSensorStatus("Radar", false);
        return false;
    }
    
    logSensorStatus("Radar", true);
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "Radar configured for distance detection");
    
    return true;
}

void SensorManager::configureGPSForRole() {
    // Set dynamic model based on module role
    if (_gpsDynamicModel == GPS_MODEL_AUTOMOTIVE) {
        _gps.setDynamicModel(DYN_MODEL_AUTOMOTIVE);
        DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", "GPS dynamic model set to Automotive");
    } else {
        _gps.setDynamicModel(DYN_MODEL_AIRBORNE1g);
        DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", "GPS dynamic model set to Airborne <1g");
    }
}

void SensorManager::update() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    
    // Update GPS (callback-driven, just check for fresh data)
    updateGPS();
    
    // Update IMU at 100Hz
    if (now - _lastImuUpdateTime >= 10) {
        updateIMU();
        _lastImuUpdateTime = now;
    }
    
    // Update radar at 50Hz
    if (now - _lastRadarUpdate >= 20) {
        updateRadar();
        _lastRadarUpdate = now;
    }
    
    // Update dead reckoning for wing modules
    if (_enableDeadReckoning && (now - _lastFusionUpdate >= 20)) {
        updateDeadReckoning();
        _lastFusionUpdate = now;
    }
    
    // Update RTK status monitoring
    updateRTKStatus();
}

void SensorManager::updateGPS() {
    // GPS data is updated via callback - just check for fresh data
    if (_freshGpsData) {
        _freshGpsData = false;
        _lastGpsUpdateTime = millis();
        
        // Update RTK status based on accuracy
        RTKStatus_t newStatus = determineRTKStatus(_gpsHorizontalAccuracy);
        if (newStatus != _rtkStatus) {
            _rtkStatus = newStatus;
            _rtkStatusChanged = true;
            _lastRTKStatusChange = millis();
            
            String statusStr = (_rtkStatus == RTK_FIXED) ? "Fixed" : 
                              (_rtkStatus == RTK_FLOAT) ? "Float" : "None";
            DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "RTK status changed to: " + statusStr);
        }
    }
}

void SensorManager::updateIMU() {
    // Check if new IMU data is available
    if (_bno080.dataAvailable()) {
        // Read rotation vector (quaternion)
        _imuQuatI = _bno080.getQuatI();
        _imuQuatJ = _bno080.getQuatJ();
        _imuQuatK = _bno080.getQuatK();
        _imuQuatReal = _bno080.getQuatReal();
        
        // Read accelerometer
        _imuAccelX = _bno080.getAccelX();
        _imuAccelY = _bno080.getAccelY();
        _imuAccelZ = _bno080.getAccelZ();
        
        // Read gyroscope
        _imuGyroX = _bno080.getGyroX();
        _imuGyroY = _bno080.getGyroY();
        _imuGyroZ = _bno080.getGyroZ();
        
        _imuDataValid = true;
    }
}

void SensorManager::updateRadar() {
    // Setup for reading
    if (_radar.detectorReadingSetup()) {
        uint32_t numDistances = 0;
        _radar.getNumberDistances(numDistances);
        
        if (numDistances > 0) {
            uint32_t distance = 0;
            _radar.getPeakDistance(0, distance);
            _radarDistance = distance / 1000.0f; // Convert mm to meters
            _radarDataValid = true;
        } else {
            _radarDataValid = false;
        }
    } else {
        _radarDataValid = false;
    }
}

void SensorManager::updateDeadReckoning() {
    // Dead reckoning implementation for wing modules
    if (!_enableDeadReckoning || !_gpsValidFix) return;
    
    uint32_t now = millis();
    float dt = (now - _lastFusionUpdate) / 1000.0f; // Convert to seconds
    
    if (dt <= 0) return;
    
    // Simple dead reckoning: predict position using last known velocity
    // This is a basic implementation - more sophisticated fusion could be added
    
    if (_lastGpsUpdateTime > 0) {
        // Use GPS position as ground truth when available
        _fusedLatitude = _gpsLatitude;
        _fusedLongitude = _gpsLongitude;
        _fusedAltitude = _gpsAltitude / 1000.0f; // Convert mm to meters
    } else {
        // Predict position using IMU data (basic integration)
        // This would need proper coordinate transformation and drift correction
        // For now, just maintain last known position
    }
}

void SensorManager::updateRTKStatus() {
    // Update horizontal accuracy in meters
    _horizontalAccuracy = _gpsHorizontalAccuracy / 10000.0f; // Convert from mm*0.1 to meters
}

RTKStatus_t SensorManager::determineRTKStatus(uint32_t horizontalAccuracy) {
    // Convert from mm*0.1 to meters
    float accuracyMeters = horizontalAccuracy / 10000.0f;
    
    if (accuracyMeters <= 0.02f) {
        return RTK_FIXED;  // <2cm = RTK Fixed
    } else if (accuracyMeters <= 0.50f) {
        return RTK_FLOAT;  // 2cm-50cm = RTK Float
    } else {
        return RTK_NONE;   // >50cm = Standard GPS
    }
}

void SensorManager::gpsHPPOSLLHCallback(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct) {
    if (_instance == nullptr) return;
    
    // Store high-precision GPS data
    _instance->_gpsLatitude = ubxDataStruct->lat * 1e-7 + ubxDataStruct->latHp * 1e-9;
    _instance->_gpsLongitude = ubxDataStruct->lon * 1e-7 + ubxDataStruct->lonHp * 1e-9;
    _instance->_gpsAltitude = ubxDataStruct->hMSL + ubxDataStruct->hMSLHp;
    _instance->_gpsHorizontalAccuracy = ubxDataStruct->hAcc;
    _instance->_gpsVerticalAccuracy = ubxDataStruct->vAcc;
    _instance->_gpsTimeOfWeek = ubxDataStruct->iTOW;
    _instance->_gpsValidFix = (ubxDataStruct->flags.all & 0x01) != 0;
    
    // Set fresh data flag
    _instance->_freshGpsData = true;
}

void SensorManager::forwardRtcmToGps(const uint8_t* data, size_t len) {
    if (!_gpsInitialized) return;
    
    // Forward RTCM correction data to GPS
    _gps.pushRawData(const_cast<uint8_t*>(data), len);
    
    DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", "RTCM data forwarded: " + String(len) + " bytes");
}

void SensorManager::populatePacket(SensorDataPacket* packet) {
    if (!packet) return;
    
    // GPS data
    packet->Latitude = _gpsLatitude;
    packet->Longitude = _gpsLongitude;
    packet->Altitude = _gpsAltitude / 1000.0f; // Convert mm to meters
    packet->GPSFixQuality = _gpsValidFix ? 1 : 0;
    packet->RTKStatus = (uint8_t)_rtkStatus;
    packet->HorizontalAccuracy = _horizontalAccuracy;
    
    // IMU data
    packet->QuaternionW = _imuQuatReal;
    packet->QuaternionX = _imuQuatI;
    packet->QuaternionY = _imuQuatJ;
    packet->QuaternionZ = _imuQuatK;
    packet->AccelX = _imuAccelX;
    packet->AccelY = _imuAccelY;
    packet->AccelZ = _imuAccelZ;
    packet->GyroX = _imuGyroX;
    packet->GyroY = _imuGyroY;
    packet->GyroZ = _imuGyroZ;
    
    // Radar data
    packet->RadarDistance = _radarDistance;
    packet->RadarValid = _radarDataValid ? 1 : 0;
    
    // Timestamp
    packet->Timestamp = millis();
    
    // Sender ID based on module role
    switch (_moduleRole) {
        case MODULE_LEFT:   packet->SenderId = SENDER_LEFT_WING; break;
        case MODULE_CENTRE: packet->SenderId = SENDER_CENTRE; break;
        case MODULE_RIGHT:  packet->SenderId = SENDER_RIGHT_WING; break;
        default:           packet->SenderId = SENDER_UNKNOWN; break;
    }
}

String SensorManager::getGPSStatusString() {
    if (!_gpsInitialized) return "GPS: FAIL";
    
    String status = "GPS: ";
    if (_gpsValidFix) {
        switch (_rtkStatus) {
            case RTK_FIXED: status += "RTK-FIX"; break;
            case RTK_FLOAT: status += "RTK-FLT"; break;
            default:        status += "STD"; break;
        }
        status += " " + String(_horizontalAccuracy, 2) + "m";
    } else {
        status += "NO FIX";
    }
    
    return status;
}

String SensorManager::getIMUStatusString() {
    if (!_imuInitialized) return "IMU: FAIL";
    
    String status = "IMU: ";
    if (_imuDataValid) {
        status += "OK";
    } else {
        status += "NO DATA";
    }
    
    return status;
}

String SensorManager::getRadarStatusString() {
    if (!_radarInitialized) return "Radar: FAIL";
    
    String status = "Radar: ";
    if (_radarDataValid) {
        status += String(_radarDistance, 2) + "m";
    } else {
        status += "NO DATA";
    }
    
    return status;
}

void SensorManager::logSensorStatus(const String& sensor, bool success) {
    String message = sensor + " initialization " + (success ? "successful" : "failed");
    LogLevel_t level = success ? LOG_INFO : LOG_ERROR;
    DiagnosticManager::logMessage(level, "SensorManager", message);
}
