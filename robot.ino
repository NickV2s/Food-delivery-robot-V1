#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// Workflow summary:
// 1. Wait for an RFID tag that represents a delivery destination.
// 2. Drive forward for that table's configured travel time.
// 3. Stop immediately if the ultrasonic sensor, any microswitch, or the IR edge logic trips.
// 4. If travel finishes normally, wait for a second RFID tag to unlock the lid.
// 5. After unlocking, accept the next movement command and relock the lid for the next trip.

// Mock pin definitions for the prototype logic. Replace with the real wiring if this moves beyond a mock sketch.
#define RFID_SS_PIN 33
#define RFID_RST_PIN 13
#define RFID_SCK_PIN 14
#define RFID_MISO_PIN 27
#define RFID_MOSI_PIN 26
#define SERVO_PIN 32
#define ULTRASONIC_TRIGGER 16
#define ULTRASONIC_ECHO 17
#define IR_LEFT 34
#define IR_RIGHT 35
#define MICROSWITCH_LEFT 23
#define MICROSWITCH_CENTER 12
#define MICROSWITCH_RIGHT 25

#define LEFT_IN1 19
#define LEFT_IN2 21
#define LEFT_IN3 22
#define LEFT_IN4 2

#define RIGHT_IN1 15
#define RIGHT_IN2 4
#define RIGHT_IN3 18
#define RIGHT_IN4 5

const unsigned long HEARTBEAT_INTERVAL_MS = 2000UL;
const unsigned long RFID_SETTLE_DELAY_MS = 250UL;
const unsigned long TABLE_A_TRAVEL_MS = 2000UL;
const unsigned long TABLE_B_TRAVEL_MS = 4500UL;
const float OBSTACLE_STOP_DISTANCE_CM = 100.0F;
// Placeholder threshold because the exact IR sensor output behavior was left open for the mock version.
const int IR_EDGE_THRESHOLD = 1800;
// For the SG90 setup described, 90 degrees is locked and 0 degrees is unlocked.
const int SERVO_LOCKED_ANGLE = 90;
const int SERVO_UNLOCKED_ANGLE = 0;

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo lidServo;

// These states model the full delivery cycle from idle, to driving, to lid unlock.
enum RobotState {
  WAITING_FOR_START,
  MOVING,
  WAITING_FOR_UNLOCK,
  LID_UNLOCKED
};

enum TagAction {
  START_MOVE,
  UNLOCK_LID
};

struct TagCommand {
  byte uid[7];
  byte uidLength;
  TagAction action;
  unsigned long travelTimeMs;
  const char* label;
};

// Placeholder RFID UIDs for Table A, Table B, and lid unlock in the mock flow.
// Replace these tag values and travel times with the real delivery-table data later.
const TagCommand TAG_COMMANDS[] = {
  { { 0xDE, 0xAD, 0xBE, 0xEF }, 4, START_MOVE, TABLE_A_TRAVEL_MS, "Route to Table A" },
  { { 0xCA, 0xFE, 0xBA, 0xBE }, 4, START_MOVE, TABLE_B_TRAVEL_MS, "Route to Table B" },
  { { 0x12, 0x34, 0x56, 0x78 }, 4, UNLOCK_LID, 0UL, "Unlock lid after delivery" }
};
const size_t TAG_COMMAND_COUNT = sizeof(TAG_COMMANDS) / sizeof(TAG_COMMANDS[0]);

unsigned long lastHeartbeatMs = 0;
unsigned long movementStartMs = 0;
unsigned long movementDurationMs = 0;
bool rfidReady = false;
RobotState robotState = WAITING_FOR_START;

const char* getStateName(RobotState state) {
  switch (state) {
    case WAITING_FOR_START:
      return "WAITING_FOR_START";
    case MOVING:
      return "MOVING";
    case WAITING_FOR_UNLOCK:
      return "WAITING_FOR_UNLOCK";
    case LID_UNLOCKED:
      return "LID_UNLOCKED";
  }

  return "UNKNOWN";
}

// Search the command table for a tag UID that matches the scanned card.
const TagCommand* findTagCommand(const MFRC522::Uid& uid) {
  for (size_t commandIndex = 0; commandIndex < TAG_COMMAND_COUNT; commandIndex++) {
    const TagCommand& command = TAG_COMMANDS[commandIndex];

    if (uid.size != command.uidLength) {
      continue;
    }

    bool matches = true;
    for (byte i = 0; i < uid.size; i++) {
      if (uid.uidByte[i] != command.uid[i]) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return &command;
    }
  }

  return nullptr;
}

