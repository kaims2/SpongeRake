/*******************************************************************
  The control program of the Ardunio GalaxyRVR.
  ...
********************************************************************/
#define VERSION "1.1.0"

#include <Arduino.h>
#include <SoftPWM.h>
#include <string.h>


#include "rgb.h"
#include "soft_servo.h"
#include "car_control.h"
#include "ir_obstacle.h"
#include "ultrasonic.h"
#include "cmd_code_config.hpp"
#include "SunFounder_AI_Camera.h"
#include "battery.h"
/*************************** Configure *******************************/
/** @name Configure 
 * 
 */
///@{
/** Whether to enable Watchdog */
#define WATCH_DOG 0
#if WATCH_DOG
  #include <avr/wdt.h>
#endif

#define TEST 0
#if TEST
  #include "test.h"
#endif

#define MEM 0
#if MEM
  #include <MemoryFree.h>
  #include <pgmStrToRAM.h> 
#endif

#define WIFI_MODE WIFI_MODE_AP
#define SSID "GalaxyRVR"
#define PASSWORD "12345678"

#define NAME "GalaxyRVR"
#define TYPE "GalaxyRVR"
#define PORT "8765"

#define OBSTACLE_AVOID_POWER 80
#define OBSTACLE_FOLLOW_POWER 80
#define VOICE_CONTROL_POWER 80
#define FOLLOW_DISTANCE 20
#define WS_HEADER "WS+"

// Define the new mode and its color
#define MODE_SPIRAL 6 
#define MODE_SPIRAL_COLOR PURPLE

#define MODE_LAWNMOWER 7
#define MODE_LAWNMOWER_COLOR BLUE
#define LAWNMOWER_SPEED 60
#define LAWNMOWER_WALL_THRESHOLD 20.0
#define LAWNMOWER_SHIFT_TIME 300
#define LAWNMOWER_TURN_TIME 600

/*********************** Global variables ****************************/
AiCamera aiCam = AiCamera(NAME, TYPE);
SoftServo servo;

#define SERVO_PIN 6
#define SERVO_REVERSE false

char voice_buf_temp[20];
int8_t current_voice_code = -1;
int32_t voice_time = 0; 
uint32_t voice_start_time = 0; 

int8_t leftMotorPower = 0;
int8_t rightMotorPower = 0;
uint8_t servoAngle = 90;

uint32_t rgb_blink_interval = 500; 
uint32_t rgb_blink_start_time = 0;
bool rgb_blink_flag = 0;
bool cam_lamp_status = false;

// Variables for Spiral Movement Mode (Scaled for 0-100 range)
int spiralBaseSpeed = 60;  
int spiralTurnSpeed = 20;   
int spiralTurnIncrement = 2; 
int spiralMaxTurnSpeed = 80; 
int spiralState = 0; // 0: Normal, 1: Backing up, 2: Turning
unsigned long spiralStateTimer = 0;
unsigned long lastSpiralIncrementTime = 0;

// Variables for Lawnmower Mode
// 0: driving forward, 1: shifting, 2: turning
int lawnmowerState = 0;
int lawnmowerDirection = 1; // 1 = turn right, -1 = turn left (alternates)
unsigned long lawnmowerStateTimer = 0;


/*********************** setup() & loop() ************************/
void setup() {
  int m = millis();
  Serial.begin(115200);
  Serial.print("GalaxyRVR version "); Serial.println(VERSION);
  Serial.println(F("Initialzing..."));

#if defined(ARDUINO_AVR_UNO)
  SoftPWMBegin(); 
#endif
  rgbBegin();
  rgbWrite(ORANGE); 
  carBegin();
  irObstacleBegin();
  batteryBegin();
  servo.attach(SERVO_PIN);
  servo.write(90);

#if !TEST
  aiCam.begin(SSID, PASSWORD, WIFI_MODE, PORT);
  aiCam.setOnReceived(onReceive);
#endif

  while (millis() - m < 500) {  
    delay(1);
  }

#if WATCH_DOG
  wdt_disable();       
  delay(3000);         
  wdt_enable(WDTO_2S); 
#endif

  Serial.println(F("Okie!"));
  rgbWrite(GREEN);  
}

