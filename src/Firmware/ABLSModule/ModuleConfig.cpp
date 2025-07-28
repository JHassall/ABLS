/*
 * ModuleConfig.cpp
 * 
 * Implementation of hardware-based module role detection for ABLS unified firmware.
 * 
 * Author: ABLS Development Team
 */

#include "ModuleConfig.h"
#include "DiagnosticManager.h"

// Static member initialization
ModuleRole_t ModuleConfig::_moduleRole = MODULE_UNKNOWN;
bool ModuleConfig::_roleDetected = false;

void ModuleConfig::detectRole() {
    Serial.println("Detecting module role via 5-way DIP switch...");
    
    // Configure all DIP switch pins with internal pullup resistors
    for (int i = 0; i < NUM_CONFIG_PINS; i++) {
        pinMode(CONFIG_PINS[i], INPUT_PULLUP);
    }
    
    // Small delay to allow pins to stabilize
    delay(10);
    
    // Read all pin states (LOW = tied to GND = active)
    bool pinStates[NUM_CONFIG_PINS];
    int activePins = 0;
    int activePin = -1;
    
    for (int i = 0; i < NUM_CONFIG_PINS; i++) {
        pinStates[i] = (digitalRead(CONFIG_PINS[i]) == LOW);
        if (pinStates[i]) {
            activePins++;
            activePin = i;
        }
    }
    
    // Debug output - show all pin states
    Serial.print("DIP Switch States: ");
    for (int i = 0; i < NUM_CONFIG_PINS; i++) {
        Serial.print("Pin ");
        Serial.print(CONFIG_PINS[i]);
        Serial.print("=");
        Serial.print(pinStates[i] ? "LOW" : "HIGH");
        if (i < NUM_CONFIG_PINS - 1) Serial.print(", ");
    }
    Serial.println();
    
    // Determine role based on pin states
    if (activePins == 1) {
        // Valid configuration - exactly one pin is active
        _moduleRole = (ModuleRole_t)activePin;
        _roleDetected = true;
        
        Serial.print("✅ Valid Configuration Detected: DIP Position ");
        Serial.print(activePin);
        Serial.print(" (Pin ");
        Serial.print(CONFIG_PINS[activePin]);
        Serial.print(") → Role: ");
        Serial.println(getRoleName());
    } else {
        // Invalid configuration
        _moduleRole = MODULE_UNKNOWN;
        _roleDetected = false;
        
        Serial.print("❌ Invalid Configuration: ");
        Serial.print(activePins);
        Serial.println(" pins active (expected exactly 1)");
        
        handleConfigurationError();
    }
}

ModuleRole_t ModuleConfig::getRole() {
    return _moduleRole;
}

String ModuleConfig::getRoleName() {
    switch (_moduleRole) {
        case MODULE_LEFT:    return "LEFT_WING";
        case MODULE_CENTRE:  return "CENTRE";
        case MODULE_RIGHT:   return "RIGHT_WING";
        case MODULE_SPARE_3: return "SPARE_3";
        case MODULE_SPARE_4: return "SPARE_4";
        default:            return "UNKNOWN";
    }
}

bool ModuleConfig::isRoleDetected() {
    return _roleDetected;
}

bool ModuleConfig::isCentreModule() {
    return _moduleRole == MODULE_CENTRE;
}

bool ModuleConfig::isWingModule() {
    return _moduleRole == MODULE_LEFT || _moduleRole == MODULE_RIGHT;
}

bool ModuleConfig::isLeftWing() {
    return _moduleRole == MODULE_LEFT;
}

bool ModuleConfig::isRightWing() {
    return _moduleRole == MODULE_RIGHT;
}

bool ModuleConfig::isValidConfiguration() {
    return _roleDetected && _moduleRole != MODULE_UNKNOWN;
}

void ModuleConfig::handleConfigurationError() {
    String errorMsg = "Invalid DIP switch configuration detected!";
    
    Serial.println();
    Serial.println("=== CONFIGURATION ERROR ===");
    Serial.println(errorMsg);
    Serial.println();
    printConfigurationInstructions();
    Serial.println();
    Serial.println("System halted - please fix hardware configuration and restart.");
    Serial.println("==============================");
    
    // Log error to SD card and show on OLED
    DiagnosticManager::logError("ModuleConfig", errorMsg);
    DiagnosticManager::showErrorScreen("DIP Switch Config Error - Check wiring");
    
    // Halt execution - configuration must be fixed
    while(1) {
        // Flash built-in LED to indicate error state
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}

void ModuleConfig::printConfigurationInstructions() {
    Serial.println("DIP Switch Configuration Instructions:");
    Serial.println("Exactly ONE pin must be tied to GND:");
    Serial.println();
    Serial.println("| DIP Pos | Pin | Module Role | Features");
    Serial.println("|---------|-----|-------------|----------");
    Serial.println("|    0    |  2  | Left Wing   | Sensor fusion, RTCM RX, Airborne GPS");
    Serial.println("|    1    |  3  | Centre      | Hydraulic control, RTCM TX, Auto GPS");
    Serial.println("|    2    |  4  | Right Wing  | Sensor fusion, RTCM RX, Airborne GPS");
    Serial.println("|    3    |  5  | Spare 3     | Future expansion");
    Serial.println("|    4    |  6  | Spare 4     | Future expansion");
    Serial.println();
    Serial.println("Example: For Centre Module, tie Pin 3 to GND (DIP position 1)");
}
