/*
 * ABLS: Automatic Boom Levelling System
 * Unified Sensor Manager
 * 
 * Combines the best features from BoomCentreModule and BoomWingModule:
 * - Callback-based GPS processing with high-precision data
 * - RTK quality monitoring and status reporting
 * - Dead reckoning sensor fusion (wing modules only)
 * - IMU orientation and motion sensing
 * - Radar distance measurement
 * - Conditional feature activation based on module role
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"
#include "ModuleConfig.h"
#include <SparkFun_BNO080_Arduino_Library.h>
#include <SparkFun_Qwiic_XM125_Arduino_Library.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// RTK Status Enumeration for GPS Quality Assessment
typedef enum {
    RTK_NONE = 0,     // Standard GPS (>50cm accuracy)
    RTK_FLOAT = 1,    // RTK Float solution (2cm-50cm accuracy)  
    RTK_FIXED = 2     // RTK Fixed solution (<2cm accuracy)
} RTKStatus_t;

// GPS Dynamic Models for different module roles
typedef enum {
    GPS_MODEL_AUTOMOTIVE = 4,   // For centre module (tractor chassis)
    GPS_MODEL_AIRBORNE1G = 6    // For wing modules (boom tips)
} GPSDynamicModel_t;

class SensorManager {
public:
    SensorManager();
    
    // Initialization and lifecycle
    bool initialize();
    void update();
    bool isInitialized() { return _initialized; }
    
    // GPS RTCM correction handling
    void forwardRtcmToGps(const uint8_t* data, size_t len);
    
    // Data access
    void populatePacket(SensorDataPacket* packet);
    RTKStatus_t getRTKStatus() { return _rtkStatus; }
    float getHorizontalAccuracy() { return _horizontalAccuracy; }
    
    // GPS Callback function (must be static for callback registration)
    static void gpsHPPOSLLHCallback(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct);
    
    // Diagnostic information
    String getGPSStatusString();
    String getIMUStatusString();
    String getRadarStatusString();

private:
    // Initialization state
    bool _initialized;
    bool _gpsInitialized;
    bool _imuInitialized;
    bool _radarInitialized;
    
    // Module role-specific configuration
    ModuleRole_t _moduleRole;
    GPSDynamicModel_t _gpsDynamicModel;
    bool _enableDeadReckoning;  // Only for wing modules
    
    // GPS Callback State
    static SensorManager* _instance; // Static instance pointer for callback access
    bool _freshGpsData; // Flag indicating new GPS data from callback
    
    // GPS High-Precision Data (from callback)
    double _gpsLatitude, _gpsLongitude;
    int32_t _gpsAltitude; // mm above ellipsoid
    uint32_t _gpsHorizontalAccuracy; // mm * 0.1
    uint32_t _gpsVerticalAccuracy;   // mm * 0.1
    uint32_t _gpsTimeOfWeek; // ms
    bool _gpsValidFix;
    
    // RTK Quality Monitoring
    RTKStatus_t _rtkStatus; // Current RTK status
    float _horizontalAccuracy; // Current horizontal accuracy in meters
    bool _rtkStatusChanged; // Flag indicating RTK status change
    uint32_t _lastRTKStatusChange;
    
    // Dead Reckoning / Sensor Fusion State (Wing modules only)
    float _fusedLatitude, _fusedLongitude, _fusedAltitude;
    float _velocityX, _velocityY, _velocityZ; // m/s North, East, Down
    uint32_t _lastGpsUpdateTime;
    uint32_t _lastImuUpdateTime;
    uint32_t _lastFusionUpdate;
    
    // IMU Data
    float _imuQuatI, _imuQuatJ, _imuQuatK, _imuQuatReal;
    float _imuAccelX, _imuAccelY, _imuAccelZ;
    float _imuGyroX, _imuGyroY, _imuGyroZ;
    bool _imuDataValid;
    
    // Radar Data
    float _radarDistance; // meters
    bool _radarDataValid;
    uint32_t _lastRadarUpdate;
    
    // Sensor objects
    BNO080 _bno080;
    SparkFunXM125Distance _radar;
    SFE_UBLOX_GNSS_SERIAL _gps;
    
    // Internal methods
    bool initializeGPS();
    bool initializeIMU();
    bool initializeRadar();
    void updateGPS();
    void updateIMU();
    void updateRadar();
    void updateDeadReckoning(); // Wing modules only
    void updateRTKStatus();
    void configureGPSForRole();
    RTKStatus_t determineRTKStatus(uint32_t horizontalAccuracy);
    void logSensorStatus(const String& sensor, bool success);
};

#endif // SENSOR_MANAGER_H
