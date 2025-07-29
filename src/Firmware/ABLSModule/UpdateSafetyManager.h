#ifndef UPDATE_SAFETY_MANAGER_H
#define UPDATE_SAFETY_MANAGER_H

// =================================================================
// ABLS (Automatic Boom Level System)
// Update Safety Manager - OTA Update Safety Systems
// 
// Provides safety checks and enforcement for OTA firmware updates
// on agricultural equipment. Ensures updates only occur when safe.
//
// Author: RgF Engineering
// Based on: OTA Firmware Update System Plan requirements
// =================================================================

#include <Arduino.h>
#include "ModuleConfig.h"
#include "SensorManager.h"
#include "HydraulicController.h"
#include "DiagnosticManager.h"

// Safety check result codes
typedef enum {
    SAFETY_OK = 0,                    // Safe to proceed with update
    SAFETY_SYSTEM_MOVING,             // Machine is not stationary
    SAFETY_HYDRAULICS_ACTIVE,         // Hydraulic operations in progress
    SAFETY_GPS_UNAVAILABLE,           // GPS data not available for speed check
    SAFETY_UPDATE_IN_PROGRESS,        // Another update already active
    SAFETY_CRITICAL_OPERATION,        // Critical agricultural operation active
    SAFETY_POWER_INSUFFICIENT,        // Insufficient power for safe update
    SAFETY_UNKNOWN_ERROR              // Unknown safety issue
} SafetyCheckResult_t;

// Update mode states
typedef enum {
    UPDATE_MODE_NORMAL = 0,           // Normal operation mode
    UPDATE_MODE_PREPARING,            // Preparing for update (disabling systems)
    UPDATE_MODE_ACTIVE,               // Update in progress
    UPDATE_MODE_RECOVERY              // Recovering from update (re-enabling systems)
} UpdateMode_t;

class UpdateSafetyManager {
public:
    // Initialization and configuration
    static void init();
    static void update();
    
    // Primary safety check functions
    static SafetyCheckResult_t isSafeToUpdate();
    static bool isSystemStationary();
    static bool areHydraulicsIdle();
    static bool isGPSDataValid();
    static bool isPowerSufficient();
    
    // Update mode management
    static bool enterUpdateMode();
    static void exitUpdateMode();
    static UpdateMode_t getCurrentMode();
    static bool isUpdateModeActive();
    
    // Safety monitoring during updates
    static bool monitorSafetyDuringUpdate();
    static void emergencyAbortUpdate();
    
    // Configuration and thresholds
    static void setStationarySpeedThreshold(float speedMs);
    static void setHydraulicIdleTimeout(uint32_t timeoutMs);
    static void setMinimumVoltage(float minVoltage);
    
    // Status and diagnostics
    static String getSafetyStatusString();
    static void logSafetyEvent(const String& event);
    
private:
    // Safety state tracking
    static UpdateMode_t _currentMode;
    static unsigned long _modeChangeTime;
    static bool _updateModeActive;
    static SafetyCheckResult_t _lastSafetyResult;
    
    // Safety thresholds and configuration
    static float _stationarySpeedThreshold;      // m/s - speed below which system is "stationary"
    static uint32_t _hydraulicIdleTimeout;       // ms - time hydraulics must be idle
    static float _minimumVoltage;                // V - minimum voltage for safe update
    static uint32_t _safetyCheckInterval;        // ms - how often to check safety during update
    
    // Safety monitoring state
    static unsigned long _lastMotionTime;        // Last time system was moving
    static unsigned long _lastHydraulicActivity; // Last hydraulic activity
    static unsigned long _lastSafetyCheck;       // Last safety check timestamp
    static float _currentSpeed;                  // Current GPS speed (m/s)
    static float _currentVoltage;                // Current system voltage
    
    // Internal safety check functions
    static bool checkGPSSpeed();
    static bool checkHydraulicStatus();
    static bool checkPowerStatus();
    static bool checkCriticalOperations();
    static void updateSafetyState();
    
    // Mode transition helpers
    static bool prepareForUpdate();
    static void disableNonEssentialSystems();
    static void enableNonEssentialSystems();
    static void notifySystemsOfUpdateMode(bool entering);
    
    // Emergency procedures
    static void performEmergencyShutdown();
    static void restoreSystemsAfterAbort();
    
    // Logging and diagnostics
    static void logModeChange(UpdateMode_t oldMode, UpdateMode_t newMode);
    static void logSafetyCheckResult(SafetyCheckResult_t result);
};

// Safety check result helper functions
const char* safetyResultToString(SafetyCheckResult_t result);
const char* updateModeToString(UpdateMode_t mode);

#endif // UPDATE_SAFETY_MANAGER_H
