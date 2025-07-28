/*
 * ABLS: Automatic Boom Levelling System
 * Unified Module Firmware
 * 
 * This unified firmware runs on all three ABLS modules:
 * - Left Wing Module (sensor fusion + RTCM receiving)
 * - Centre Module (hydraulic control + RTCM broadcasting)
 * - Right Wing Module (sensor fusion + RTCM receiving)
 * 
 * Module role is detected at startup via hardware GPIO pins:
 * - Pin 2 tied to GND = Left Wing Module
 * - Pin 3 tied to GND = Centre Module
 * - Pin 4 tied to GND = Right Wing Module
 * - Pin 5 Reserved for Future Module
 * - Pin 6 Reserved for Future Module
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#include "ModuleConfig.h"
#include "DataPackets.h"
#include "DiagnosticManager.h"
#include "SensorManager.h"
#include "NetworkManager.h"
#include "HydraulicController.h"
#include "VersionManager.h"
#include "OTAUpdateManager.h"

// Global component instances
SensorManager sensorManager;
NetworkManager networkManager;
HydraulicController hydraulicController;
OTAUpdateManager otaUpdateManager;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Wait for serial or timeout
    
    // Initialize built-in LED for error indication
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    Serial.println("=== ABLS Unified Module Starting ===");
    
    // Step 1: Initialize diagnostic system (OLED + SD logging)
    DiagnosticManager::initialize();
    
    // Step 2: Detect hardware role
    ModuleConfig::detectRole();
    
    // Log role detection results
    DiagnosticManager::logRoleDetection(ModuleConfig::getRole(), ModuleConfig::isRoleDetected());
    
    if (!ModuleConfig::isValidConfiguration()) {
        // Error handling already done in detectRole()
        return;
    }
    
    Serial.print("Module Role: ");
    Serial.println(ModuleConfig::getRoleName());
    
    // Step 3: Initialize sensors (all modules)
    Serial.println("Initializing sensors...");
    if (!sensorManager.initialize()) {
        DiagnosticManager::logError("Setup", "Sensor initialization failed");
        DiagnosticManager::showErrorScreen("Sensor Init Failed");
        while(1) { delay(1000); } // Halt on sensor failure
    }
    
    // Step 4: Initialize network (all modules)
    Serial.println("Initializing network...");
    if (!networkManager.initialize()) {
        DiagnosticManager::logError("Setup", "Network initialization failed");
        DiagnosticManager::showErrorScreen("Network Init Failed");
        while(1) { delay(1000); } // Halt on network failure
    }
    
    // Step 5: Initialize hydraulic controller (centre module only)
    Serial.println("Initializing hydraulic controller...");
    if (!hydraulicController.initialize()) {
        DiagnosticManager::logError("Setup", "Hydraulic controller initialization failed");
        DiagnosticManager::showErrorScreen("Hydraulic Init Failed");
        while(1) { delay(1000); } // Halt on hydraulic failure
    }
    
    // Step 6: Initialize Firmware Update system
    Serial.println("Initializing OTA update system...");
    if (!OTAUpdateManager::initialize()) {
        DiagnosticManager::logError("Setup", "Firmware Updater initialization failed");
        DiagnosticManager::showErrorScreen("Updater Init Failed");
        while(1) { delay(1000); } // Halt on OTA failure
    }
    
    // Step 7: Connect components together
    networkManager.setSensorManager(&sensorManager);
    networkManager.setHydraulicController(&hydraulicController);
    
    // Step 8: Final status update
    DiagnosticManager::setSystemStatus("All systems initialized - Ready");
    DiagnosticManager::logMessage(LOG_INFO, "Setup", "All systems initialized successfully");
    
    Serial.println("=== Setup Complete - System Ready ===");
    Serial.println("Module initialization complete");
    Serial.println("=====================================");
}

void loop() {
    // Update diagnostic display and logging
    DiagnosticManager::updateDisplay();
    
    // Update all system components
    sensorManager.update();
    networkManager.update();
    hydraulicController.update();
    OTAUpdateManager::update();
    
    // Send sensor data to Toughbook at 50Hz (every 20ms)
    static uint32_t lastSensorDataSent = 0;
    if (millis() - lastSensorDataSent >= 20) {
        SensorDataPacket sensorPacket;
        sensorManager.populatePacket(&sensorPacket);
        hydraulicController.populateRamPositions(&sensorPacket);
        networkManager.sendSensorData(sensorPacket);
        lastSensorDataSent = millis();
    }
    
    // Update diagnostic information every 2 seconds
    static uint32_t lastDiagnosticUpdate = 0;
    if (millis() - lastDiagnosticUpdate >= 2000) {
        // Update sensor data display
        DiagnosticManager::setSensorData(
            sensorManager.getGPSStatusString(),
            sensorManager.getIMUStatusString(),
            sensorManager.getRadarStatusString()
        );
        
        // Update system status
        String systemStatus = "Running - ";
        systemStatus += ModuleConfig::getRoleName();
        if (ModuleConfig::getRole() == MODULE_CENTRE) {
            systemStatus += " - " + hydraulicController.getStatusString();
        }
        DiagnosticManager::setSystemStatus(systemStatus);
        
        lastDiagnosticUpdate = millis();
    }
    
    // Heartbeat logging every 10 seconds
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 10000) {
        uint32_t uptime = millis() / 1000;
        
        Serial.print("[HEARTBEAT] ");
        Serial.print(ModuleConfig::getRoleName());
        Serial.print(" - Uptime: ");
        Serial.print(uptime);
        Serial.print("s, Network: ");
        Serial.print(networkManager.getPacketsSent());
        Serial.print(" TX, ");
        Serial.print(networkManager.getPacketsReceived());
        Serial.print(" RX");
        
        if (ModuleConfig::getRole() == MODULE_CENTRE) {
            Serial.print(" - Hydraulics: ");
            Serial.print(hydraulicController.getStatusString());
        }
        
        Serial.println();
        
        DiagnosticManager::logMessage(LOG_DEBUG, "System", 
            "Heartbeat - Uptime: " + String(uptime) + "s, " +
            "TX: " + String(networkManager.getPacketsSent()) + ", " +
            "RX: " + String(networkManager.getPacketsReceived()));
        
        lastHeartbeat = millis();
    }
    
    // Small delay to prevent overwhelming the system
    delay(5);
}
