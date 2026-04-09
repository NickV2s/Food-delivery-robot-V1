# 1050 Robot

This is mock control code for a food delivery robot running on an ESP-32 microcontroller. The robot navigates on a table, avoids obstacles, and unlocks a lid using RFID.

## Components

- **ESP-32 Microcontroller**: Main controller
- **2 Motor Drivers** (e.g., L298N): Control 4 DC motors (2 per side for differential drive)
- **RFID Reader** (MFRC522): Detects RFID tags to unlock the lid
- **Ultrasonic Sensor** (HC-SR04): Detects obstacles ahead
- **2 IR Sensors**: Cliff detection on left and right sides
- **3 Microswitches**: Safety stops (e.g., bump sensors)
- **Microservo**: Uses placeholder angles in code, currently set so 90 degrees is locked and 0 degrees is unlocked

## Setup Instructions

1. **Install Arduino IDE**: Download from [arduino.cc](https://www.arduino.cc/en/software)
2. **Add ESP-32 Board Support**:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add `https://dl.espressif.com/dl/package_esp32_index.json` to Additional Board Manager URLs
   - Tools > Board > Boards Manager > Search for ESP32 > Install
3. **Install Libraries**:
   - MFRC522 (for RFID): Sketch > Include Library > Manage Libraries > Search for MFRC522
   - ESP32Servo (for the lid servo on ESP-32): Sketch > Include Library > Manage Libraries > Search for ESP32Servo
4. **Hardware Connections**:
   - Update the pin definitions in `robot.ino` to match your actual wiring before powering the drivetrain
   - The sketch currently uses explicit RFID SPI pins (`RFID_SCK_PIN`, `RFID_MISO_PIN`, `RFID_MOSI_PIN`) instead of the ESP-32 default mapping
   - Ensure power supply: One battery with buck converter for ESP-32 and motors
5. **Upload Code**:
   - Select ESP-32 board in Tools > Board
   - Select correct port in Tools > Port
   - Click Upload

## Robot Behavior

- **Idle State**: Stays stopped while waiting for an RFID movement command
- **RFID Movement Command**: A recognized placeholder RFID tag starts movement for that tag's configured placeholder travel time
- **Safety Stops**: During movement, the robot immediately stops if the ultrasonic sensor sees an obstacle within 1 meter, any of the three microswitches is pressed, or either IR sensor detects a table edge
- **Unlock Flow**: After the robot finishes its full travel time, it waits for a separate RFID unlock command and then moves the servo to the configured unlock angle
- **Tag Security**: Unknown RFID tags are ignored

## Customization

- Replace the example UIDs in the `TAG_COMMANDS` table with the real RFID tag UIDs you want to use
- Set each movement tag's `travelTimeMs` value to the travel time for that route
- Adjust `IR_EDGE_THRESHOLD` in the code based on your IR sensor readings or rewrite the logic if your IR modules are used as digital sensors
- Adjust `OBSTACLE_STOP_DISTANCE_CM` if you want a stop distance other than the current mock value of 1 meter

## Troubleshooting

- Check Serial Monitor for debug output
- Verify all pin connections
- Ensure adequate power supply (motors can draw significant current)

## Dependencies

- Arduino IDE with ESP-32 support
- MFRC522 library
- ESP32Servo library