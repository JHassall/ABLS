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
        DiagnosticManager::logError("SensorManager", "IMU I2C initialization failed");
        logSensorStatus("IMU", false);
        return false;
    }
    
    // COMPREHENSIVE IMU CONFIGURATION based on SparkFun BNO080 examples
    
    // Increase I2C speed for better performance (as shown in SparkFun examples)
    Wire.setClock(400000); // 400kHz I2C for faster IMU communication
    
    // Enable dynamic calibration for accelerometer and gyroscope (no magnetometer due to metal boom)
    // This enables automatic calibration as shown in SparkFun Example9-Calibrate
    _bno080.calibrateAccelerometer(); // Enable calibration for Accelerometer
    _bno080.calibrateGyro(); // Enable calibration for Gyroscope
    // Note: Magnetometer calibration disabled due to metal boom interference
    
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "IMU dynamic calibration enabled for all sensors");
    
    // Enable multiple sensor outputs for comprehensive motion sensing
    
    // Primary sensors for navigation and control
    // Note: BNO080 enable methods return void, so we can't check for errors during enable
    _bno080.enableRotationVector(10); // 10ms = 100Hz - quaternion for orientation
    _bno080.enableAccelerometer(10); // 10ms = 100Hz - raw acceleration  
    _bno080.enableGyro(10); // 10ms = 100Hz - angular velocity
    
    // Additional sensors for enhanced accuracy and diagnostics
    
    // Linear accelerometer (gravity-compensated) - critical for motion analysis
    _bno080.enableLinearAccelerometer(10); // 10ms = 100Hz
    
    // Magnetometer disabled due to metal boom mounting (magnetic interference)
    // Using Game Rotation Vector instead for orientation without magnetometer dependency
    
    // Game rotation vector (no magnetometer) for backup orientation
    _bno080.enableGameRotationVector(20); // 20ms = 50Hz
    
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "IMU sensors enabled: Rotation Vector, Accelerometer, Gyro, Linear Accel, Game Vector");
    
    // Allow sensors to stabilize and begin calibration
    delay(100);
    
    // Check initial calibration status
    if (_bno080.dataAvailable()) {
        // Try to get initial accuracy readings
        byte quatAccuracy = _bno080.getQuatAccuracy();
        byte accelAccuracy = _bno080.getAccelAccuracy();
        byte gyroAccuracy = _bno080.getGyroAccuracy();
        // Note: Game rotation vector accuracy not available in this BNO080 library version
        
        DiagnosticManager::logMessage(LOG_INFO, "SensorManager", 
            "IMU initial accuracy - Quat: " + String(quatAccuracy) + 
            ", Accel: " + String(accelAccuracy) + 
            ", Gyro: " + String(gyroAccuracy) + " (0=Unreliable, 3=High, Mag disabled for metal boom)");
    }
    
    // Initialize performance monitoring variables
    _imuDataCount = 0;
    _imuStartTime = millis();
    _lastCalibrationCheck = millis();
    
    logSensorStatus("IMU", true);
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", 
        "IMU configured successfully - Rotation Vector, Linear Accel, Gyro, Game Vector @ 100Hz with dynamic calibration (Mag disabled for metal boom)");
    
    return true;
}

