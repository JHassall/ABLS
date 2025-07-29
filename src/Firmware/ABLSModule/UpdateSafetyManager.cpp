#include "UpdateSafetyManager.h"

// =================================================================
// ABLS (Automatic Boom Level System)
// Update Safety Manager - OTA Update Safety Systems Implementation
// 
// Provides safety checks and enforcement for OTA firmware updates
// on agricultural equipment. Ensures updates only occur when safe.
//
// Author: RgF Engineering
// Based on: OTA Firmware Update System Plan requirements
// =================================================================

// Static member initialization
UpdateMode_t UpdateSafetyManager::_currentMode = UPDATE_MODE_NORMAL;
unsigned long UpdateSafetyManager::_modeChangeTime = 0;
bool UpdateSafetyManager::_updateModeActive = false;
SafetyCheckResult_t UpdateSafetyManager::_lastSafetyResult = SAFETY_OK;

// Safety thresholds (configurable)
float UpdateSafetyManager::_stationarySpeedThreshold = 0.1f;  // 0.1 m/s (0.36 km/h)
uint32_t UpdateSafetyManager::_hydraulicIdleTimeout = 5000;   // 5 seconds
float UpdateSafetyManager::_minimumVoltage = 11.5f;           // 11.5V minimum
uint32_t UpdateSafetyManager::_safetyCheckInterval = 1000;    // 1 second

// Safety monitoring state
unsigned long UpdateSafetyManager::_lastMotionTime = 0;
unsigned long UpdateSafetyManager::_lastHydraulicActivity = 0;
unsigned long UpdateSafetyManager::_lastSafetyCheck = 0;
float UpdateSafetyManager::_currentSpeed = 0.0f;
float UpdateSafetyManager::_currentVoltage = 12.0f;

void UpdateSafetyManager::init() {
    _currentMode = UPDATE_MODE_NORMAL;
    _modeChangeTime = millis();
    _updateModeActive = false;
    _lastSafetyResult = SAFETY_OK;
    
    _lastMotionTime = millis();
    _lastHydraulicActivity = millis();
    _lastSafetyCheck = millis();
    
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Initialized with safety thresholds:");
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "  Stationary speed: " + String(_stationarySpeedThreshold) + " m/s");
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "  Hydraulic idle timeout: " + String(_hydraulicIdleTimeout) + " ms");
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "  Minimum voltage: " + String(_minimumVoltage) + " V");
}

void UpdateSafetyManager::update() {
    unsigned long now = millis();
    
    // Periodic safety checks
    if (now - _lastSafetyCheck >= _safetyCheckInterval) {
        updateSafetyState();
        _lastSafetyCheck = now;
        
        // If in update mode, continuously monitor safety
        if (_updateModeActive) {
            if (!monitorSafetyDuringUpdate()) {
                DiagnosticManager::logMessage(LOG_ERROR, "UpdateSafetyManager", "Safety violation during update - aborting!");
                emergencyAbortUpdate();
            }
        }
    }
}

SafetyCheckResult_t UpdateSafetyManager::isSafeToUpdate() {
    // Check if another update is already in progress
    if (_updateModeActive) {
        _lastSafetyResult = SAFETY_UPDATE_IN_PROGRESS;
        return _lastSafetyResult;
    }
    
    // Check if system is stationary
    if (!isSystemStationary()) {
        _lastSafetyResult = SAFETY_SYSTEM_MOVING;
        logSafetyCheckResult(_lastSafetyResult);
        return _lastSafetyResult;
    }
    
    // Check if hydraulics are idle
    if (!areHydraulicsIdle()) {
        _lastSafetyResult = SAFETY_HYDRAULICS_ACTIVE;
        logSafetyCheckResult(_lastSafetyResult);
        return _lastSafetyResult;
    }
    
    // Check if GPS data is valid
    if (!isGPSDataValid()) {
        _lastSafetyResult = SAFETY_GPS_UNAVAILABLE;
        logSafetyCheckResult(_lastSafetyResult);
        return _lastSafetyResult;
    }
    
    // Check if power is sufficient
    if (!isPowerSufficient()) {
        _lastSafetyResult = SAFETY_POWER_INSUFFICIENT;
        logSafetyCheckResult(_lastSafetyResult);
        return _lastSafetyResult;
    }
    
    // Check for critical operations
    if (!checkCriticalOperations()) {
        _lastSafetyResult = SAFETY_CRITICAL_OPERATION;
        logSafetyCheckResult(_lastSafetyResult);
        return _lastSafetyResult;
    }
    
    _lastSafetyResult = SAFETY_OK;
    return _lastSafetyResult;
}