void loop() {
#if !TEST
  aiCam.loop();
  if (aiCam.ws_connected == false) {
    currentMode = MODE_DISCONNECT;
    int8_t current_voice_code = -1;
    int8_t voice_time = 0;
    if (currentMode != MODE_DISCONNECT) {
      rgb_blink_start_time = 0;
      rgb_blink_flag = 1;
    }
  } else {
    if (currentMode == MODE_DISCONNECT) currentMode = MODE_NONE;
  }
  modeHandler();
#else
  motors_test();
#endif

#if WATCH_DOG
  wdt_reset(); 
#endif

#if MEM
  Serial.print(F("Free RAM = "));  
  Serial.println(freeMemory());    
#endif
}

/***************************** Functions ******************************/
void modeHandler() {
  switch (currentMode) {
    case MODE_NONE:
      rgbWrite(MODE_NONE_COLOR);
      carStop();
      servoAngle = 90;
      servo.write(servoAngle);
      break;
    case MODE_DISCONNECT:
      if (millis() - rgb_blink_start_time > rgb_blink_interval) {
        rgb_blink_flag = !rgb_blink_flag;
        rgb_blink_start_time = millis();
      }
      if (rgb_blink_flag) rgbWrite(MODE_DISCONNECT_COLOR);
      else rgbOff();
      carStop();
      servoAngle = 90;
      servo.write(servoAngle);
      break;
    case MODE_OBSTACLE_FOLLOWING:
      rgbWrite(MODE_OBSTACLE_FOLLOWING_COLOR);
      servo.write(servoAngle);
      obstacleFollowing();
      break;
    case MODE_OBSTACLE_AVOIDANCE:
      rgbWrite(MODE_OBSTACLE_AVOIDANCE_COLOR);
      servo.write(servoAngle);
      obstacleAvoidance();
      break;
    case MODE_APP_CONTROL:
      rgbWrite(MODE_APP_CONTROL_COLOR);
      servo.write(servoAngle);
      carSetMotors(leftMotorPower, rightMotorPower);
      break;
    case MODE_VOICE_CONTROL:
      rgbWrite(MODE_VOICE_CONTROL_COLOR);
      servo.write(servoAngle);
      voice_control();
      break;
    case MODE_SPIRAL:
      rgbWrite(MODE_SPIRAL_COLOR);
      servo.write(servoAngle);
      spiralMovement();
      break;
    case MODE_LAWNMOWER:
      rgbWrite(MODE_LAWNMOWER_COLOR);
      servo.write(servoAngle);
      lawnmowerMovement();
      break;
    default:
      break;
  }
}

/**
 * Spiral movement and obstacle avoidance program (Non-blocking)
 */
void spiralMovement() {
  unsigned long currentMillis = millis();

  // 1. Handle ongoing backing up / turning maneuvers without using delay()
  if (spiralState == 1) { // Backing up
    if (currentMillis - spiralStateTimer >= 500) {
      spiralState = 2; // Transition to turn
      spiralStateTimer = currentMillis;
    } else {
      carSetMotors(-spiralBaseSpeed, -spiralBaseSpeed); // Move backward
    }
    return;
  } else if (spiralState == 2) { // Turning Left
    if (currentMillis - spiralStateTimer >= 500) {
      spiralState = 0; // Transition back to normal spiral
    } else {
      carTurnLeft(spiralBaseSpeed);
    }
    return;
  }

  // 2. Normal checking and pathfinding
  byte result = irObstacleRead();
  bool leftIsClear = result & 0b00000010;
  bool rightIsClear = result & 0b00000001;
  float distance = ultrasonicRead();

  // Check for obstacles
  if ((distance > 0 && distance < 10) || (!rightIsClear && !leftIsClear)) {
    // Start obstacle maneuver sequence
    spiralState = 1;
    spiralStateTimer = currentMillis;
    carSetMotors(-spiralBaseSpeed, -spiralBaseSpeed); 
  } else if (!rightIsClear) {
    carTurnLeft(spiralBaseSpeed);
  } else if (!leftIsClear) {
    carTurnRight(spiralBaseSpeed);
  } else {
    // No obstacles, move in a spiral pattern
    carSetMotors(spiralBaseSpeed, spiralBaseSpeed - spiralTurnSpeed);
    
    // Increment the turn speed every 500ms
    if (currentMillis - lastSpiralIncrementTime > 500) {
      spiralTurnSpeed += spiralTurnIncrement;
      if (spiralTurnSpeed > spiralMaxTurnSpeed) {
        spiralTurnSpeed = 20;  // Reset turning speed
      }
      lastSpiralIncrementTime = currentMillis;
    }
  }
}

