// =================================================================
// ABLS - Central Teensy Firmware
// =================================================================
// This is the main control hub. It performs two key roles:
// 1. SENSOR NODE: Reads its own GPS, IMU, and Radar data.
// 2. CONTROL HUB: Reads all 3 ram positions and controls all 3 hydraulic valves.
// =================================================================

// --- Library Includes ---
#include <ArduinoJson.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <Adafruit_ADS1X15.h> // For the external ADC

// --- Custom Component Includes ---
#include "DataPackets.h"
#include "SensorManager.h"
#include "HydraulicController.h"
#include "NetworkManager.h"

// --- Network Configuration ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 101);         // This Teensy's IP
IPAddress toughbookIp(192, 168, 1, 50); // Toughbook's IP
unsigned int toughbookPort = 8888;      // Port to send sensor data to
unsigned int localPort = 8889;          // Port to listen for commands on

// --- Global Objects ---
SensorManager sensorManager;
HydraulicController hydraulicController;
NetworkManager networkManager(toughbookIp, toughbookPort, localPort);

// --- Timing Control ---
const unsigned long SENSOR_PACKET_INTERVAL_MS = 20; // 50Hz
unsigned long previousTime = 0;

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  // It's good practice to wait for Serial to connect, especially for debugging.
  // Add a timeout so it doesn't hang forever if not connected.
  long setupStartTime = millis();
  while (!Serial && (millis() - setupStartTime < 5000)) {
    ; // wait for serial port to connect.
  }

  Serial.println("\n========================================");
  Serial.println(" ABLS Central Teensy Firmware Booting");
  Serial.println("========================================");

  // Initialize all manager components
  sensorManager.init();
  hydraulicController.init();
  networkManager.init(mac);

  Serial.println("\nSetup Complete. Entering main loop...");
}

// =================================================================
//  MAIN LOOP
// =================================================================
void loop() {
  unsigned long currentTime = millis();

  // --- Receive Network Commands (as often as possible) ---
  ControlCommandPacket command;
  if (networkManager.receiveCommand(command)) {
    // If a valid command is received, pass it to the hydraulic controller
    hydraulicController.updateSetpoint(command.TargetId, command.Value);
  }

  // --- Timed Loop for Sensor Reading, PID, and Sending Data (50Hz) ---
  if (currentTime - previousTime >= SENSOR_PACKET_INTERVAL_MS) {
    previousTime = currentTime;

    // 1. Read all sensors
    sensorManager.update();

    // 2. Read ram positions and run PID loops
    hydraulicController.update();

    // 3. Prepare and send the network packet
    SensorDataPacket packet;
    sensorManager.populatePacket(&packet);
    hydraulicController.addRamPositionsToPacket(&packet);
    networkManager.sendPacket(packet);
  }
}

// --- Library Includes ---
#include <ArduinoJson.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

// --- Custom Component Includes ---
// (We will create these files next)
#include "SensorManager.h"
#include "HydraulicController.h"
#include "NetworkManager.h"

// --- Network Configuration ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 101); // This Teensy's IP
IPAddress toughbookIp(192, 168, 1, 50); // Toughbook's IP
unsigned int toughbookPort = 8888; // Port to send sensor data to
unsigned int localPort = 8889;     // Port to listen for commands on

// --- Global Objects ---
SensorManager sensorManager;
HydraulicController hydraulicController;
NetworkManager networkManager(toughbookIp, toughbookPort, localPort);

// --- Timing Control ---
const unsigned long SENSOR_PACKET_INTERVAL_MS = 20; // 50Hz
unsigned long previousTime = 0;

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  Serial.println("Central Teensy Initializing...");

  // Initialize all components
  sensorManager.init();
  hydraulicController.init();
  networkManager.init(mac, ip);

  Serial.println("Initialization Complete. Starting main loop.");
  previousTime = millis();
}

// =================================================================
//  LOOP
// =================================================================
void loop() {
  // 1. Handle incoming network commands (non-blocking)
  ControlCommandPacket command = networkManager.checkForCommands();
  if (command.isValid) {
    hydraulicController.updateSetpoint(command.TargetId, command.Value);
  }

  // 2. Update all sensor readings
  sensorManager.update();

  // 3. Update the hydraulic PID loops
  hydraulicController.update();

  // 4. Send sensor data packet on a fixed interval (50Hz)
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= SENSOR_PACKET_INTERVAL_MS) {
    previousTime = currentTime;

    // Create the data packet
    SensorDataPacket packet = sensorManager.getSensorData();
    packet.SourceId = "center_boom";
    
    // Add the latest ram positions to the packet
    hydraulicController.addRamPositionsToPacket(&packet);

    // Send the packet
    networkManager.sendPacket(packet);
  }
}