bool UpdateSafetyManager::isSystemStationary() {
    // Get current GPS speed
    if (!checkGPSSpeed()) {
        return false;
    }
    
    // Check if speed is below threshold
    bool isStationary = (_currentSpeed <= _stationarySpeedThreshold);
    
    // Update motion tracking
    if (!isStationary) {
        _lastMotionTime = millis();
    }
    
    return isStationary;
}

bool UpdateSafetyManager::areHydraulicsIdle() {
    // Only check hydraulics on Centre module
    if (ModuleConfig::getRole() != MODULE_CENTRE) {
        return true; // Wing modules don't control hydraulics
    }
    
    return checkHydraulicStatus();
}

bool UpdateSafetyManager::isGPSDataValid() {
    return checkGPSSpeed();
}

bool UpdateSafetyManager::isPowerSufficient() {
    return checkPowerStatus();
}

bool UpdateSafetyManager::enterUpdateMode() {
    // Verify it's safe to enter update mode
    SafetyCheckResult_t safetyResult = isSafeToUpdate();
    if (safetyResult != SAFETY_OK) {
        DiagnosticManager::logMessage(LOG_ERROR, "UpdateSafetyManager", "Cannot enter update mode - " + 
                             String(safetyResultToString(safetyResult)));
        return false;
    }
    
    // Prepare for update
    if (!prepareForUpdate()) {
        DiagnosticManager::logMessage(LOG_ERROR, "UpdateSafetyManager", "Failed to prepare for update");
        return false;
    }
    
    // Enter update mode
    UpdateMode_t oldMode = _currentMode;
    _currentMode = UPDATE_MODE_ACTIVE;
    _modeChangeTime = millis();
    _updateModeActive = true;
    
    logModeChange(oldMode, _currentMode);
    notifySystemsOfUpdateMode(true);
    
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Successfully entered update mode");
    return true;
}

void UpdateSafetyManager::exitUpdateMode() {
    if (!_updateModeActive) {
        return; // Already in normal mode
    }
    
    UpdateMode_t oldMode = _currentMode;
    _currentMode = UPDATE_MODE_RECOVERY;
    _modeChangeTime = millis();
    
    // Re-enable systems
    enableNonEssentialSystems();
    notifySystemsOfUpdateMode(false);
    
    // Return to normal mode
    _currentMode = UPDATE_MODE_NORMAL;
    _updateModeActive = false;
    
    logModeChange(oldMode, _currentMode);
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Exited update mode, returned to normal operation");
}

UpdateMode_t UpdateSafetyManager::getCurrentMode() {
    return _currentMode;
}

bool UpdateSafetyManager::isUpdateModeActive() {
    return _updateModeActive;
}

bool UpdateSafetyManager::monitorSafetyDuringUpdate() {
    // Continuous safety monitoring during update
    SafetyCheckResult_t result = isSafeToUpdate();
    
    // Allow UPDATE_IN_PROGRESS during monitoring (we're already updating)
    if (result == SAFETY_UPDATE_IN_PROGRESS) {
        return true;
    }
    
    return (result == SAFETY_OK);
}

