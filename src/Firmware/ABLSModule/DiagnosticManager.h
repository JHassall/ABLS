/*
 * ABLS: Automatic Boom Levelling System
 * Diagnostic Manager - OLED Display and SD Card Logging
 * 
 * Provides real-time diagnostics display and comprehensive logging
 * for development debugging and field troubleshooting.
 * 
 * Hardware:
 * - PiicoDev OLED Display (SSD1306, 128x64, I2C)
 * - Teensy 4.1 built-in SD card slot
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#ifndef DIAGNOSTIC_MANAGER_H
#define DIAGNOSTIC_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <SPI.h>
#include "ModuleConfig.h"

// OLED Display Configuration
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1  // Reset pin (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS  0x3C // I2C address for 128x64 display

// SD Card Configuration
#define SD_CS_PIN       BUILTIN_SDCARD  // Teensy 4.1 built-in SD card CS pin

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} LogLevel_t;

// Display pages for cycling through different information
typedef enum {
    DISPLAY_STATUS = 0,     // Module role, uptime, status
    DISPLAY_NETWORK = 1,    // Network status, IP, packets
    DISPLAY_SENSORS = 2,    // GPS, IMU, radar data
    DISPLAY_SYSTEM = 3,     // Memory, SD card, errors
    DISPLAY_MAX = 4         // Total number of display pages
} DisplayPage_t;

class DiagnosticManager {
public:
    // Initialization
    static bool initialize();
    static bool isInitialized() { return _initialized; }
    
    // OLED Display Management
    static void updateDisplay();
    static void showBootScreen();
    static void showErrorScreen(const String& error);
    static void nextDisplayPage();
    static void setDisplayPage(DisplayPage_t page);
    
    // SD Card Logging
    static void logMessage(LogLevel_t level, const String& component, const String& message);
    static void logStartup();
    static void logRoleDetection(ModuleRole_t role, bool success);
    static void logError(const String& component, const String& error);
    static void logCrash(const String& reason);
    
    // System Information
    static void updateSystemStats();
    static uint32_t getUptime() { return millis() - _startTime; }
    static uint32_t getFreeMemory();
    static bool isSDCardAvailable() { return _sdCardAvailable; }
    
    // Display Content Updates
    static void setNetworkStatus(const String& status, const String& ip = "");
    static void setSensorData(const String& gps, const String& imu, const String& radar);
    static void setSystemStatus(const String& status);

private:
    // Hardware objects
    static Adafruit_SSD1306 _display;
    static File _logFile;
    
    // State tracking
    static bool _initialized;
    static bool _displayAvailable;
    static bool _sdCardAvailable;
    static uint32_t _startTime;
    static uint32_t _lastDisplayUpdate;
    static DisplayPage_t _currentPage;
    static uint32_t _pageChangeTime;
    
    // Display content
    static String _networkStatus;
    static String _networkIP;
    static String _gpsData;
    static String _imuData;
    static String _radarData;
    static String _systemStatus;
    static uint32_t _errorCount;
    static uint32_t _warningCount;
    
    // Internal methods
    static bool initializeOLED();
    static bool initializeSDCard();
    static void drawStatusPage();
    static void drawNetworkPage();
    static void drawSensorsPage();
    static void drawSystemPage();
    static String getLogLevelString(LogLevel_t level);
    static String getTimestamp();
    static void createLogFileName(char* buffer, size_t bufferSize);
};

#endif // DIAGNOSTIC_MANAGER_H
