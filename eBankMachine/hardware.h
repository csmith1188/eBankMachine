#pragma once

#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Adafruit_PN532.h>
#include <Keypad.h>

extern LiquidCrystal_I2C lcd;
extern Servo hopperServo;
extern Adafruit_PN532 nfc;
extern Keypad keypad;

extern int irDropThreshold;
extern int irDepThreshold;
extern bool limitSwitchPressed;

void hardwareInit();
void hardwareTick();
bool hardwareRecovering();

void servoAttach();
void servoStop();
void servoWriteUs(int us);

void irCalibrate();