bool SensorManager::initializeRadar() {
    logSensorStatus("Radar", false); // Starting initialization
    
    // Initialize radar on I2C
    if (!_radar.begin()) {
        DiagnosticManager::logError("SensorManager", "Radar I2C initialization failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // COMPREHENSIVE RADAR CONFIGURATION based on SparkFun XM125 examples
    
    // Reset sensor configuration to ensure clean state
    if (_radar.setCommand(SFE_XM125_DISTANCE_RESET_MODULE) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar reset command failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Wait for reset to complete
    if (_radar.busyWait() != 0) {
        DiagnosticManager::logError("SensorManager", "Radar reset busy wait failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Check for errors after reset
    uint32_t errorStatus = 0;
    _radar.getDetectorErrorStatus(errorStatus);
    if (errorStatus != 0) {
        DiagnosticManager::logError("SensorManager", "Radar detector error after reset: " + String(errorStatus));
        logSensorStatus("Radar", false);
        return false;
    }
    
    delay(100); // Allow sensor to stabilize
    
    // Configure detection range for boom height sensing (100mm to 3000mm)
    // Start range: 100mm (10cm minimum boom height)
    if (_radar.setStart(100) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar start range configuration failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // End range: 3000mm (3m maximum boom height)
    if (_radar.setEnd(3000) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar end range configuration failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Configure threshold settings for agricultural environment
    // Threshold sensitivity: 200 (moderate sensitivity to ignore spray droplets/dust)
    if (_radar.setThresholdSensitivity(200) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar threshold sensitivity configuration failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Fixed amplitude threshold: 150 (filter out weak reflections)
    if (_radar.setFixedAmpThreshold(150) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar amplitude threshold configuration failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    delay(100); // Allow configuration to settle
    
    // Apply configuration
    if (_radar.setCommand(SFE_XM125_DISTANCE_APPLY_CONFIGURATION) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar configuration application failed");
        
        // Check for specific error details
        _radar.getDetectorErrorStatus(errorStatus);
        if (errorStatus != 0) {
            DiagnosticManager::logError("SensorManager", "Radar detector error during config: " + String(errorStatus));
        }
        
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Wait for configuration to complete
    if (_radar.busyWait() != 0) {
        DiagnosticManager::logError("SensorManager", "Radar configuration busy wait failed");
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Final error status check
    _radar.getDetectorErrorStatus(errorStatus);
    if (errorStatus != 0) {
        DiagnosticManager::logError("SensorManager", "Radar detector error after configuration: " + String(errorStatus));
        logSensorStatus("Radar", false);
        return false;
    }
    
    // Verify configuration by reading back settings
    uint32_t startVal = 0, endVal = 0;
    _radar.getStart(startVal);
    _radar.getEnd(endVal);
    
    DiagnosticManager::logMessage(LOG_INFO, "SensorManager", 
        "Radar configured successfully - Range: " + String(startVal) + "mm to " + String(endVal) + "mm");
    
    logSensorStatus("Radar", true);
    
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
    } else {
        // ENHANCED ERROR RECOVERY: Check for GPS timeout
        if (_lastGpsUpdateTime > 0 && (millis() - _lastGpsUpdateTime > 10000)) { // 10 second timeout
            if (_gpsValidFix) {
                DiagnosticManager::logError("SensorManager", "GPS communication timeout - no data for 10 seconds");
                _gpsValidFix = false; // Mark GPS as invalid due to timeout
            }
        }
    }
}

void SensorManager::updateIMU() {
    // COMPREHENSIVE IMU UPDATE based on SparkFun BNO080 examples (no magnetometer due to metal boom)
    
    // Check if new IMU data is available
    if (_bno080.dataAvailable()) {
        // ACCURACY MONITORING - Check sensor accuracy levels (SparkFun Example9-Calibrate pattern)
        byte quatAccuracy = _bno080.getQuatAccuracy();
        byte accelAccuracy = _bno080.getAccelAccuracy();
        byte gyroAccuracy = _bno080.getGyroAccuracy();
        byte linAccelAccuracy = _bno080.getLinAccelAccuracy();
        // Note: Game rotation vector accuracy not available in this BNO080 library version
        
        // CALIBRATION STATUS MONITORING - Check periodically for calibration needs
        uint32_t now = millis();
        if (now - _lastCalibrationCheck > 30000) { // Check every 30 seconds
            _lastCalibrationCheck = now;
            
            // Log accuracy status for diagnostics
            if (quatAccuracy < 2 || accelAccuracy < 2 || gyroAccuracy < 2) {
                DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", 
                    "IMU calibration status - Quat: " + String(quatAccuracy) + 
                    ", Accel: " + String(accelAccuracy) + 
                    ", Gyro: " + String(gyroAccuracy) + 
                    ", LinAccel: " + String(linAccelAccuracy) + " (2+ recommended for reliable operation)");
            }
        }
        
        // PRIMARY SENSOR DATA - Read rotation vector (quaternion)
        float quatI = _bno080.getQuatI();
        float quatJ = _bno080.getQuatJ();
        float quatK = _bno080.getQuatK();
        float quatReal = _bno080.getQuatReal();
        
        // ENHANCED VALIDATION: Check quaternion accuracy before using
        if (quatAccuracy == 0) {
            DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", "IMU quaternion accuracy unreliable - continuing with available data");
            // Note: Game rotation vector methods not available in this BNO080 library version
            // Continue with standard quaternion data but mark as lower confidence
        }
        
        // Validate quaternion magnitude
        float quatMagnitude = sqrt(quatI*quatI + quatJ*quatJ + quatK*quatK + quatReal*quatReal);
        if (quatMagnitude < 0.9f || quatMagnitude > 1.1f) {
            DiagnosticManager::logError("SensorManager", "Invalid IMU quaternion magnitude: " + String(quatMagnitude, 4));
            _imuDataValid = false;
            return;
        }
        
        // RAW ACCELEROMETER - Read raw acceleration (includes gravity)
        float accelX = _bno080.getAccelX();
        float accelY = _bno080.getAccelY();
        float accelZ = _bno080.getAccelZ();
        
        // Validate accelerometer data (reasonable range: -50g to +50g)
        if (abs(accelX) > 50.0f || abs(accelY) > 50.0f || abs(accelZ) > 50.0f) {
            DiagnosticManager::logError("SensorManager", "Invalid IMU acceleration values: X=" + String(accelX, 2) + ", Y=" + String(accelY, 2) + ", Z=" + String(accelZ, 2));
            _imuDataValid = false;
            return;
        }
        
        // LINEAR ACCELEROMETER - Read gravity-compensated acceleration (SparkFun Example12 pattern)
        float linAccelX = _bno080.getLinAccelX();
        float linAccelY = _bno080.getLinAccelY();
        float linAccelZ = _bno080.getLinAccelZ();
        
        // Validate linear acceleration (should be smaller than raw accel)
        if (abs(linAccelX) > 20.0f || abs(linAccelY) > 20.0f || abs(linAccelZ) > 20.0f) {
            DiagnosticManager::logError("SensorManager", "Invalid IMU linear acceleration values: X=" + String(linAccelX, 2) + ", Y=" + String(linAccelY, 2) + ", Z=" + String(linAccelZ, 2));
            _imuDataValid = false;
            return;
        }
        
        // GYROSCOPE - Read angular velocity
        float gyroX = _bno080.getGyroX();
        float gyroY = _bno080.getGyroY();
        float gyroZ = _bno080.getGyroZ();
        
        // Validate gyroscope data (reasonable range: -2000 deg/s)
        if (abs(gyroX) > 2000.0f || abs(gyroY) > 2000.0f || abs(gyroZ) > 2000.0f) {
            DiagnosticManager::logError("SensorManager", "Invalid IMU gyroscope values: X=" + String(gyroX, 2) + ", Y=" + String(gyroY, 2) + ", Z=" + String(gyroZ, 2));
            _imuDataValid = false;
            return;
        }
        
        // ACCURACY-BASED DATA VALIDATION
        // Only use data if minimum accuracy is achieved
        if (quatAccuracy == 0) {
            DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", "IMU quaternion accuracy = 0 (unreliable) - using with caution");
            // Continue but mark as lower confidence rather than rejecting completely
        }
        
        if (accelAccuracy == 0) {
            DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", "IMU accelerometer accuracy unreliable");
            // Continue but mark as lower confidence
        }
        
        // DATA IS VALID - Update stored values
        _imuQuatI = quatI;
        _imuQuatJ = quatJ;
        _imuQuatK = quatK;
        _imuQuatReal = quatReal;
        _imuAccelX = accelX;
        _imuAccelY = accelY;
        _imuAccelZ = accelZ;
        _imuGyroX = gyroX;
        _imuGyroY = gyroY;
        _imuGyroZ = gyroZ;
        
        // Store linear acceleration for motion analysis
        _imuLinAccelX = linAccelX;
        _imuLinAccelY = linAccelY;
        _imuLinAccelZ = linAccelZ;
        
        // Store accuracy levels for external use
        _imuQuatAccuracy = quatAccuracy;
        _imuAccelAccuracy = accelAccuracy;
        _imuGyroAccuracy = gyroAccuracy;
        
        _imuDataValid = true;
        _lastImuUpdateTime = now;
        
        // PERFORMANCE MONITORING - Track data rate (SparkFun SPI example pattern)
        _imuDataCount++;
        if (_imuDataCount % 1000 == 0) { // Every 1000 samples
            float dataRate = (float)_imuDataCount / ((now - _imuStartTime) / 1000.0f);
            DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", 
                "IMU performance: " + String(dataRate, 1) + "Hz data rate, Accuracy: Q=" + String(quatAccuracy) + 
                ", A=" + String(accelAccuracy) + ", G=" + String(gyroAccuracy) + ", L=" + String(linAccelAccuracy));
        }
        
        // DETAILED DEBUG LOGGING (periodic)
        if (_imuDataCount % 5000 == 0) { // Every 5000 samples (~50 seconds at 100Hz)
            DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", 
                "IMU detailed - Quat: [" + String(quatI, 3) + ", " + String(quatJ, 3) + ", " + String(quatK, 3) + ", " + String(quatReal, 3) + 
                "], LinAccel: [" + String(linAccelX, 2) + ", " + String(linAccelY, 2) + ", " + String(linAccelZ, 2) + "]");
        }
        
    } else {
        // ENHANCED ERROR RECOVERY: Check for IMU timeout
        if (millis() - _lastImuUpdateTime > 1000) { // 1 second timeout
            if (_imuDataValid) {
                DiagnosticManager::logError("SensorManager", "IMU communication timeout - no data for 1 second");
                _imuDataValid = false;
            }
        }
    }
}

void SensorManager::updateRadar() {
    // COMPREHENSIVE RADAR UPDATE based on SparkFun XM125 examples
    
    uint32_t errorStatus = 0;
    uint32_t measDistErr = 0;
    uint32_t calibrateNeeded = 0;
    
    // Check detector error status before starting measurement
    _radar.getDetectorErrorStatus(errorStatus);
    if (errorStatus != 0) {
        DiagnosticManager::logError("SensorManager", "Radar detector error status: " + String(errorStatus));
        _radarDataValid = false;
        return;
    }
    
    // Start detector for measurement
    if (_radar.setCommand(SFE_XM125_DISTANCE_START_DETECTOR) != 0) {
        DiagnosticManager::logError("SensorManager", "Radar start detector command failed");
        _radarDataValid = false;
        return;
    }
    
    // Wait for measurement to complete
    if (_radar.busyWait() != 0) {
        DiagnosticManager::logError("SensorManager", "Radar measurement busy wait failed");
        _radarDataValid = false;
        return;
    }
    
    // Check for errors after measurement
    _radar.getDetectorErrorStatus(errorStatus);
    if (errorStatus != 0) {
        DiagnosticManager::logError("SensorManager", "Radar detector error after measurement: " + String(errorStatus));
        _radarDataValid = false;
        return;
    }
    
    // Check for measurement distance error
    _radar.getMeasureDistanceError(measDistErr);
    if (measDistErr == 1) {
        DiagnosticManager::logError("SensorManager", "Radar measurement distance error detected");
        _radarDataValid = false;
        return;
    }
    
    // Check if recalibration is needed
    _radar.getCalibrationNeeded(calibrateNeeded);
    if (calibrateNeeded == 1) {
        DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", "Radar calibration needed - recalibrating");
        
        // Perform recalibration
        if (_radar.setCommand(SFE_XM125_DISTANCE_RECALIBRATE) != 0) {
            DiagnosticManager::logError("SensorManager", "Radar recalibration command failed");
            _radarDataValid = false;
            return;
        }
        
        // Wait for recalibration to complete
        if (_radar.busyWait() != 0) {
            DiagnosticManager::logError("SensorManager", "Radar recalibration busy wait failed");
            _radarDataValid = false;
            return;
        }
        
        DiagnosticManager::logMessage(LOG_INFO, "SensorManager", "Radar recalibration completed successfully");
    }
    
    // MULTI-PEAK DETECTION for ground and crop canopy
    // Read multiple peaks to detect both ground level and crop height
    
    uint32_t peak0Distance = 0, peak1Distance = 0;
    int32_t peak0Strength = 0, peak1Strength = 0;
    
    // Get primary peak (usually ground)
    _radar.getPeak0Distance(peak0Distance);
    _radar.getPeak0Strength(peak0Strength);
    
    // Get secondary peak (usually crop canopy)
    _radar.getPeak1Distance(peak1Distance);
    _radar.getPeak1Strength(peak1Strength);
    
    // SIGNAL STRENGTH ANALYSIS for reliability
    const int32_t MIN_SIGNAL_STRENGTH = 100; // Minimum strength for reliable detection
    
    bool peak0Valid = (peak0Distance > 0) && (peak0Strength > MIN_SIGNAL_STRENGTH);
    bool peak1Valid = (peak1Distance > 0) && (peak1Strength > MIN_SIGNAL_STRENGTH);
    
    if (peak0Valid) {
        // Primary peak detected - convert to meters and validate range
        float distanceMeters = peak0Distance / 1000.0f;
        
        // Validate range for boom height sensing (0.1m to 3.0m)
        if (distanceMeters >= 0.1f && distanceMeters <= 3.0f) {
            _radarDistance = distanceMeters;
            _radarDataValid = true;
            _lastRadarUpdate = millis();
            
            // Log detailed measurement for debugging
            DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", 
                "Radar Peak0: " + String(distanceMeters, 3) + "m, Strength: " + String(peak0Strength));
            
            // If secondary peak is also valid, log it for crop detection analysis
            if (peak1Valid) {
                float peak1Meters = peak1Distance / 1000.0f;
                if (peak1Meters >= 0.1f && peak1Meters <= 3.0f && peak1Meters != distanceMeters) {
                    DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", 
                        "Radar Peak1: " + String(peak1Meters, 3) + "m, Strength: " + String(peak1Strength) + " (crop canopy?)");
                }
            }
            
        } else {
            DiagnosticManager::logError("SensorManager", 
                "Radar distance out of range: " + String(distanceMeters, 3) + "m (expected 0.1-3.0m)");
            _radarDataValid = false;
        }
        
    } else if (peak1Valid) {
        // Only secondary peak valid - use it as backup
        float distanceMeters = peak1Distance / 1000.0f;
        
        if (distanceMeters >= 0.1f && distanceMeters <= 3.0f) {
            _radarDistance = distanceMeters;
            _radarDataValid = true;
            _lastRadarUpdate = millis();
            
            DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", 
                "Radar Peak1 (backup): " + String(distanceMeters, 3) + "m, Strength: " + String(peak1Strength));
        } else {
            DiagnosticManager::logError("SensorManager", 
                "Radar backup distance out of range: " + String(distanceMeters, 3) + "m");
            _radarDataValid = false;
        }
        
    } else {
        // No valid peaks detected
        if (peak0Distance > 0 || peak1Distance > 0) {
            // Peaks detected but signal strength too weak
            DiagnosticManager::logMessage(LOG_WARNING, "SensorManager", 
                "Radar weak signals - Peak0: " + String(peak0Strength) + ", Peak1: " + String(peak1Strength) + " (min: " + String(MIN_SIGNAL_STRENGTH) + ")");
        } else {
            // No targets detected - could be normal (high boom) or error
            DiagnosticManager::logMessage(LOG_DEBUG, "SensorManager", "Radar no targets detected");
        }
        _radarDataValid = false;
    }
    
    // TIMEOUT DETECTION for communication failures
    if (!_radarDataValid && (millis() - _lastRadarUpdate > 5000)) {
        DiagnosticManager::logError("SensorManager", "Radar communication timeout - no valid readings for 5 seconds");
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
