#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// Pin definitions (placeholders - update with actual pins)
#define RFID_SS_PIN 5
#define RFID_RST_PIN 22
#define SERVO_PIN 18
#define ULTRASONIC_TRIGGER 19
#define ULTRASONIC_ECHO 21
#define IR_LEFT 34  // Analog pin for left cliff sensor
#define IR_RIGHT 35  // Analog pin for right cliff sensor
#define MICROSWITCH_LEFT 23
#define MICROSWITCH_RIGHT 25

// Left motor driver pins
#define LEFT_IN1 12
#define LEFT_IN2 13
#define LEFT_ENA 14  // PWM
#define LEFT_IN3 26
#define LEFT_IN4 27
#define LEFT_ENB 32  // PWM

// Right motor driver pins
#define RIGHT_IN1 4
#define RIGHT_IN2 16
#define RIGHT_ENA 17  // PWM
#define RIGHT_IN3 2
#define RIGHT_IN4 15
#define RIGHT_ENB 33

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo servo;

const int IR_THRESHOLD = 500;  // Adjust based on your IR sensors (lower value = cliff detected)

void setup() {
  Serial.begin(115200);
  
  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize servo
  servo.attach(SERVO_PIN);
  servo.write(0);  // Initial position (locked)
  
  // Set motor pins as outputs
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(LEFT_ENA, OUTPUT);
  pinMode(LEFT_IN3, OUTPUT);
  pinMode(LEFT_IN4, OUTPUT);
  pinMode(LEFT_ENB, OUTPUT);
  
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(RIGHT_ENA, OUTPUT);
  pinMode(RIGHT_IN3, OUTPUT);
  pinMode(RIGHT_IN4, OUTPUT);
  pinMode(RIGHT_ENB, OUTPUT);
  
  // Set sensor pins
  pinMode(ULTRASONIC_TRIGGER, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);
  pinMode(MICROSWITCH_LEFT, INPUT_PULLUP);
  pinMode(MICROSWITCH_RIGHT, INPUT_PULLUP);
}

void loop() {
  // Read ultrasonic distance
  long duration;
  digitalWrite(ULTRASONIC_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIGGER, LOW);
  duration = pulseIn(ULTRASONIC_ECHO, HIGH);
  int distance = duration * 0.034 / 2;  // Convert to cm
  
  // Read IR sensors
  int ir_left = analogRead(IR_LEFT);
  int ir_right = analogRead(IR_RIGHT);
  
  // Read microswitches (assuming LOW when pressed)
  bool micro_left_pressed = (digitalRead(MICROSWITCH_LEFT) == LOW);
  bool micro_right_pressed = (digitalRead(MICROSWITCH_RIGHT) == LOW);
  
  // Check RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Unlock lid by rotating servo 90 degrees
    servo.write(90);
    delay(1000);  // Hold for 1 second
    servo.write(0);  // Return to locked position
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
  // Control motors based on sensor readings
  if (distance < 8 || micro_left_pressed || micro_right_pressed || 
      ir_left < IR_THRESHOLD || ir_right < IR_THRESHOLD) {
    stopMotors();
  } else {
    moveForward();
  }
  
  delay(100);  // Small delay to prevent excessive looping
}

void moveForward() {
  // Set left motors forward
  digitalWrite(LEFT_IN1, HIGH);
  digitalWrite(LEFT_IN2, LOW);
  digitalWrite(LEFT_IN3, HIGH);
  digitalWrite(LEFT_IN4, LOW);
  
  // Set right motors forward
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
  digitalWrite(RIGHT_IN3, HIGH);
  digitalWrite(RIGHT_IN4, LOW);
  
  // Set speed (adjust values as needed)
  analogWrite(LEFT_ENA, 200);
  analogWrite(LEFT_ENB, 200);
  analogWrite(RIGHT_ENA, 200);
  analogWrite(RIGHT_ENB, 200);
}

void stopMotors() {
  // Stop all motors
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
  digitalWrite(LEFT_IN3, LOW);
  digitalWrite(LEFT_IN4, LOW);
  
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
  digitalWrite(RIGHT_IN3, LOW);
  digitalWrite(RIGHT_IN4, LOW);
  
  analogWrite(LEFT_ENA, 0);
  analogWrite(LEFT_ENB, 0);
  analogWrite(RIGHT_ENA, 0);
  analogWrite(RIGHT_ENB, 0);
}