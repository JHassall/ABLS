# Boom Centre Module Firmware - Logic Overview

This document provides a comprehensive technical overview of the `BoomCentreModule` firmware for the ABLS project. It details the architecture, data flow, and logic of each major component.

## 1. High-Level Architecture

The Boom Centre Module firmware is responsible for three primary tasks:

1.  **Sensing**: Collecting data from its own suite of sensors (GPS, IMU, Radar).
2.  **Controlling**: Managing the three hydraulic rams (Center, Left, Right) that control the boomspray's position, based on commands from the Toughbook.
3.  **Networking**: Acting as the central communication hub. It sends its own sensor data to the Toughbook, receives control commands and RTK correction data (RTCM) from the Toughbook, and forwards the RTCM data to its own GPS module.

The code is modularized into three main components, managed by the main `.ino` file:

*   `SensorManager`: Interfaces with all physical sensors.
*   `HydraulicController`: Manages the ADCs for ram position feedback and the PID controllers for ram actuation.
*   `NetworkManager`: Handles all UDP network communication.

--- 

## 2. Data Flow and Communication (`DataPackets.h`)

Communication between the Teensy and the Toughbook is handled via UDP packets, defined in `DataPackets.h`.

*   **`SensorDataPacket` (Teensy -> Toughbook)**
    *   **Purpose**: Sends a complete snapshot of the Central Teensy's state to the central computer.
    *   **Key Fields**:
        *   `Timestamp`: `millis()` timestamp for the packet.
        *   `Latitude`, `Longitude`, `Altitude`, `Heading`: From the F9P GPS.
        *   `Roll`, `Pitch`, `Yaw`: From the BNO080 IMU.
        *   `RadarDistance`: From the XM125 Radar.
        *   `RamPosCenterPercent`, `RamPosLeftPercent`, `RamPosRightPercent`: Feedback from the hydraulic ram ADCs.

*   **`ControlCommandPacket` (Toughbook -> Teensy)**
    *   **Purpose**: Receives target setpoints for the hydraulic rams from the central computer.
    *   **Key Fields**: `SetpointCenter`, `SetpointLeft`, `SetpointRight` (float values representing desired ram positions).

*   **`RTCMDataPacket` (Toughbook -> Teensy)**
    *   **Purpose**: Receives RTK correction data to be forwarded to the F9P GPS for high-accuracy positioning.

--- 

## 3. Sensor Management (`SensorManager`)

*   **Purpose**: Initializes and reads data from all onboard sensors.
*   **Initialization (`init()`)**: 
    1.  **IMU (BNO080)**: Initializes the sensor and enables the `ROTATION_VECTOR` report, which provides fused roll, pitch, and yaw data.
    2.  **Radar (XM125)**: Initializes the sensor and, critically, configures it for **distance mode** using `distanceSetup(500, 5000)`, setting its operational range from 0.5 to 5 meters. This was a key fix from our debugging session.
    3.  **GPS (u-blox F9P)**: Initializes the GPS on `Serial1`, sets its dynamic model to `AUTOMOTIVE` (suitable for a vehicle chassis), and sets the navigation frequency to 10 Hz.
*   **Data Collection (`populatePacket()`)**: This function is called in the main loop to fill the `SensorDataPacket`.
    *   **IMU**: Reads the latest roll, pitch, and yaw if available.
    *   **GPS**: Reads the latest Position, Velocity, and Time (PVT) data.
    *   **Radar**: Implements the correct multi-step reading process:
        1.  `detectorReadingSetup()`: Prepares the sensor for a new reading.
        2.  `getNumberDistances()`: Checks how many objects are detected.
        3.  `getPeakDistance(0, ...)`: If any objects are detected, it retrieves the distance of the first (and strongest) peak. This is the primary distance measurement.

