#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"
#include <SparkFun_BNO080_Arduino_Library.h>
#include <SparkFun_Qwiic_XM125_Arduino_Library.h>
#include <SparkFun_u-blox_GNSS_v3.h>

class SensorManager {
public:
    SensorManager();
    void init();
    void update(); // Reads all attached sensors
    void forwardRtcmToGps(const uint8_t* data, size_t len);
    void populatePacket(SensorDataPacket* packet); // Populates the packet with the latest data

private:
    // Sensor Fusion / Dead Reckoning State
    float _fusedLatitude, _fusedLongitude, _fusedAltitude;
    float _velocityX, _velocityY, _velocityZ; // In a suitable frame, e.g., m/s North, East, Down
    unsigned long _lastGpsUpdateTime;
    unsigned long _lastImuUpdateTime;

    void updateFusion(); // The core dead-reckoning algorithm

    // Sensor objects
    BNO080 _bno080;
    SparkFunXM125Distance _radar;
    SFE_UBLOX_GNSS_SERIAL _gps;

    // Sensor data is read directly into the packet in populatePacket().
};

#endif // SENSOR_MANAGER_H
