#include "HydraulicController.h"

// --- Pin/Channel Definitions ---
// Define which ADC channel on the ADS1115 corresponds to which ram
#define ADC_CHANNEL_CENTER   0
#define ADC_CHANNEL_LEFT     1
#define ADC_CHANNEL_RIGHT    2

// Define which PWM pin on the Teensy controls which valve
#define VALVE_PIN_CENTER 2
#define VALVE_PIN_LEFT   3
#define VALVE_PIN_RIGHT  4

// --- Constructor ---
HydraulicController::HydraulicController() :
    _ramCenter{ADC_CHANNEL_CENTER, VALVE_PIN_CENTER},
    _ramLeft{ADC_CHANNEL_LEFT, VALVE_PIN_LEFT},
    _ramRight{ADC_CHANNEL_RIGHT, VALVE_PIN_RIGHT}
{
    // The member initializer list above is used to set the const pins/channels
}

// --- Initialization ---
void HydraulicController::init() {
    Serial.println("Initializing Hydraulic Controller with ADS1115...");
    
    // Initialize the ADS1115
    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS1115. Check wiring.");
        while (1); // Halt execution
    }
    
    // Set the gain (if needed). GAIN_ONE gives a range of +/- 4.096V.
    // This is a good default if your pots output 0-3.3V or 0-5V.
    ads.setGain(GAIN_ONE);

    // Set pin modes for valve outputs
    pinMode(_ramCenter.valvePin, OUTPUT);
    pinMode(_ramLeft.valvePin, OUTPUT);
    pinMode(_ramRight.valvePin, OUTPUT);
    
    Serial.println("Hydraulic Controller Initialized.");
}

// --- Main Update Loop ---
void HydraulicController::update() {
    // Read current positions from the ADS1115
    _ramCenter.currentPositionPercent = readRamPosition(_ramCenter.adcChannel);
    _ramLeft.currentPositionPercent   = readRamPosition(_ramLeft.adcChannel);
    _ramRight.currentPositionPercent  = readRamPosition(_ramRight.adcChannel);

    // PID Calculation for each ram
    auto calculate_pid = [](RamChannel& ram) {
        double error = ram.setpointPositionPercent - ram.currentPositionPercent;
        ram.integral += error;
        ram.integral = constrain(ram.integral, -100.0, 100.0); // Basic anti-windup
        double derivative = error - ram.previousError;
        double output = (ram.Kp * error) + (ram.Ki * ram.integral) + (ram.Kd * derivative);
        ram.previousError = error;
        analogWrite(ram.valvePin, constrain(output, 0, 255));
    };

    calculate_pid(_ramCenter);
    calculate_pid(_ramLeft);
    calculate_pid(_ramRight);
}

// --- Update ram setpoints from a network command packet ---
void HydraulicController::setSetpoints(const ControlCommandPacket& command) {
    if (command.Command != "setpoint") return; // Only process setpoint commands

    double value = constrain(command.Value, 0.0, 100.0);

    if (command.TargetId == "ram_center") {
        _ramCenter.setpointPositionPercent = value;
    } else if (command.TargetId == "ram_left") {
        _ramLeft.setpointPositionPercent = value;
    } else if (command.TargetId == "ram_right") {
        _ramRight.setpointPositionPercent = value;
    }
}

// --- Helper to read ADS1115 channel and convert to percentage ---
double HydraulicController::readRamPosition(uint8_t channel) {
    int16_t rawValue = ads.readADC_SingleEnded(channel);
    // Map the 16-bit ADC value to a percentage (0.0-100.0)
    // The max value for single-ended reads is 32767
    return (rawValue / 32767.0) * 100.0;
}

// --- Helper to populate the outgoing sensor packet ---
void HydraulicController::addRamPositionsToPacket(SensorDataPacket* packet) {
    packet->RamPosCenterPercent = _ramCenter.currentPositionPercent;
    packet->RamPosLeftPercent = _ramLeft.currentPositionPercent;
    packet->RamPosRightPercent = _ramRight.currentPositionPercent;
}
