#ifndef HYDRAULIC_CONTROLLER_H
#define HYDRAULIC_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "DataPackets.h"

// A simple structure to hold all data for a single hydraulic ram channel
struct RamChannel {
    const uint8_t adcChannel; // ADC channel on the ADS1115 (0-3)
    const uint8_t valvePin;

    double currentPositionPercent = 0.0;
    double setpointPositionPercent = 50.0; // Default to middle position

    // PID tuning parameters (will need tuning)
    double Kp = 1.0;
    double Ki = 0.1;
    double Kd = 0.01;

    // PID internal variables
    double integral = 0.0;
    double previousError = 0.0;
};

class HydraulicController {
public:
    HydraulicController();
    void init();
    void update(); // Reads ADCs and runs PID loops
    void updateSetpoint(const String& targetId, double value);
    void addRamPositionsToPacket(SensorDataPacket* packet);

private:
    Adafruit_ADS1115 ads;  // The ADC object

    RamChannel _ramCenter;
    RamChannel _ramLeft;
    RamChannel _ramRight;
    
    double readRamPosition(uint8_t channel);
};

#endif // HYDRAULIC_CONTROLLER_H
