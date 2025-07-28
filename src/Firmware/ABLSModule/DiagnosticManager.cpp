/*
 * ABLS: Automatic Boom Levelling System
 * Diagnostic Manager Implementation
 * 
 * Author: James Hassall @ RobotsGoFarming.com
 * Version: 1.0.0
 */

#include "DiagnosticManager.h"

// Static member initialization
Adafruit_SSD1306 DiagnosticManager::_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
File DiagnosticManager::_logFile;
bool DiagnosticManager::_initialized = false;
bool DiagnosticManager::_displayAvailable = false;
bool DiagnosticManager::_sdCardAvailable = false;
uint32_t DiagnosticManager::_startTime = 0;
uint32_t DiagnosticManager::_lastDisplayUpdate = 0;
DisplayPage_t DiagnosticManager::_currentPage = DISPLAY_STATUS;
uint32_t DiagnosticManager::_pageChangeTime = 0;

// Display content
String DiagnosticManager::_networkStatus = "Initializing...";
String DiagnosticManager::_networkIP = "";
String DiagnosticManager::_gpsData = "No data";
String DiagnosticManager::_imuData = "No data";
String DiagnosticManager::_radarData = "No data";
String DiagnosticManager::_systemStatus = "Starting up";
uint32_t DiagnosticManager::_errorCount = 0;
uint32_t DiagnosticManager::_warningCount = 0;

bool DiagnosticManager::initialize() {
    _startTime = millis();
    
    Serial.println("Initializing Diagnostic Manager...");
    
    // Initialize I2C for OLED display
    Wire.begin();
    
    // Initialize OLED display
    _displayAvailable = initializeOLED();
    if (_displayAvailable) {
        Serial.println("✅ OLED Display initialized successfully");
        showBootScreen();
    } else {
        Serial.println("❌ OLED Display initialization failed");
    }
    
    // Initialize SD card
    _sdCardAvailable = initializeSDCard();
    if (_sdCardAvailable) {
        Serial.println("✅ SD Card initialized successfully");
        logStartup();
    } else {
        Serial.println("❌ SD Card initialization failed");
    }
    
    _initialized = (_displayAvailable || _sdCardAvailable);
    
    if (_initialized) {
        logMessage(LOG_INFO, "DiagnosticManager", "Diagnostic system initialized");
        Serial.println("✅ Diagnostic Manager initialized");
    } else {
        Serial.println("❌ Diagnostic Manager initialization failed - no hardware available");
    }
    
    return _initialized;
}

bool DiagnosticManager::initializeOLED() {
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        return false;
    }
    
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.display();
    
    return true;
}

bool DiagnosticManager::initializeSDCard() {
    if (!SD.begin(SD_CS_PIN)) {
        return false;
    }
    
    // Create logs directory if it doesn't exist
    if (!SD.exists("/logs")) {
        SD.mkdir("/logs");
    }
    
    return true;
}

void DiagnosticManager::showBootScreen() {
    if (!_displayAvailable) return;
    
    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setCursor(0, 0);
    _display.println("ABLS");
    
    _display.setTextSize(1);
    _display.setCursor(0, 20);
    _display.println("Automatic Boom");
    _display.println("Levelling System");
    
    _display.setCursor(0, 40);
    _display.println("Initializing...");
    
    _display.setCursor(0, 56);
    _display.println("v1.0.0");
    
    _display.display();
    delay(2000); // Show boot screen for 2 seconds
}

void DiagnosticManager::showErrorScreen(const String& error) {
    if (!_displayAvailable) return;
    
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("ERROR:");
    _display.println();
    
    // Word wrap the error message
    int lineLength = 21; // Characters per line at text size 1
    String remaining = error;
    int y = 16;
    
    while (remaining.length() > 0 && y < 56) {
        String line = remaining.substring(0, min((int)remaining.length(), lineLength));
        _display.setCursor(0, y);
        _display.println(line);
        remaining = remaining.substring(line.length());
        y += 8;
    }
    
    _display.display();
}