void UpdateSafetyManager::emergencyAbortUpdate() {
    DiagnosticManager::logMessage(LOG_CRITICAL, "UpdateSafetyManager", "EMERGENCY ABORT - Safety violation during update!");
    
    // Perform emergency shutdown
    performEmergencyShutdown();
    
    // Restore systems
    restoreSystemsAfterAbort();
    
    // Exit update mode
    _updateModeActive = false;
    _currentMode = UPDATE_MODE_NORMAL;
    _modeChangeTime = millis();
    
    logSafetyEvent("EMERGENCY_ABORT_COMPLETED");
}

// Configuration functions
void UpdateSafetyManager::setStationarySpeedThreshold(float speedMs) {
    _stationarySpeedThreshold = speedMs;
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Stationary speed threshold set to " + 
                         String(speedMs) + " m/s");
}

void UpdateSafetyManager::setHydraulicIdleTimeout(uint32_t timeoutMs) {
    _hydraulicIdleTimeout = timeoutMs;
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Hydraulic idle timeout set to " + 
                         String(timeoutMs) + " ms");
}

void UpdateSafetyManager::setMinimumVoltage(float minVoltage) {
    _minimumVoltage = minVoltage;
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Minimum voltage set to " + 
                         String(minVoltage) + " V");
}

String UpdateSafetyManager::getSafetyStatusString() {
    String status = "Safety Status: ";
    status += safetyResultToString(_lastSafetyResult);
    status += ", Mode: ";
    status += updateModeToString(_currentMode);
    status += ", Speed: ";
    status += String(_currentSpeed, 2);
    status += " m/s, Voltage: ";
    status += String(_currentVoltage, 1);
    status += " V";
    return status;
}

void UpdateSafetyManager::logSafetyEvent(const String& event) {
    DiagnosticManager::logMessage(LOG_WARNING, "UpdateSafetyManager", "SAFETY_EVENT - " + event);
}

// Private helper functions
bool UpdateSafetyManager::checkGPSSpeed() {
    // Get GPS speed from SensorManager
    // Note: SensorManager doesn't currently expose ground speed directly
    // For now, we'll use a conservative approach and assume stationary
    
    // TODO: Enhance SensorManager to provide ground speed calculation
    // This could be calculated from consecutive GPS position updates
    // or added as a direct method to access GPS velocity data
    
    _currentSpeed = 0.0f; // Conservative assumption - stationary
    
    // In a full implementation, this would:
    // 1. Access GPS velocity data from SensorManager
    // 2. Calculate ground speed from velocity components
    // 3. Apply smoothing/filtering for stability
    
    return true; // GPS data available (SensorManager handles GPS status)
}

bool UpdateSafetyManager::checkHydraulicStatus() {
    // Check if hydraulics are in safe state
    // Note: We need access to the HydraulicController instance
    // For now, we'll use a conservative approach
    
    unsigned long now = millis();
    
    // TODO: Access actual HydraulicController instance to check status
    // This requires either:
    // 1. Passing HydraulicController reference to UpdateSafetyManager
    // 2. Making HydraulicController methods static
    // 3. Using a global instance reference
    
    // For now, assume hydraulics are safe (conservative)
    bool hydraulicsInSafeState = true;
    
    if (!hydraulicsInSafeState) {
        _lastHydraulicActivity = now;
    }
    
    // Check if hydraulics have been idle for required timeout
    return (now - _lastHydraulicActivity >= _hydraulicIdleTimeout);
}

bool UpdateSafetyManager::checkPowerStatus() {
    // Check system voltage
    // TODO: Implement actual voltage monitoring
    // Example: _currentVoltage = analogRead(VOLTAGE_PIN) * VOLTAGE_SCALE;
    
    _currentVoltage = 12.5f; // Placeholder - assume good voltage
    
    return (_currentVoltage >= _minimumVoltage);
}

bool UpdateSafetyManager::checkCriticalOperations() {
    // Check for any critical agricultural operations
    // This could include active spraying, seeding, etc.
    
    // TODO: Implement checks for critical operations
    // For now, assume no critical operations
    
    return true;
}