*   **Sensor Fusion Strategy**:
    *   **Onboard (IMU)**: The BNO080 IMU performs its own internal sensor fusion to combine its accelerometer and gyroscope data, providing a stable and accurate `ROTATION_VECTOR` (roll, pitch, yaw). This is handled automatically by the sensor itself.
    *   **System-Level (Toughbook)**: The `BoomCentreModule` firmware **does not** perform any explicit fusion of GPS and IMU data. The design philosophy is to send the raw, timestamped data from each sensor (GPS position, IMU orientation) to the Toughbook. The central control software on the Toughbook is responsible for the higher-level fusion, combining the data streams from all three modules with the Digital Elevation Model (DEM) to create a complete picture of the boomspray's state.
    *   **Dead Reckoning**: The "predict-correct" dead reckoning algorithm, which fuses GPS and IMU for continuous position estimation, is implemented in the `BoomWingModule` firmware, not the `BoomCentreModule`.

--- 

## 4. Hydraulic Control (`HydraulicController`)

*   **Purpose**: Manages the entire hydraulic system: reading ram positions and driving the rams to their target setpoints using PID control.
*   **Initialization (`init()`)**: 
    1.  **ADCs (ADS1115)**: Initializes the three separate ADC boards, one for each hydraulic ram, on their unique I2C addresses.
    2.  **PID Controllers**: Creates and configures three independent `PID` controllers. Each is set to `AUTOMATIC` mode with specific tuning parameters (Kp, Ki, Kd).
    3.  **PWM Pins**: Sets up the Teensy's PWM output pins that will drive the hydraulic valves.
*   **Ram Position Feedback (`getRamPositions()`)**: 
    *   Reads the raw analog value from each of the three ADCs.
    *   Converts this raw value into a percentage (0-100) and populates the `RamPos...Percent` fields in the main `SensorDataPacket`.
*   **Control Loop (`update()`)**: 
    *   This function is the core of the control logic. It receives the `ControlCommandPacket` from the network.
    *   It updates the `_setpoint` variable for each of the three PID controllers with the new targets from the packet.
    *   It reads the current ram position (`_input`) from the ADCs.
    *   It calls `Compute()` on each PID controller, which calculates the required output power based on the error between the setpoint and the current input.
    *   Finally, it applies the calculated power (`_output`) to the corresponding PWM pin using `analogWrite()`, driving the hydraulic valve.

--- 

## 5. Network Operations (`NetworkManager`)

*   **Purpose**: Manages all UDP communication with the Toughbook.
*   **Initialization (`init()`)**: 
    *   Configures the Teensy's Ethernet interface with a static IP address (`192.168.1.180`).
    *   Starts listening for incoming UDP packets on the specified port (`4444`).
*   **Sending Data (`sendPacket()`)**: 
    *   Takes the populated `SensorDataPacket`, begins a UDP packet to the Toughbook's IP (`192.168.1.100`), writes the packet bytes, and ends the packet to send it.
*   **Receiving Data (`loop()`)**: 
    *   Constantly checks for incoming UDP packets.
    *   When a packet arrives, it checks the first byte (`packetId`) to determine its type:
        *   If it's a `ControlCommandPacket` (`0xBB`), it calls the `_hydraulicController->update()` function, passing the command data to the control logic.
        *   If it's an `RTCMDataPacket` (`0xCC`), it calls `_sensorManager->getGPS()->pushRawData()`, forwarding the correction data directly to the F9P module.

--- 

## 6. Main Program Flow (`BoomCentreModule.ino`)

*   **Purpose**: Orchestrates the entire system, tying all the manager components together.
*   **`setup()`**: 
    1.  Initializes the Serial connection for debugging.
    2.  Creates instances of the three manager classes: `sensorManager`, `hydraulicController`, `networkManager`.
    3.  Calls the `init()` method on each manager in sequence, preparing the sensors, hydraulics, and network.
    4.  Passes references between managers where needed (e.g., `networkManager` needs to know about `hydraulicController` to pass it commands).
*   **`loop()`**: 
    *   The main loop runs continuously and is designed to be non-blocking.
    *   It calls the `loop()` method of the `networkManager`, which handles all incoming data.
    *   It uses a `millis()` timer to ensure that the main sensing-and-sending block of code runs at a consistent frequency (currently set to every 20ms, i.e., 50 Hz).
    *   **At 50 Hz**: 
        1.  A new `SensorDataPacket` is created.
        2.  `hydraulicController->getRamPositions()` is called to get the latest feedback.
        3.  `sensorManager->populatePacket()` is called to fill the rest of the packet with sensor data.
        4.  `networkManager->sendPacket()` is called to transmit the completed packet to the Toughbook.
