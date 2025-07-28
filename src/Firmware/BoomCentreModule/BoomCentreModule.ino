// =================================================================
//  ABLS Boom Centre Module Firmware
// =================================================================
//
//  This firmware is for the central Boom Centre Module (Teensy 4.1 controller).
//  It has been reconstructed to align with the LOGIC_OVERVIEW.md documentation.
//
//  Primary Responsibilities:
//    1. Reading all its local sensors (GPS, IMU, Radar).
//    2. Reading all three hydraulic ram position sensors (via ADS1115).
//    3. Controlling all three hydraulic rams via PID controllers.
//    4. Receiving RTCM correction data from a radio modem, broadcasting it over
//       UDP, and forwarding it to its own GPS module.
//    5. Sending a fused packet of all its sensor data to the Toughbook at 50Hz.
//    6. Receiving control command packets from the Toughbook to update PID setpoints.
//
// =================================================================

// --- Arduino and System Includes ---
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <ArduinoJson.h>

// --- Custom Component Includes ---
#include "DataPackets.h"
#include "SensorManager.h"
#include "HydraulicController.h"
#include "NetworkManager.h"

// --- Hardware Definitions ---
#define RTCM_RADIO_PORT Serial2 // Radio modem is connected to Serial2

// --- Network Configuration ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress toughbookIp(192, 168, 1, 100); // IP of the Toughbook

// --- Global Objects ---
NetworkManager networkManager(toughbookIp, SENSOR_DATA_PORT, HYDRAULIC_COMMAND_PORT);
HydraulicController hydraulicController;
SensorManager sensorManager;

// Buffer for incoming/outgoing RTCM data
byte rtcmBuffer[512];

// Timing for the main 50Hz loop
const int SENSOR_PACKET_INTERVAL_MS = 20; // 50Hz
unsigned long previousTime = 0;

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { /* wait for serial port to connect */ }
  Serial.println("\nABLS Boom Centre Module Firmware Initializing...");

  // Initialize hardware and managers
  networkManager.begin(mac);
  sensorManager.init();
  hydraulicController.init();

  // Pass the hydraulic controller instance to the network manager
  // so it can directly update the PID setpoints upon receiving a command.
  networkManager.setHydraulicController(&hydraulicController);

  // Initialize the serial port for the RTCM radio
  RTCM_RADIO_PORT.begin(115200);

  Serial.println("\nSetup Complete. Entering main loop...");
  previousTime = millis();
}

// =================================================================
//  MAIN LOOP
// =================================================================
void loop() {
  // --- Handle Real-Time Tasks (as often as possible) ---

  // 1. Read RTCM data from radio, broadcast it, and forward to our local GPS
  handleRtcmRadio();

  // 2. Listen for hydraulic commands from the Toughbook (handled by NetworkManager)
  networkManager.update();

  // --- Timed Loop for Sensor Reading, PID, and Sending Data (50Hz) ---
  if (millis() - previousTime >= SENSOR_PACKET_INTERVAL_MS) {
    previousTime = millis();

    // 1. Read all sensors (GPS, IMU, Radar)
    sensorManager.update();

    // 2. Read ram positions and run PID loops
    hydraulicController.update();

    // 3. Prepare and send the fused sensor packet
    SensorDataPacket packet;
    sensorManager.populatePacket(&packet);
    hydraulicController.addRamPositionsToPacket(&packet);
    networkManager.sendSensorData(packet);
  }
}

// =================================================================
//  HELPER FUNCTIONS
// =================================================================

/**
 * @brief Checks the radio modem for incoming RTCM data. If found, it
 *        broadcasts the data over UDP to the entire network AND forwards
 *        it directly to the local GPS module for immediate use.
 */
void handleRtcmRadio() {
    if (RTCM_RADIO_PORT.available()) {
        int bytesRead = RTCM_RADIO_PORT.readBytes(rtcmBuffer, sizeof(rtcmBuffer));
        if (bytesRead > 0) {
            // Broadcast to other Teensys
            networkManager.broadcastRtcmData(rtcmBuffer, bytesRead);
            // Also forward to our own GPS
            sensorManager.forwardRtcmToGps(rtcmBuffer, bytesRead);
        }
    }
}
