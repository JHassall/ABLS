#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"
#include <SparkFun_BNO080_Arduino_Library.h>
#include <SparkFun_XM125_Distance.h>
#include <SparkFun_u-blox_GNSS_v3.h>

class SensorManager {
public:
    SensorManager();
    void init();
    void update();
    void populatePacket(CentralSensorDataPacket* packet);

private:
    BNO080 _bno080;
    SparkFunXM125Distance _radar;
    SFE_UBLOX_GNSS_SERIAL _gps;

    // Sensor data is read directly into the packet in populatePacket().
};

#endif // SENSOR_MANAGER_H
