/*
 * ModuleConfig.h
 * 
 * Hardware-based module role detection for ABLS unified firmware.
 * Uses GPIO pins with INPUT_PULLUP to detect which module role this hardware
 * should assume based on installer configuration.
 * 
 * Installation Configuration:
 * - Pin 2 tied to GND = Left Wing Module
 * - Pin 3 tied to GND = Centre Module  
 * - Pin 4 tied to GND = Right Wing Module
 * 
 * Author: ABLS Development Team
 */

#ifndef MODULE_CONFIG_H
#define MODULE_CONFIG_H

#include <Arduino.h>

// Hardware configuration detection pins (5-way DIP switch)
#define PIN_CONFIG_0        2   // DIP switch position 0
#define PIN_CONFIG_1        3   // DIP switch position 1
#define PIN_CONFIG_2        4   // DIP switch position 2
#define PIN_CONFIG_3        5   // DIP switch position 3
#define PIN_CONFIG_4        6   // DIP switch position 4

// Number of configuration pins
#define NUM_CONFIG_PINS     5

// Configuration pin array for easy iteration
const int CONFIG_PINS[NUM_CONFIG_PINS] = {PIN_CONFIG_0, PIN_CONFIG_1, PIN_CONFIG_2, PIN_CONFIG_3, PIN_CONFIG_4};

typedef enum {
    MODULE_LEFT = 0,      // Left wing module (DIP position 0)
    MODULE_CENTRE = 1,    // Centre module (DIP position 1) - hydraulic control
    MODULE_RIGHT = 2,     // Right wing module (DIP position 2)
    MODULE_SPARE_3 = 3,   // Future expansion (DIP position 3)
    MODULE_SPARE_4 = 4,   // Future expansion (DIP position 4)
    MODULE_UNKNOWN = 255  // Error condition - no valid config detected
} ModuleRole_t;

class ModuleConfig {
public:
    // Primary interface methods
    static void detectRole();
    static ModuleRole_t getRole();
    static String getRoleName();
    static bool isRoleDetected();
    
    // Convenience methods for role checking
    static bool isCentreModule();
    static bool isWingModule();
    static bool isLeftWing();
    static bool isRightWing();
    static bool isValidConfiguration();
    
private:
    // Static member variables
    static ModuleRole_t _moduleRole;
    static bool _roleDetected;
    
    // Internal methods
    static void handleConfigurationError();
    static void printConfigurationInstructions();
};

#endif // MODULE_CONFIG_H