/**
 * Lawnmower pattern: drive forward until wall, shift over, turn 180, repeat
 */
void lawnmowerMovement() {
  unsigned long currentMillis = millis();

  switch (lawnmowerState) {
    case 0: // Driving forward
    {
      float distance = ultrasonicRead();
      if (distance > 0 && distance < LAWNMOWER_WALL_THRESHOLD) {
        carStop();
        lawnmowerState = 1;
        lawnmowerStateTimer = currentMillis;
      } else {
        carForward(LAWNMOWER_SPEED);
      }
      break;
    }
    case 1: // Shifting sideways
      if (lawnmowerDirection == 1)
        carTurnRight(LAWNMOWER_SPEED);
      else
        carTurnLeft(LAWNMOWER_SPEED);

      if (currentMillis - lawnmowerStateTimer >= LAWNMOWER_SHIFT_TIME) {
        carStop();
        lawnmowerState = 2;
        lawnmowerStateTimer = currentMillis;
      }
      break;

    case 2: // Completing 180-degree turn
      if (lawnmowerDirection == 1)
        carTurnRight(LAWNMOWER_SPEED);
      else
        carTurnLeft(LAWNMOWER_SPEED);

      if (currentMillis - lawnmowerStateTimer >= LAWNMOWER_TURN_TIME) {
        carStop();
        lawnmowerDirection *= -1;
        lawnmowerState = 0;
      }
      break;
  }
}

void obstacleFollowing() {
  byte result = irObstacleRead();
  bool leftIsClear = result & 0b00000010;
  bool rightIsClear = result & 0b00000001;
  float usDistance = ultrasonicRead();
  if (usDistance < 4 && usDistance > 0) {
    carStop();
  } else if (usDistance < 10 && usDistance > 0) {
    carForward(30);
  } else if (usDistance < FOLLOW_DISTANCE && usDistance > 0) {
    carForward(OBSTACLE_FOLLOW_POWER);
  } else {
    if (!leftIsClear) {
      carTurnLeft((int8_t)OBSTACLE_FOLLOW_POWER);
    } else if (!rightIsClear) {
      carTurnRight(OBSTACLE_FOLLOW_POWER);
    } else {
      carStop();
    }
  }
}

int8_t last_clear = -1;  
bool last_forward = false;

void obstacleAvoidance() {
  // ... (Original logic preserved)
  byte result = irObstacleRead();
  bool leftIsClear = result & 0b00000010;   
  bool rightIsClear = result & 0b00000001;  
  bool middleIsClear = ultrasonicIsClear();

  if (middleIsClear && leftIsClear && rightIsClear) {  
    last_forward = true;
    carForward(OBSTACLE_AVOID_POWER);
  } else {
    if ((leftIsClear && rightIsClear) || (!leftIsClear && !rightIsClear)) {  
      if (last_clear == 1) carTurnLeft(OBSTACLE_AVOID_POWER);
      else carTurnRight(OBSTACLE_AVOID_POWER);
      last_forward = false;
    } else if (leftIsClear) {  
      if (last_clear == 1 || last_forward == true) {
        carTurnLeft(OBSTACLE_AVOID_POWER);
        last_clear = 1;
        last_forward = false;
      }
    } else if (rightIsClear) {  
      if (last_clear == -1 || last_forward == true) {
        carTurnRight(OBSTACLE_AVOID_POWER);
        last_clear = -1;
        last_forward = false;
      }
    }
  }
}

void voice_control() {
  // ... (Original logic preserved)
  if (voice_time == -1) {
    voice_action(current_voice_code, VOICE_CONTROL_POWER);
  } else {
    if (millis() - voice_start_time <= voice_time) {
      voice_action(current_voice_code, VOICE_CONTROL_POWER);
    } else {
      currentMode = MODE_NONE;
      voice_start_time = 0;
      current_voice_code = -1;
    }
  }
}


