// =================================================================
// ABLS - Boom Wing Module Firmware
// =================================================================
// This is a wing sensor module. It performs key roles:
// 1. SENSOR NODE: Reads its own GPS, IMU, and Radar data.
// 2. SENSOR FUSION: Performs dead reckoning with GPS and IMU data.
// 3. RTK CLIENT: Receives RTCM correction data from the network.
// =================================================================

// --- Library Includes ---
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

// --- Custom Component Includes ---
#include "DataPackets.h"
#include "SensorManager.h"

#include "NetworkManager.h"

// --- Hardware Definitions ---


// --- Network Configuration ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress toughbookIp(192, 168, 1, 100); // IP of the main control computer

// --- Global Objects ---
// Initialize the NetworkManager with the correct ports from DataPackets.h
NetworkManager networkManager(toughbookIp, SENSOR_DATA_PORT, HYDRAULIC_COMMAND_PORT);

SensorManager sensorManager;

// Buffer for incoming/outgoing RTCM data
uint8_t rtcmBuffer[1024];

// --- Timing Control ---
const unsigned long SENSOR_PACKET_INTERVAL_MS = 20; // 50Hz
unsigned long previousTime = 0;

// =================================================================
//  Forward Declarations
// =================================================================
void handleRtcmBroadcast();
void handleRtcmReceive();

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);


  long setupStartTime = millis();
  while (!Serial && (millis() - setupStartTime < 5000)) {
    ; // wait for serial port to connect.
  }

  Serial.println("\n========================================");
  Serial.println(" ABLS Boom Wing Module Firmware Booting");
  Serial.println("========================================");

  // Initialize all manager components
  networkManager.begin(mac);
  networkManager.beginRtcmListener(); // Start listening for RTCM packets
  sensorManager.init();

  Serial.println("\nSetup Complete. Entering main loop...");
  previousTime = millis();
}

// =================================================================
//  MAIN LOOP
// =================================================================
void loop() {
  // Listen for broadcasted RTCM data and forward to the local GPS
  handleRtcmReceive();

  // --- Timed Loop for Sensor Reading, PID, and Sending Data (50Hz) ---
  if (millis() - previousTime >= SENSOR_PACKET_INTERVAL_MS) {
    previousTime = millis();

    // 1. Read all sensors (GPS, IMU, Radar)
    sensorManager.update();

    // 3. Prepare and send the fused sensor packet
    SensorDataPacket packet;
    sensorManager.populatePacket(&packet);
    networkManager.sendSensorData(packet);
  }
}

// =================================================================
//  RTCM Handling Functions
// =================================================================

/**
 * @brief Checks the network for incoming RTCM data packets and, if found,
 *        forwards the data to the local GPS module for processing.
 */
void handleRtcmReceive() {
  // This allows the Central Teensy to use its own broadcasted corrections
  int bytesRead = networkManager.readRtcmData(rtcmBuffer, sizeof(rtcmBuffer));
  if (bytesRead > 0) {
    sensorManager.forwardRtcmToGps(rtcmBuffer, bytesRead);
  }
}