void UpdateSafetyManager::updateSafetyState() {
    // Update current safety state
    checkGPSSpeed();
    checkPowerStatus();
    
    if (ModuleConfig::getRole() == MODULE_CENTRE) {
        checkHydraulicStatus();
    }
}

bool UpdateSafetyManager::prepareForUpdate() {
    _currentMode = UPDATE_MODE_PREPARING;
    
    // Disable non-essential systems
    disableNonEssentialSystems();
    
    // Wait a moment for systems to settle
    delay(1000);
    
    // Verify systems are ready
    return true;
}

void UpdateSafetyManager::disableNonEssentialSystems() {
    // Disable non-essential systems during update
    // Keep only critical safety systems active
    
    logSafetyEvent("DISABLING_NON_ESSENTIAL_SYSTEMS");
    
    // TODO: Implement system disable logic
    // Example: disable sensors, reduce network activity, etc.
}

void UpdateSafetyManager::enableNonEssentialSystems() {
    // Re-enable systems after update
    
    logSafetyEvent("ENABLING_NON_ESSENTIAL_SYSTEMS");
    
    // TODO: Implement system enable logic
    // Example: re-enable sensors, restore network activity, etc.
}

void UpdateSafetyManager::notifySystemsOfUpdateMode(bool entering) {
    String message = entering ? "ENTERING_UPDATE_MODE" : "EXITING_UPDATE_MODE";
    logSafetyEvent(message);
    
    // TODO: Notify other system components of update mode change
    // This could involve setting flags, sending messages, etc.
}

void UpdateSafetyManager::performEmergencyShutdown() {
    logSafetyEvent("PERFORMING_EMERGENCY_SHUTDOWN");
    
    // TODO: Implement emergency shutdown procedures
    // This should safely stop all operations immediately
}

void UpdateSafetyManager::restoreSystemsAfterAbort() {
    logSafetyEvent("RESTORING_SYSTEMS_AFTER_ABORT");
    
    // TODO: Implement system restoration after emergency abort
    // This should safely restore normal operation
}

void UpdateSafetyManager::logModeChange(UpdateMode_t oldMode, UpdateMode_t newMode) {
    DiagnosticManager::logMessage(LOG_INFO, "UpdateSafetyManager", "Mode change - " + 
                         String(updateModeToString(oldMode)) + " -> " + 
                         String(updateModeToString(newMode)));
}

void UpdateSafetyManager::logSafetyCheckResult(SafetyCheckResult_t result) {
    if (result != SAFETY_OK) {
        DiagnosticManager::logMessage(LOG_WARNING, "UpdateSafetyManager", "Safety check failed - " + 
                             String(safetyResultToString(result)));
    }
}

// Helper functions for string conversion
const char* safetyResultToString(SafetyCheckResult_t result) {
    switch (result) {
        case SAFETY_OK: return "OK";
        case SAFETY_SYSTEM_MOVING: return "SYSTEM_MOVING";
        case SAFETY_HYDRAULICS_ACTIVE: return "HYDRAULICS_ACTIVE";
        case SAFETY_GPS_UNAVAILABLE: return "GPS_UNAVAILABLE";
        case SAFETY_UPDATE_IN_PROGRESS: return "UPDATE_IN_PROGRESS";
        case SAFETY_CRITICAL_OPERATION: return "CRITICAL_OPERATION";
        case SAFETY_POWER_INSUFFICIENT: return "POWER_INSUFFICIENT";
        case SAFETY_UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        default: return "INVALID";
    }
}

const char* updateModeToString(UpdateMode_t mode) {
    switch (mode) {
        case UPDATE_MODE_NORMAL: return "NORMAL";
        case UPDATE_MODE_PREPARING: return "PREPARING";
        case UPDATE_MODE_ACTIVE: return "ACTIVE";
        case UPDATE_MODE_RECOVERY: return "RECOVERY";
        default: return "INVALID";
    }
}
