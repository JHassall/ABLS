/*
 * ABLS: Automatic Boom Levelling System
 * Hydraulic Controller Implementation
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#include "HydraulicController.h"
#include "DiagnosticManager.h"

HydraulicController::HydraulicController() :
    _initialized(false),
    _adcInitialized(false),
    _emergencyStop(false),
    _moduleRole(MODULE_UNKNOWN),
    _isActiveModule(false),
    _ramCenter(RAM_CENTER_ADC_CHANNEL, RAM_CENTER_VALVE_PIN, "Centre"),
    _ramLeft(RAM_LEFT_ADC_CHANNEL, RAM_LEFT_VALVE_PIN, "Left"),
    _ramRight(RAM_RIGHT_ADC_CHANNEL, RAM_RIGHT_VALVE_PIN, "Right"),
    _lastUpdate(0),
    _lastDiagnosticUpdate(0),
    _commandsProcessed(0),
    _safetyViolations(0)
{
}

bool HydraulicController::initialize() {
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", "Initializing hydraulic system...");
    
    // Check if this module should have hydraulic control
    _moduleRole = ModuleConfig::getRole();
    _isActiveModule = (_moduleRole == MODULE_CENTRE);
    
    if (!_isActiveModule) {
        DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
            "Not centre module - hydraulic control disabled");
        _initialized = true; // Mark as initialized but inactive
        return true;
    }
    
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
        "Centre module detected - initializing hydraulic control");
    
    // Initialize ADC
    _adcInitialized = initializeADC();
    if (!_adcInitialized) {
        DiagnosticManager::logError("HydraulicController", "ADC initialization failed");
        return false;
    }
    
    // Initialize pins
    initializePins();
    
    // Set all rams to safe middle position
    setSetpoints(DEFAULT_POSITION_PERCENT, DEFAULT_POSITION_PERCENT, DEFAULT_POSITION_PERCENT);
    
    _initialized = true;
    
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
        "Hydraulic controller initialized successfully");
    
    return true;
}

bool HydraulicController::initializeADC() {
    // Initialize ADS1115 ADC
    if (!_ads.begin()) {
        return false;
    }
    
    // Set gain for 0-5V range (adjust based on your sensor voltage)
    _ads.setGain(GAIN_ONE); // +/- 4.096V range
    
    DiagnosticManager::logMessage(LOG_DEBUG, "HydraulicController", "ADS1115 ADC initialized");
    return true;
}

void HydraulicController::initializePins() {
    // Configure valve control pins as PWM outputs
    pinMode(_ramCenter.valvePin, OUTPUT);
    pinMode(_ramLeft.valvePin, OUTPUT);
    pinMode(_ramRight.valvePin, OUTPUT);
    
    // Set all valves to neutral position (50% PWM)
    analogWrite(_ramCenter.valvePin, 127);
    analogWrite(_ramLeft.valvePin, 127);
    analogWrite(_ramRight.valvePin, 127);
    
    DiagnosticManager::logMessage(LOG_DEBUG, "HydraulicController", "Valve pins initialized");
}

void HydraulicController::update() {
    if (!_initialized || !_isActiveModule) return;
    
    uint32_t now = millis();
    
    // Update at 50Hz (20ms intervals)
    if (now - _lastUpdate < 20) return;
    
    if (_emergencyStop) {
        // In emergency stop, set all valves to neutral
        analogWrite(_ramCenter.valvePin, 127);
        analogWrite(_ramLeft.valvePin, 127);
        analogWrite(_ramRight.valvePin, 127);
        return;
    }
    
    // Update each ram channel
    updateChannel(_ramCenter);
    updateChannel(_ramLeft);
    updateChannel(_ramRight);
    
    _lastUpdate = now;
    
    // Update diagnostics every second
    if (now - _lastDiagnosticUpdate >= 1000) {
        updateDiagnostics();
        _lastDiagnosticUpdate = now;
    }
}

void HydraulicController::updateChannel(RamChannel& channel) {
    if (!channel.enabled) return;
    
    // Read current position
    channel.currentPositionPercent = readChannelPosition(channel);
    
    // Check safety limits
    channel.inSafeRange = isPositionSafe(channel.currentPositionPercent);
    if (!channel.inSafeRange) {
        _safetyViolations++;
        DiagnosticManager::logError("HydraulicController", 
            channel.name + " ram position unsafe: " + String(channel.currentPositionPercent) + "%");
        
        // Stop this channel
        channel.enabled = false;
        analogWrite(channel.valvePin, 127); // Neutral position
        return;
    }
    
    // Calculate time delta for PID
    uint32_t now = millis();
    double dt = (now - channel.lastUpdateTime) / 1000.0; // Convert to seconds
    if (dt <= 0) dt = 0.02; // Default 20ms if first update
    
    // Run PID control
    channel.pidOutput = runPID(channel, dt);
    
    // Apply PID output to valve
    applyPIDOutput(channel, channel.pidOutput);
    
    channel.lastUpdateTime = now;
}

double HydraulicController::runPID(RamChannel& channel, double dt) {
    // Calculate error
    double error = channel.setpointPositionPercent - channel.currentPositionPercent;
    
    // Proportional term
    double proportional = channel.Kp * error;
    
    // Integral term (with windup protection)
    channel.integral += error * dt;
    if (channel.integral > 100.0) channel.integral = 100.0;
    if (channel.integral < -100.0) channel.integral = -100.0;
    double integral = channel.Ki * channel.integral;
    
    // Derivative term
    double derivative = 0.0;
    if (dt > 0) {
        derivative = channel.Kd * (error - channel.previousError) / dt;
    }
    channel.previousError = error;
    
    // Combine PID terms
    double output = proportional + integral + derivative;
    
    // Limit output
    if (output > PID_OUTPUT_MAX) output = PID_OUTPUT_MAX;
    if (output < PID_OUTPUT_MIN) output = PID_OUTPUT_MIN;
    
    return output;
}

void HydraulicController::applyPIDOutput(RamChannel& channel, double pidOutput) {
    // Convert PID output (-255 to +255) to PWM value (0 to 255)
    // 127 = neutral (no movement)
    // 0 = full retract
    // 255 = full extend
    
    int pwmValue = 127 + (int)(pidOutput / 2); // Scale and offset
    
    // Ensure PWM value is within valid range
    if (pwmValue < 0) pwmValue = 0;
    if (pwmValue > 255) pwmValue = 255;
    
    // Apply PWM to valve
    analogWrite(channel.valvePin, pwmValue);
    
    // Log significant movements for debugging
    if (abs(pidOutput) > 50) {
        DiagnosticManager::logMessage(LOG_DEBUG, "HydraulicController", 
            channel.name + " ram: pos=" + String(channel.currentPositionPercent, 1) + 
            "%, target=" + String(channel.setpointPositionPercent, 1) + 
            "%, PID=" + String(pidOutput, 1) + ", PWM=" + String(pwmValue));
    }
}

double HydraulicController::readChannelPosition(RamChannel& channel) {
    // Read ADC value
    channel.rawAdcValue = _ads.readADC_SingleEnded(channel.adcChannel);
    
    // Convert ADC value to percentage (0-100%)
    // Assuming linear relationship between ADC value and position
    // This will need calibration based on actual sensor characteristics
    double percentage = (channel.rawAdcValue / 32767.0) * 100.0;
    
    // Ensure percentage is within valid range
    if (percentage < 0.0) percentage = 0.0;
    if (percentage > 100.0) percentage = 100.0;
    
    return percentage;
}

bool HydraulicController::isPositionSafe(double positionPercent) {
    return (positionPercent >= MIN_POSITION_PERCENT && 
            positionPercent <= MAX_POSITION_PERCENT);
}

void HydraulicController::processCommand(const ControlCommandPacket& command) {
    if (!_initialized || !_isActiveModule) return;
    
    _commandsProcessed++;
    
    // Validate command setpoints
    bool validCommand = true;
    if (!isPositionSafe(command.SetpointCenter)) validCommand = false;
    if (!isPositionSafe(command.SetpointLeft)) validCommand = false;
    if (!isPositionSafe(command.SetpointRight)) validCommand = false;
    
    if (!validCommand) {
        DiagnosticManager::logError("HydraulicController", 
            "Invalid command received - setpoints outside safe range");
        return;
    }
    
    // Apply setpoints
    setSetpoints(command.SetpointCenter, command.SetpointLeft, command.SetpointRight);
    
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
        "Command processed - Centre: " + String(command.SetpointCenter, 1) + 
        "%, Left: " + String(command.SetpointLeft, 1) + 
        "%, Right: " + String(command.SetpointRight, 1) + "%");
}

void HydraulicController::setSetpoints(double centerPercent, double leftPercent, double rightPercent) {
    if (!_initialized || !_isActiveModule) return;
    
    _ramCenter.setpointPositionPercent = centerPercent;
    _ramLeft.setpointPositionPercent = leftPercent;
    _ramRight.setpointPositionPercent = rightPercent;
    
    DiagnosticManager::logMessage(LOG_DEBUG, "HydraulicController", 
        "Setpoints updated - Centre: " + String(centerPercent, 1) + 
        "%, Left: " + String(leftPercent, 1) + 
        "%, Right: " + String(rightPercent, 1) + "%");
}

void HydraulicController::emergencyStop() {
    _emergencyStop = true;
    
    DiagnosticManager::logError("HydraulicController", "EMERGENCY STOP ACTIVATED");
    
    // Immediately set all valves to neutral
    analogWrite(_ramCenter.valvePin, 127);
    analogWrite(_ramLeft.valvePin, 127);
    analogWrite(_ramRight.valvePin, 127);
}

void HydraulicController::resume() {
    if (_emergencyStop) {
        _emergencyStop = false;
        DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", "Emergency stop cleared - resuming operation");
    }
}

void HydraulicController::populateRamPositions(SensorDataPacket* packet) {
    if (!packet || !_isActiveModule) return;
    
    packet->RamPosCenterPercent = _ramCenter.currentPositionPercent;
    packet->RamPosLeftPercent = _ramLeft.currentPositionPercent;
    packet->RamPosRightPercent = _ramRight.currentPositionPercent;
}

bool HydraulicController::isInSafeState() {
    if (!_isActiveModule) return true; // Non-active modules are always "safe"
    
    return (!_emergencyStop && 
            _ramCenter.inSafeRange && 
            _ramLeft.inSafeRange && 
            _ramRight.inSafeRange);
}

String HydraulicController::getStatusString() {
    if (!_isActiveModule) return "Inactive";
    if (!_initialized) return "Not initialized";
    if (_emergencyStop) return "EMERGENCY STOP";
    if (!isInSafeState()) return "UNSAFE";
    
    return "Active";
}

void HydraulicController::enableChannel(int channel, bool enable) {
    switch (channel) {
        case 0: _ramCenter.enabled = enable; break;
        case 1: _ramLeft.enabled = enable; break;
        case 2: _ramRight.enabled = enable; break;
    }
    
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
        "Channel " + String(channel) + " " + (enable ? "enabled" : "disabled"));
}

void HydraulicController::setPIDGains(int channel, double kp, double ki, double kd) {
    RamChannel* ram = nullptr;
    switch (channel) {
        case 0: ram = &_ramCenter; break;
        case 1: ram = &_ramLeft; break;
        case 2: ram = &_ramRight; break;
        default: return;
    }
    
    ram->Kp = kp;
    ram->Ki = ki;
    ram->Kd = kd;
    
    DiagnosticManager::logMessage(LOG_INFO, "HydraulicController", 
        ram->name + " PID gains updated - Kp:" + String(kp, 3) + 
        ", Ki:" + String(ki, 3) + ", Kd:" + String(kd, 3));
}

void HydraulicController::getPIDGains(int channel, double* kp, double* ki, double* kd) {
    if (!kp || !ki || !kd) return;
    
    RamChannel* ram = nullptr;
    switch (channel) {
        case 0: ram = &_ramCenter; break;
        case 1: ram = &_ramLeft; break;
        case 2: ram = &_ramRight; break;
        default: return;
    }
    
    *kp = ram->Kp;
    *ki = ram->Ki;
    *kd = ram->Kd;
}

void HydraulicController::updateDiagnostics() {
    // Log channel status for debugging
    logChannelStatus(_ramCenter);
    logChannelStatus(_ramLeft);
    logChannelStatus(_ramRight);
    
    // Update diagnostic display
    String status = getStatusString();
    if (_isActiveModule) {
        status += " C:" + String(_ramCenter.currentPositionPercent, 0) + 
                  "% L:" + String(_ramLeft.currentPositionPercent, 0) + 
                  "% R:" + String(_ramRight.currentPositionPercent, 0) + "%";
    }
    
    DiagnosticManager::setSystemStatus(status);
}

void HydraulicController::logChannelStatus(const RamChannel& channel) {
    DiagnosticManager::logMessage(LOG_DEBUG, "HydraulicController", 
        channel.name + " - Pos:" + String(channel.currentPositionPercent, 1) + 
        "%, Target:" + String(channel.setpointPositionPercent, 1) + 
        "%, ADC:" + String(channel.rawAdcValue) + 
        ", PID:" + String(channel.pidOutput, 1) + 
        ", Safe:" + (channel.inSafeRange ? "Y" : "N") + 
        ", En:" + (channel.enabled ? "Y" : "N"));
}
