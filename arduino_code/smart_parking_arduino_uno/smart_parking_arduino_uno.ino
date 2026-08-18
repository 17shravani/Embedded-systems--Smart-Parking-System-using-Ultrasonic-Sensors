#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

/* =========================================================================
   PROJECT: Smart Parking System using Ultrasonic Sensors (Arduino UNO Edition)
   PLATFORM: Arduino UNO / Nano
   COMPONENTS:
     - 4x HC-SR04 Ultrasonic Sensors
     - 1x 16x2 I2C LCD Display (Address 0x27 or 0x3F)
     - 4x Green LEDs (Slots Free)
     - 4x Red LEDs (Slots Occupied)
     - 1x Active Buzzer
     - 1x SG90 Servo Motor (Entry Gate Barrier)
   ========================================================================= */

// LCD setup (16 chars, 2 rows at 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Servo
Servo gateServo;
const int SERVO_PIN = 9;

// Buzzer
const int BUZZER_PIN = 8;

// Pin Definitions for 4 Slots
// Slot 1
const int TRIG_1 = A0;
const int ECHO_1 = A1;
const int LED_G1 = 2;
const int LED_R1 = 3;

// Slot 2
const int TRIG_2 = A2;
const int ECHO_2 = A3;
const int LED_G2 = 4;
const int LED_R2 = 5;

// Slot 3
const int TRIG_3 = 6;
const int ECHO_3 = 7;
const int LED_G3 = 10;
const int LED_R3 = 11;

// Slot 4
const int TRIG_4 = 12;
const int ECHO_4 = 13;
// Note: We use digital pins carefully to leave I2C A4/A5 free for LCD
// Red and Green for Slot 4 can share pins or use multiplexer; on UNO we can use:
// Let's use direct pin mappings:
const float DISTANCE_THRESHOLD = 15.0; // cm for tabletop / slot model

struct SlotData {
  int trig;
  int echo;
  int greenPin;
  int redPin;
  bool isOccupied;
  float distanceCm;
};

SlotData slots[4] = {
  {A0, A1, 2, 3, false, 100.0},
  {A2, A3, 4, 5, false, 100.0},
  {6,  7,  10, 11, false, 100.0},
  {12, 13, 8, 9, false, 100.0} // Alternate mapping for 4-slot model
};

float measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return 300.0;
  return (float)duration / 58.2;
}

void setup() {
  Serial.begin(9600);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SMART PARKING");
  lcd.setCursor(0, 1);
  lcd.print("SYSTEM READY...");
  
  // Attach Servo
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Closed

  // Setup Slot Pins
  for (int i = 0; i < 3; i++) { // Default 3 or 4 slot pin initialization
    pinMode(slots[i].trig, OUTPUT);
    pinMode(slots[i].echo, INPUT);
    pinMode(slots[i].greenPin, OUTPUT);
    pinMode(slots[i].redPin, OUTPUT);
    digitalWrite(slots[i].greenPin, HIGH);
    digitalWrite(slots[i].redPin, LOW);
  }

  delay(1500);
  lcd.clear();
}

void loop() {
  int freeCount = 0;

  for (int i = 0; i < 3; i++) {
    float dist = measureDistance(slots[i].trig, slots[i].echo);
    slots[i].distanceCm = dist;

    if (dist < DISTANCE_THRESHOLD && dist > 1.0) {
      slots[i].isOccupied = true;
      digitalWrite(slots[i].greenPin, LOW);
      digitalWrite(slots[i].redPin, HIGH);
    } else {
      slots[i].isOccupied = false;
      digitalWrite(slots[i].greenPin, HIGH);
      digitalWrite(slots[i].redPin, LOW);
      freeCount++;
    }
    delay(20);
  }

  // Update LCD
  lcd.setCursor(0, 0);
  if (freeCount == 0) {
    lcd.print("PARKING FULL!   ");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    lcd.print("FREE SLOTS: ");
    lcd.print(freeCount);
    lcd.print(" / 3 ");
  }

  lcd.setCursor(0, 1);
  lcd.print("S1:");
  lcd.print(slots[0].isOccupied ? "O " : "F ");
  lcd.print("S2:");
  lcd.print(slots[1].isOccupied ? "O " : "F ");
  lcd.print("S3:");
  lcd.print(slots[2].isOccupied ? "O" : "F");

  // Gate Control
  if (freeCount > 0) {
    gateServo.write(90); // Open
  } else {
    gateServo.write(0);  // Close
  }

  delay(500);
}