void DiagnosticManager::updateDisplay() {
    if (!_displayAvailable) return;
    
    uint32_t now = millis();
    
    // Update display every 500ms
    if (now - _lastDisplayUpdate < 500) return;
    _lastDisplayUpdate = now;
    
    // Auto-cycle through pages every 5 seconds
    if (now - _pageChangeTime > 5000) {
        nextDisplayPage();
        _pageChangeTime = now;
    }
    
    _display.clearDisplay();
    
    switch (_currentPage) {
        case DISPLAY_STATUS:
            drawStatusPage();
            break;
        case DISPLAY_NETWORK:
            drawNetworkPage();
            break;
        case DISPLAY_SENSORS:
            drawSensorsPage();
            break;
        case DISPLAY_SYSTEM:
            drawSystemPage();
            break;
        default:
            // Should never reach here, but handle gracefully
            drawStatusPage();
            break;
    }
    
    _display.display();
}

void DiagnosticManager::drawStatusPage() {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("STATUS [1/4]");
    _display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
    
    _display.setCursor(0, 12);
    _display.print("Role: ");
    _display.println(ModuleConfig::getRoleName());
    
    _display.setCursor(0, 22);
    _display.print("Uptime: ");
    uint32_t uptime = getUptime() / 1000;
    _display.print(uptime / 60);
    _display.print("m ");
    _display.print(uptime % 60);
    _display.println("s");
    
    _display.setCursor(0, 32);
    _display.print("Status: ");
    _display.println(_systemStatus);
    
    _display.setCursor(0, 42);
    _display.print("Errors: ");
    _display.print(_errorCount);
    _display.print(" Warn: ");
    _display.println(_warningCount);
    
    _display.setCursor(0, 56);
    _display.print("Mem: ");
    _display.print(getFreeMemory());
    _display.println(" bytes");
}

void DiagnosticManager::drawNetworkPage() {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("NETWORK [2/4]");
    _display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
    
    _display.setCursor(0, 12);
    _display.print("Status: ");
    _display.println(_networkStatus);
    
    if (_networkIP.length() > 0) {
        _display.setCursor(0, 22);
        _display.print("IP: ");
        _display.println(_networkIP);
    }
    
    _display.setCursor(0, 32);
    _display.println("Packets:");
    _display.setCursor(0, 42);
    _display.println("TX: 0  RX: 0");
    
    _display.setCursor(0, 56);
    _display.println("RTCM: Waiting...");
}

void DiagnosticManager::drawSensorsPage() {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("SENSORS [3/4]");
    _display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
    
    _display.setCursor(0, 12);
    _display.print("GPS: ");
    _display.println(_gpsData);
    
    _display.setCursor(0, 22);
    _display.print("IMU: ");
    _display.println(_imuData);
    
    _display.setCursor(0, 32);
    _display.print("Radar: ");
    _display.println(_radarData);
    
    _display.setCursor(0, 42);
    _display.println("RTK: No fix");
    
    _display.setCursor(0, 56);
    _display.println("All sensors init...");
}

void DiagnosticManager::drawSystemPage() {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("SYSTEM [4/4]");
    _display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
    
    _display.setCursor(0, 12);
    _display.print("SD Card: ");
    _display.println(_sdCardAvailable ? "OK" : "FAIL");
    
    _display.setCursor(0, 22);
    _display.print("OLED: ");
    _display.println(_displayAvailable ? "OK" : "FAIL");
    
    _display.setCursor(0, 32);
    _display.print("Free RAM: ");
    _display.println(getFreeMemory());
    
    _display.setCursor(0, 42);
    _display.print("Log Entries: ");
    _display.println("0");
    
    _display.setCursor(0, 56);
    _display.println("System: Running");
}

