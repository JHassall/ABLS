#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"
#include <SparkFun_BNO080_Arduino_Library.h>
#include <SparkFun_Qwiic_XM125_Arduino_Library.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// RTK Status Enumeration for GPS Quality Assessment
typedef enum {
    RTK_NONE = 0,     // Standard GPS (>50cm accuracy)
    RTK_FLOAT = 1,    // RTK Float solution (2cm-50cm accuracy)
    RTK_FIXED = 2     // RTK Fixed solution (<2cm accuracy)
} RTKStatus_t;

class SensorManager {
public:
    SensorManager();
    void init();
    void update(); // Reads all attached sensors
    void forwardRtcmToGps(const uint8_t* data, size_t len);
    void populatePacket(SensorDataPacket* packet); // Populates the packet with the latest data

    // GPS Callback function (must be static for callback registration)
    static void gpsHPPOSLLHCallback(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct);

private:
    // Sensor Fusion / Dead Reckoning State
    float _fusedLatitude, _fusedLongitude, _fusedAltitude;
    float _velocityX, _velocityY, _velocityZ; // In a suitable frame, e.g., m/s North, East, Down
    unsigned long _lastGpsUpdateTime;
    unsigned long _lastImuUpdateTime;
    
    // GPS Callback State
    static SensorManager* _instance; // Static instance pointer for callback access
    bool _freshGpsData; // Flag indicating new GPS data from callback
    
    // RTK Quality Monitoring
    RTKStatus_t _rtkStatus; // Current RTK status
    float _horizontalAccuracy; // Current horizontal accuracy in meters
    bool _rtkStatusChanged; // Flag indicating RTK status change

    void updateFusion(); // The core dead-reckoning algorithm

    // Sensor objects
    BNO080 _bno080;
    SparkFunXM125Distance _radar;
    SFE_UBLOX_GNSS_SERIAL _gps;

    // Sensor data is read directly into the packet in populatePacket().
};

#endif // SENSOR_MANAGER_H
