// =================================================================
// ABLS - Outer Teensy Firmware
// =================================================================
// This firmware is for the two outer wing Teensys (Left and Right).
// Its sole purpose is to perform local sensor fusion (dead reckoning)
// and send a fused data packet to the Toughbook at 50Hz.
// =================================================================

// --- BOARD CONFIGURATION ---
// UNCOMMENT the line for the board you are programming
#define IS_LEFT_TEENSY
// #define IS_RIGHT_TEENSY

// --- Library Includes ---
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ArduinoJson.h>
#include <SparkFun_BNO080_Arduino_Library.h>

// --- Custom Component Includes ---
#include "DataPackets.h"
#include "SensorManager.h"
#include "NetworkManager.h"

// --- Network Configuration ---
byte mac_left[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xDD };
byte mac_right[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xCC };
IPAddress ip_left(192, 168, 1, 102);
IPAddress ip_right(192, 168, 1, 103);
IPAddress toughbookIp(192, 168, 1, 50);
unsigned int toughbookPort = 8888;
unsigned int localPort = 8890; // Can be the same for both, as they don't listen

// --- Global Objects ---
SensorManager sensorManager;
NetworkManager networkManager(toughbookIp, toughbookPort);
OuterSensorDataPacket packet; // The data packet we will send

// --- Timing Control ---
const unsigned long SENSOR_PACKET_INTERVAL_MS = 20; // 50Hz
unsigned long previousTime = 0;

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  long setupStartTime = millis();
  while (!Serial && (millis() - setupStartTime < 5000)) {
    ; // wait for serial port to connect.
  }

  Serial.println("\n=======================================");
  Serial.println(" ABLS Outer Teensy Firmware Booting");
  Serial.println("=======================================");

  // Configure based on which board this is
  #if defined(IS_LEFT_TEENSY)
    Serial.println("Board configured as LEFT wing.");
    networkManager.init(mac_left, localPort);
    strcpy(packet.SenderId, "OuterLeft");
  #elif defined(IS_RIGHT_TEENSY)
    Serial.println("Board configured as RIGHT wing.");
    networkManager.init(mac_right, localPort);
    strcpy(packet.SenderId, "OuterRight");
  #else
    #error "No board identity defined! Please define IS_LEFT_TEENSY or IS_RIGHT_TEENSY"
  #endif

  // Initialize sensor manager
  sensorManager.init();

  Serial.println("\nSetup Complete. Entering main loop...");
}

// =================================================================
//  MAIN LOOP
// =================================================================
void loop() {
  // This must be called as frequently as possible to check for new
  // GPS and IMU data and run the fusion algorithm.
  sensorManager.update();

  // This timed loop runs at 50Hz to send the latest fused data
  if (millis() - previousTime >= SENSOR_PACKET_INTERVAL_MS) {
    previousTime = millis();

    // Populate the packet with the latest data from the sensor manager
    sensorManager.populatePacket(&packet);

    // Send the packet to the Toughbook
    networkManager.sendPacket(packet);
  }
}