void DiagnosticManager::nextDisplayPage() {
    _currentPage = (DisplayPage_t)((_currentPage + 1) % DISPLAY_MAX);
    _pageChangeTime = millis();
}

void DiagnosticManager::setDisplayPage(DisplayPage_t page) {
    _currentPage = page;
    _pageChangeTime = millis();
}

void DiagnosticManager::logMessage(LogLevel_t level, const String& component, const String& message) {
    if (!_sdCardAvailable) return;
    
    // Count errors and warnings
    if (level == LOG_ERROR || level == LOG_CRITICAL) {
        _errorCount++;
    } else if (level == LOG_WARNING) {
        _warningCount++;
    }
    
    // Create log file name based on current date
    char logFileName[32];
    createLogFileName(logFileName, sizeof(logFileName));
    
    // Open log file for appending
    _logFile = SD.open(logFileName, FILE_WRITE);
    if (_logFile) {
        _logFile.print(getTimestamp());
        _logFile.print(" [");
        _logFile.print(getLogLevelString(level));
        _logFile.print("] ");
        _logFile.print(component);
        _logFile.print(": ");
        _logFile.println(message);
        _logFile.close();
    }
}

void DiagnosticManager::logStartup() {
    logMessage(LOG_INFO, "System", "=== ABLS Module Starting ===");
    logMessage(LOG_INFO, "System", "Firmware Version: 1.0.0");
    logMessage(LOG_INFO, "System", "Author: James Hassall @ RobotsGoFarming.com");
}

void DiagnosticManager::logRoleDetection(ModuleRole_t role, bool success) {
    if (success) {
        logMessage(LOG_INFO, "ModuleConfig", "Role detected: " + ModuleConfig::getRoleName());
    } else {
        logMessage(LOG_ERROR, "ModuleConfig", "Role detection failed - invalid DIP switch configuration");
    }
}

void DiagnosticManager::logError(const String& component, const String& error) {
    logMessage(LOG_ERROR, component, error);
}

void DiagnosticManager::logCrash(const String& reason) {
    logMessage(LOG_CRITICAL, "System", "CRASH: " + reason);
}

uint32_t DiagnosticManager::getFreeMemory() {
    // Teensy 4.1 has different memory layout - this is an approximation
    extern unsigned long _heap_end;
    extern char *__brkval;
    
    if (__brkval == 0) {
        // If no heap allocation has occurred, return stack pointer distance
        return (char*)&_heap_end - (char*)__builtin_frame_address(0);
    } else {
        return (char*)&_heap_end - __brkval;
    }
}

String DiagnosticManager::getLogLevelString(LogLevel_t level) {
    switch (level) {
        case LOG_DEBUG:    return "DEBUG";
        case LOG_INFO:     return "INFO";
        case LOG_WARNING:  return "WARN";
        case LOG_ERROR:    return "ERROR";
        case LOG_CRITICAL: return "CRIT";
        default:           return "UNKNOWN";
    }
}

String DiagnosticManager::getTimestamp() {
    uint32_t ms = millis();
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu.%03lu", 
             hours % 24, minutes % 60, seconds % 60, ms % 1000);
    
    return String(timestamp);
}

void DiagnosticManager::createLogFileName(char* buffer, size_t bufferSize) {
    // For now, use a simple daily log file name
    // In a real implementation, you might use RTC for actual date
    uint32_t days = millis() / (24UL * 60UL * 60UL * 1000UL);
    snprintf(buffer, bufferSize, "/logs/abls_%03lu.log", days);
}

void DiagnosticManager::setNetworkStatus(const String& status, const String& ip) {
    _networkStatus = status;
    _networkIP = ip;
}

void DiagnosticManager::setSensorData(const String& gps, const String& imu, const String& radar) {
    _gpsData = gps;
    _imuData = imu;
    _radarData = radar;
}

void DiagnosticManager::setSystemStatus(const String& status) {
    _systemStatus = status;
}