void printUid(const MFRC522::Uid& uid) {
  Serial.print("UID: ");
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(uid.uidByte[i], HEX);
    if (i < uid.size - 1) {
      Serial.print(" ");
    }
  }
  Serial.println();
}

// Each motor side is driven by two H-bridge channels.
void driveMotorChannel(int in1Pin, int in2Pin, bool forward) {
  digitalWrite(in1Pin, forward ? HIGH : LOW);
  digitalWrite(in2Pin, forward ? LOW : HIGH);
}

void setLeftMotor(bool forward) {
  driveMotorChannel(LEFT_IN1, LEFT_IN2, forward);
  driveMotorChannel(LEFT_IN3, LEFT_IN4, forward);
}

void setRightMotor(bool forward) {
  driveMotorChannel(RIGHT_IN1, RIGHT_IN2, forward);
  driveMotorChannel(RIGHT_IN3, RIGHT_IN4, !forward);
}

void moveForward() {
  setLeftMotor(true);
  setRightMotor(true);
}

void stopMotors() {
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
  digitalWrite(LEFT_IN3, LOW);
  digitalWrite(LEFT_IN4, LOW);

  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
  digitalWrite(RIGHT_IN3, LOW);
  digitalWrite(RIGHT_IN4, LOW);
}

// Returns -1 when the ultrasonic sensor does not receive an echo in time.
float readObstacleDistanceCm() {
  digitalWrite(ULTRASONIC_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIGGER, LOW);

  unsigned long pulseDuration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000UL);
  if (pulseDuration == 0) {
    return -1.0F;
  }

  return pulseDuration * 0.0343F / 2.0F;
}

bool isObstacleDetected() {
  float distanceCm = readObstacleDistanceCm();
  return distanceCm > 0.0F && distanceCm <= OBSTACLE_STOP_DISTANCE_CM;
}

bool isMicroswitchTriggered() {
  return digitalRead(MICROSWITCH_LEFT) == LOW
    || digitalRead(MICROSWITCH_CENTER) == LOW
    || digitalRead(MICROSWITCH_RIGHT) == LOW;
}

bool isEdgeDetected() {
  // Placeholder analog logic for the mock build. If the final IR modules are used as digital sensors,
  // this function should be rewritten to use digitalRead instead of an analog threshold.
  int leftReading = analogRead(IR_LEFT);
  int rightReading = analogRead(IR_RIGHT);
  return leftReading < IR_EDGE_THRESHOLD || rightReading < IR_EDGE_THRESHOLD;
}

// Safety conditions are checked in priority order while the robot is moving.
const char* getSafetyStopReason() {
  if (isObstacleDetected()) {
    return "Obstacle detected. Travel aborted.";
  }

  if (isMicroswitchTriggered()) {
    return "Microswitch triggered. Travel aborted.";
  }

  if (isEdgeDetected()) {
    return "Edge detected by IR sensor. Travel aborted.";
  }

  return nullptr;
}

void startMovement(unsigned long durationMs, const char* label) {
  // Any new trip begins with the lid returned to its locked transport position.
  lidServo.write(SERVO_LOCKED_ANGLE);
  movementStartMs = millis();
  movementDurationMs = durationMs;
  robotState = MOVING;
  moveForward();

  Serial.print("Starting movement command: ");
  Serial.print(label);
  Serial.print(" for ");
  Serial.print(durationMs);
  Serial.println(" ms.");
}

void finishMovement() {
  stopMotors();
  robotState = WAITING_FOR_UNLOCK;
  movementDurationMs = 0;
  Serial.println("Travel complete. Waiting for RFID unlock tag.");
}

// Any safety-triggered stop sends the robot back to the idle state.
void abortMovement(const char* reason) {
  stopMotors();
  robotState = WAITING_FOR_START;
  movementDurationMs = 0;
  Serial.println(reason);
  Serial.println("Waiting for a new RFID movement command.");
}

// The lid only unlocks after the delivery route completes and an unlock tag is scanned.
void unlockLid() {
  lidServo.write(SERVO_UNLOCKED_ANGLE);
  robotState = LID_UNLOCKED;
  Serial.println("Unlock tag accepted. Lid servo moved to the configured unlock angle.");
}

