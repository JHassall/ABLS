#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <SparkFun_BNO080_Arduino_Library.h>
#include "DataPackets.h"

// A structure to hold the fused state of the Teensy
struct FusedState {
    // Position (updated by GPS and IMU)
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;

    // Velocity (updated by GPS)
    float speed = 0.0;   // in m/s
    float heading = 0.0; // in degrees

    // Orientation (updated by IMU)
    float roll = 0.0;
    float pitch = 0.0;
    float yaw = 0.0;

    // GPS metadata
    int satellites = 0;
};

class SensorManager {
public:
    SensorManager();
    void init();
    void update(); // This will be called frequently to check for new sensor data

    // Public methods to be called when new data arrives from specific sensors
    void onGpsUpdate(/* GPS data packet from your GPS library */);
    void onImuUpdate();

    void populatePacket(OuterSensorDataPacket* packet);

private:
    BNO080 _bno080; // The BNO080 object
    // Placeholder for your GPS object, e.g., SparkFun_u-blox_GNSS_Arduino myGNSS;

    FusedState _currentState;

    // Timing for dead reckoning prediction
    unsigned long _lastImuPredictionTime = 0;

    void predictNewPosition();
};

#endif // SENSOR_MANAGER_H
