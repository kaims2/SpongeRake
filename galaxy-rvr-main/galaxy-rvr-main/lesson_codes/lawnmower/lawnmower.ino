#include <SoftPWM.h>

// Motor pins
const int in1 = 2;
const int in2 = 3;
const int in3 = 4;
const int in4 = 5;

// Ultrasonic pin (shared trig/echo)
#define ULTRASONIC_PIN 10

// Tuning
#define WALL_THRESHOLD 20.0  // cm — how close before turning
#define DRIVE_SPEED 200
#define TURN_DURATION 600    // ms — how long to turn 180 degrees (adjust to your surface)
#define SHIFT_DURATION 300   // ms — lateral shift so the next pass is offset

// Lawnmower states
enum LawnState {
  DRIVING_FORWARD,
  SHIFTING,
  TURNING,
};

LawnState state = DRIVING_FORWARD;
int direction = 1;            // 1 = turn right, -1 = turn left (alternates each pass)
unsigned long stateTimer = 0;

void setup() {
  SoftPWMBegin();
  Serial.begin(9600);
  Serial.println("Lawnmower mode started");
}

void loop() {
  switch (state) {
    case DRIVING_FORWARD:
      moveForward(DRIVE_SPEED);
      if (readUltrasonic() < WALL_THRESHOLD && readUltrasonic() > 0) {
        stopMove();
        state = SHIFTING;
        stateTimer = millis();
      }
      break;

    case SHIFTING:
      // Small lateral shift before turning so the next row is offset
      if (direction == 1)
        turnRight(DRIVE_SPEED);
      else
        turnLeft(DRIVE_SPEED);

      if (millis() - stateTimer >= SHIFT_DURATION) {
        stopMove();
        state = TURNING;
        stateTimer = millis();
      }
      break;

    case TURNING:
      // Continue the same turn to complete ~180 degrees
      if (direction == 1)
        turnRight(DRIVE_SPEED);
      else
        turnLeft(DRIVE_SPEED);

      if (millis() - stateTimer >= TURN_DURATION) {
        stopMove();
        direction *= -1;  // alternate turn direction for next wall
        state = DRIVING_FORWARD;
      }
      break;
  }
}

// ---- Ultrasonic ----
float readUltrasonic() {
  delay(4);
  pinMode(ULTRASONIC_PIN, OUTPUT);
  digitalWrite(ULTRASONIC_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_PIN, LOW);
  pinMode(ULTRASONIC_PIN, INPUT);

  float duration = pulseIn(ULTRASONIC_PIN, HIGH, 18000);
  float distance = duration * 0.017;
  if (distance > 300 || distance == 0) return -1;
  return distance;
}

// ---- Motor helpers ----
void moveForward(int speed) {
  SoftPWMSet(in1, speed);
  SoftPWMSet(in2, 0);
  SoftPWMSet(in3, 0);
  SoftPWMSet(in4, speed);
}

void moveBackward(int speed) {
  SoftPWMSet(in1, 0);
  SoftPWMSet(in2, speed);
  SoftPWMSet(in3, speed);
  SoftPWMSet(in4, 0);
}

void turnLeft(int speed) {
  SoftPWMSet(in1, 0);
  SoftPWMSet(in2, speed);
  SoftPWMSet(in3, 0);
  SoftPWMSet(in4, speed);
}

void turnRight(int speed) {
  SoftPWMSet(in1, speed);
  SoftPWMSet(in2, 0);
  SoftPWMSet(in3, speed);
  SoftPWMSet(in4, 0);
}

void stopMove() {
  SoftPWMSet(in1, 0);
  SoftPWMSet(in2, 0);
  SoftPWMSet(in3, 0);
  SoftPWMSet(in4, 0);
}