// Tag actions are accepted only in the state where they make sense.
void handleTagCommand(const TagCommand& command) {
  if (command.action == START_MOVE) {
    // After a delivery is unlocked, the next movement tag starts a new trip and relocks the lid.
    if (robotState == WAITING_FOR_START || robotState == LID_UNLOCKED) {
      startMovement(command.travelTimeMs, command.label);
    } else {
      Serial.print("Ignoring movement tag while state is ");
      Serial.println(getStateName(robotState));
    }
    return;
  }

  if (command.action == UNLOCK_LID) {
    if (robotState == WAITING_FOR_UNLOCK) {
      unlockLid();
    } else {
      Serial.print("Ignoring unlock tag while state is ");
      Serial.println(getStateName(robotState));
    }
  }
}

// Poll the RFID reader once per loop and dispatch the matching command, if any.
void pollRfid() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  printUid(rfid.uid);

  const TagCommand* command = findTagCommand(rfid.uid);
  if (command == nullptr) {
    Serial.println("RFID tag not recognized. No action taken.");
  } else {
    Serial.print("RFID command received: ");
    Serial.println(command->label);
    handleTagCommand(*command);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(RFID_SETTLE_DELAY_MS);
}

// Movement runs without a blocking delay so safety sensors can stop the robot immediately.
void updateMovement() {
  if (robotState != MOVING) {
    return;
  }

  const char* stopReason = getSafetyStopReason();
  if (stopReason != nullptr) {
    abortMovement(stopReason);
    return;
  }

  if (millis() - movementStartMs >= movementDurationMs) {
    finishMovement();
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("Boot OK");
  Serial.print("RC522 pins -> SS:");
  Serial.print(RFID_SS_PIN);
  Serial.print(" RST:");
  Serial.print(RFID_RST_PIN);
  Serial.print(" SCK:");
  Serial.print(RFID_SCK_PIN);
  Serial.print(" MISO:");
  Serial.print(RFID_MISO_PIN);
  Serial.print(" MOSI:");
  Serial.println(RFID_MOSI_PIN);

  Serial.println("Running mock hardware configuration. Update placeholder pins and thresholds before real deployment.");

  // Inputs are configured first so the robot can read all safety sensors immediately.
  pinMode(ULTRASONIC_TRIGGER, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);
  pinMode(MICROSWITCH_LEFT, INPUT_PULLUP);
  pinMode(MICROSWITCH_CENTER, INPUT_PULLUP);
  pinMode(MICROSWITCH_RIGHT, INPUT_PULLUP);

  // Reset and initialize the RFID reader on the configured SPI pins.
  pinMode(RFID_RST_PIN, OUTPUT);
  digitalWrite(RFID_RST_PIN, LOW);
  delay(50);
  digitalWrite(RFID_RST_PIN, HIGH);
  delay(50);

  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();
  delay(20);
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("RC522 VersionReg: 0x");
  Serial.println(version, HEX);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("RC522 not detected. Check wiring for SDA/SS, RST, SCK, MISO, MOSI, 3.3V, GND.");
  } else {
    rfidReady = true;
    Serial.println("RC522 detected.");
  }

  // Motor outputs default low so the robot never drives on boot.
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(LEFT_IN3, OUTPUT);
  pinMode(LEFT_IN4, OUTPUT);
  
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(RIGHT_IN3, OUTPUT);
  pinMode(RIGHT_IN4, OUTPUT);

  // Start with the lid in the locked position for the described SG90 linkage.
  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN, 500, 2400);
  lidServo.write(SERVO_LOCKED_ANGLE);

  stopMotors();

  if (rfidReady) {
    Serial.println("Setup complete. Waiting for RFID movement tag...");
  } else {
    Serial.println("Setup complete, but RC522 is not responding.");
  }
}

void loop() {
  // Periodic heartbeat helps confirm the current state over Serial.
  if (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = millis();

    Serial.print("Alive: ");
    Serial.println(rfidReady ? getStateName(robotState) : "RC522 not detected");
  }

  // Keep the robot stopped if the RFID reader is not available.
  if (!rfidReady) {
    delay(100);
    return;
  }

  // RFID is checked every pass so the sketch can react quickly to start and unlock commands.
  pollRfid();

  // Movement updates happen separately so safety checks are not blocked by a long delay.
  updateMovement();

  delay(25);
}
