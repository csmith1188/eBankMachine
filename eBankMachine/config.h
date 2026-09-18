#pragma once

#include <Arduino.h>

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy eBankMachine/secrets.example.h to eBankMachine/secrets.h and fill in credentials."
#endif
#else
#include "secrets.h"
#endif

/* ----- I2C / LCD ----- */
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr uint8_t LCD_ADDR = 0x27;
constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;

/* ----- PN532 (I2C) ----- */
constexpr int PIN_PN532_IRQ = -1;
constexpr int PIN_PN532_RESET = -1;
constexpr uint16_t kNfcTimeoutMs = 100;
constexpr uint32_t kNfcPollMs = 60;
constexpr uint32_t kNfcHoldMs = 600;

/* ----- Sensors / actuators ----- */
constexpr int PIN_IR_DROP = 34;
constexpr int PIN_IR_DEP = 35;
constexpr int PIN_SERVO = 14;
constexpr int PIN_LIMIT = 23;
constexpr bool kLimitActiveLow = true;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
constexpr int PIN_LED = LED_BUILTIN;

/* ----- Keypad ----- */
constexpr uint8_t KEYPAD_ROWS = 4;
constexpr uint8_t KEYPAD_COLS = 4;

/* ----- Servo pulse widths ----- */
constexpr int kServoMinUs = 500;
constexpr int kServoMaxUs = 2400;
constexpr int kServoHz = 50;
constexpr int kServoNeutralUs = 1403;
constexpr int kServoDownUs = kServoNeutralUs + 250;
constexpr int kServoUpUs = 1500;

/* ----- Economy ----- */
constexpr int kDpogsPerPogWithdraw = 110;
constexpr int kDpogsPerPogDeposit = 90;
constexpr int kTransferTaxPercent = 5;
constexpr int kMaxCurrency = 22;
constexpr int kLowStockThreshold = 5;

/* ----- Timing ----- */
constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kIrSampleMs = 10;
constexpr uint32_t kDepSampleUs = 2000;
constexpr uint32_t kDropCooldownMs = 500;
constexpr uint32_t kDepCooldownMs = 150;
constexpr int kCalibReads = 80;
constexpr int kIrThresholdOffset = 300;
constexpr int kIrThresholdMax = 3900;
constexpr uint32_t kDepBeamMinMs = 2;
constexpr uint32_t kDepBeamMaxMs = 30;
constexpr uint32_t kDropArmMs = 200;
constexpr uint32_t kDepositArmMs = 100;
constexpr uint32_t kUnjamMs = 40000;
constexpr uint32_t kRefundRetryMs = 8000;
constexpr uint32_t kKeyWaitMs = 400;
constexpr uint32_t kKeyLockoutMs = 80;
constexpr uint32_t kMenuBChordMs = 800;
constexpr uint32_t kDebugChordMs = 5000;
constexpr uint32_t kWifiBootMs = 15000;
constexpr uint32_t kWifiRetryWaitMs = 12000;
constexpr uint32_t kHttpTimeoutMs = 10000;
constexpr uint32_t kHttpLookupTimeoutMs = 8000;

constexpr size_t kEntryCap = 9;
constexpr size_t kNameCap = 16;
constexpr size_t kLcdLineCap = 17;
