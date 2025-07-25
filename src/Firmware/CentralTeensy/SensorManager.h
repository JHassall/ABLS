#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "DataPackets.h"

class SensorManager {
public:
    SensorManager();
    void init();
    void update(); // Reads all sensors
    void populatePacket(SensorDataPacket* packet);

private:
    // Placeholder for actual sensor objects
    // e.g., Adafruit_BNO055 bno = Adafruit_BNO055(55);
    // e.g., SparkFun_u-blox_GNSS_Arduino myGNSS;

    // Internal storage for sensor data
    SensorDataPacket _currentData;
};

#endif // SENSOR_MANAGER_H
