# 1050 Robot

This is the code for a food delivery robot running on an ESP-32 microcontroller. The robot navigates on a table, avoids obstacles, and unlocks a lid using RFID.

## Components

- **ESP-32 Microcontroller**: Main controller
- **2 Motor Drivers** (e.g., L298N): Control 4 DC motors (2 per side for differential drive)
- **RFID Reader** (MFRC522): Detects RFID tags to unlock the lid
- **Ultrasonic Sensor** (HC-SR04): Detects obstacles ahead
- **2 IR Sensors**: Cliff detection on left and right sides
- **2 Microswitches**: Safety stops (e.g., bump sensors)
- **Microservo**: Rotates 90 degrees to unlock the lid

## Setup Instructions

1. **Install Arduino IDE**: Download from [arduino.cc](https://www.arduino.cc/en/software)
2. **Add ESP-32 Board Support**:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add `https://dl.espressif.com/dl/package_esp32_index.json` to Additional Board Manager URLs
   - Tools > Board > Boards Manager > Search for ESP32 > Install
3. **Install Libraries**:
   - MFRC522 (for RFID): Sketch > Include Library > Manage Libraries > Search for MFRC522
   - Servo: Built-in, no installation needed
4. **Hardware Connections**:
   - Update the pin definitions in `robot.ino` to match your actual wiring
   - Ensure power supply: One battery with buck converter for ESP-32 and motors
5. **Upload Code**:
   - Select ESP-32 board in Tools > Board
   - Select correct port in Tools > Port
   - Click Upload

## Robot Behavior

- **Movement**: Moves forward continuously
- **Obstacle Avoidance**:
  - Stops if ultrasonic sensor detects object closer than 8cm
  - Stops if either microswitch is pressed
  - Stops if either IR sensor detects a cliff (analog value below threshold)
- **RFID Unlock**: When an RFID tag is detected, the servo rotates 90 degrees for 1 second to unlock the lid, then returns

## Customization

- Adjust `IR_THRESHOLD` in the code based on your IR sensor readings (test with Serial.print)
- Modify motor speeds in `moveForward()` function
- Add more complex navigation logic as needed (e.g., turning, remote control)

## Troubleshooting

- Check Serial Monitor for debug output
- Verify all pin connections
- Ensure adequate power supply (motors can draw significant current)

## Dependencies

- Arduino IDE with ESP-32 support
- MFRC522 library
- Servo library (built-in)