/*
 * ABLS: Automatic Boom Levelling System
 * Hydraulic Controller
 * 
 * Controls three hydraulic rams (Centre, Left Wing, Right Wing) using:
 * - Adafruit ADS1115 16-bit ADC for position feedback
 * - PID control loops for smooth, accurate positioning
 * - Safety limits and error handling
 * - Only active on Centre module (conditional initialization)
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#ifndef HYDRAULIC_CONTROLLER_H
#define HYDRAULIC_CONTROLLER_H

#include <Arduino.h>
#include "DataPackets.h"
#include "ModuleConfig.h"
#include <Adafruit_ADS1X15.h>

// Hydraulic ram configuration
#define RAM_CENTER_ADC_CHANNEL  0
#define RAM_LEFT_ADC_CHANNEL    1
#define RAM_RIGHT_ADC_CHANNEL   2

#define RAM_CENTER_VALVE_PIN    7
#define RAM_LEFT_VALVE_PIN      8
#define RAM_RIGHT_VALVE_PIN     9

// Safety limits
#define MIN_POSITION_PERCENT    5.0   // Minimum safe position (5%)
#define MAX_POSITION_PERCENT    95.0  // Maximum safe position (95%)
#define DEFAULT_POSITION_PERCENT 50.0 // Default middle position

// PID output limits
#define PID_OUTPUT_MIN          -255
#define PID_OUTPUT_MAX          255

// A structure to hold all data for a single hydraulic ram channel
struct RamChannel {
    const uint8_t adcChannel;     // ADC channel on the ADS1115 (0-3)
    const uint8_t valvePin;       // PWM pin for hydraulic valve
    const String name;            // Human-readable name for logging
    
    // Position data
    double currentPositionPercent = DEFAULT_POSITION_PERCENT;
    double setpointPositionPercent = DEFAULT_POSITION_PERCENT;
    int16_t rawAdcValue = 0;
    
    // PID tuning parameters (will need field tuning)
    double Kp = 2.0;   // Proportional gain
    double Ki = 0.5;   // Integral gain  
    double Kd = 0.1;   // Derivative gain
    
    // PID internal variables
    double integral = 0.0;
    double previousError = 0.0;
    double pidOutput = 0.0;
    
    // Safety and status
    bool enabled = true;
    bool inSafeRange = true;
    uint32_t lastUpdateTime = 0;
    
    // Constructor
    RamChannel(uint8_t adc, uint8_t pin, const String& n) : 
        adcChannel(adc), valvePin(pin), name(n) {}
};

class HydraulicController {
public:
    HydraulicController();
    
    // Initialization and lifecycle
    bool initialize();
    void update();
    bool isInitialized() { return _initialized; }
    
    // Command processing
    void processCommand(const ControlCommandPacket& command);
    void setSetpoints(double centerPercent, double leftPercent, double rightPercent);
    void emergencyStop();
    void resume();
    
    // Data access
    void populateRamPositions(SensorDataPacket* packet);
    
    // Safety and diagnostics
    bool isInSafeState();
    String getStatusString();
    void enableChannel(int channel, bool enable);
    
    // PID tuning (for field calibration)
    void setPIDGains(int channel, double kp, double ki, double kd);
    void getPIDGains(int channel, double* kp, double* ki, double* kd);

private:
    // Initialization state
    bool _initialized;
    bool _adcInitialized;
    bool _emergencyStop;
    
    // Module role check
    ModuleRole_t _moduleRole;
    bool _isActiveModule; // Only centre module has hydraulic control
    
    // Hardware
    Adafruit_ADS1115 _ads;  // 16-bit ADC for position feedback
    
    // Ram channels
    RamChannel _ramCenter;
    RamChannel _ramLeft;
    RamChannel _ramRight;
    
    // Timing
    uint32_t _lastUpdate;
    uint32_t _lastDiagnosticUpdate;
    
    // Statistics
    uint32_t _commandsProcessed;
    uint32_t _safetyViolations;
    
    // Internal methods
    bool initializeADC();
    void initializePins();
    void updateChannel(RamChannel& channel);
    double runPID(RamChannel& channel, double dt);
    void applyPIDOutput(RamChannel& channel, double pidOutput);
    double readChannelPosition(RamChannel& channel);
    bool isPositionSafe(double positionPercent);
    void logChannelStatus(const RamChannel& channel);
    void updateDiagnostics();
};

#endif // HYDRAULIC_CONTROLLER_H