void onReceive() {
  // --------------------- send data ---------------------
  // battery voltage
  // Serial.print(F("voltage:"));Serial.println(batteryGetVoltage());
  aiCam.sendDoc["BV"] = batteryGetVoltage();

  // IR obstacle detection data
  byte result = irObstacleRead();
  aiCam.sendDoc["N"] = int(!bool(result & 0b00000010));  // left, clear:0
  aiCam.sendDoc["P"] = int(!bool(result & 0b00000001));  // right, clear:0

  // ultrasonic
  float usDistance = int(ultrasonicRead() * 100) / 100.0;  // round two decimal places
  aiCam.sendDoc["O"] = usDistance;

  // --------------------- get data ---------------------
  // Stop
  if (aiCam.getButton(REGION_I)) {
    currentMode = MODE_NONE;
    current_voice_code = -1;
    voice_time = 0;
    carStop();
    return;
  }

  // Mode select: obstacle following, obstacle avoidance
  if (aiCam.getSwitch(REGION_E)) {
    if (currentMode != MODE_OBSTACLE_AVOIDANCE) {
      currentMode = MODE_OBSTACLE_AVOIDANCE;
    }
  } else if (aiCam.getSwitch(REGION_F)) {
    if (currentMode != MODE_OBSTACLE_FOLLOWING) {
      currentMode = MODE_OBSTACLE_FOLLOWING;
    }
  } 
    else if (aiCam.getSwitch(REGION_G)) {
    if (currentMode != MODE_SPIRAL) {
      currentMode = MODE_SPIRAL;
      spiralState = 0;
    }
  } else if (aiCam.getSwitch(REGION_C)) {
    if (currentMode != MODE_LAWNMOWER) {
      currentMode = MODE_LAWNMOWER;
      lawnmowerState = 0;
      lawnmowerDirection = 1;
    }
  } else {
    if (currentMode == MODE_OBSTACLE_FOLLOWING || currentMode == MODE_OBSTACLE_AVOIDANCE || currentMode == MODE_SPIRAL || currentMode == MODE_LAWNMOWER) {
      currentMode = MODE_NONE;
      carStop();
      return;
    }
  }

  // cam lamp
  if (aiCam.getSwitch(REGION_M) && !cam_lamp_status) {
    Serial.println("lamp on");
    aiCam.lamp_on(5);  //turn on cam lamp, level 0 ~ 10 
    cam_lamp_status = true;
  } else if (!aiCam.getSwitch(REGION_M) && cam_lamp_status) {
    Serial.println("lamp off");
    aiCam.lamp_off();  // turn off cam lamp
    cam_lamp_status = false;
  }

  // Speech control
  if (currentMode != MODE_VOICE_CONTROL) {
    current_voice_code = -1;
    voice_time = 0;
    voice_start_time = 0;
    aiCam.sendDoc["J"] = 0;
  }

  int8_t code = -1;
  voice_buf_temp[0] = 0;  // voice_buf_temp
  aiCam.getSpeech(REGION_J, voice_buf_temp);
  if (strlen(voice_buf_temp) > 0) {
    aiCam.sendDoc["J"] = 1;
    aiCam.sendData();
    aiCam.sendDoc["J"] = 0;
    code = text_2_cmd_code(voice_buf_temp);
    if (code != -1) {
      current_voice_code = code;
      voice_time = voice_action_time[code];
      voice_start_time = millis();
    }
  }

  if (current_voice_code != -1) {
    currentMode = MODE_VOICE_CONTROL;
  }

  // servo angle
  int temp = aiCam.getSlider(REGION_D);
  if (servoAngle != temp) {
    if (currentMode == MODE_NONE || currentMode == MODE_DISCONNECT) {
      currentMode = MODE_APP_CONTROL;
    }
    if (SERVO_REVERSE) {
      temp = constrain(temp, 40, 180);
      temp = 180 - temp;
    } else {
      temp = constrain(temp, 0, 140);
    }
    servoAngle = temp;
  }

  // throttle
  int throttle_L = aiCam.getThrottle(REGION_K);
  int throttle_R = aiCam.getThrottle(REGION_Q);
  // Serial.print("throttle_L: "); Serial.print(throttle_L);
  // Serial.print("throttle_R: "); Serial.println(throttle_R);
  if (throttle_L != 0 || throttle_R != 0 || throttle_L != leftMotorPower || throttle_R != rightMotorPower) {
    currentMode = MODE_APP_CONTROL;
    leftMotorPower = throttle_L;
    rightMotorPower = throttle_R;
  }
}
