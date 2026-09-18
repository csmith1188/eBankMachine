#include "hardware.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "drop.h"
#include "inventory.h"
#include "refund.h"
#include "ui.h"
#include "util.h"
#include "withdraw.h"

#include <Wire.h>

static char KEYS[KEYPAD_ROWS][KEYPAD_COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
static byte rowPins[KEYPAD_ROWS] = { 19, 18, 33, 32 };
static byte colPins[KEYPAD_COLS] = { 25, 26, 27, 13 };

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
Servo hopperServo;
Adafruit_PN532 nfc(PIN_PN532_IRQ, PIN_PN532_RESET);
Keypad keypad = Keypad(makeKeymap(KEYS), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

int irDropThreshold = 0;
int irDepThreshold = 0;
bool limitSwitchPressed = false;

static bool servoAttached = false;
static int lastReading = LOW;
static int stableState = LOW;
static uint32_t lastChangeMs = 0;
static bool prevLimitPressed = false;

static bool recovering = false;
static uint32_t recoverUntilMs = 0;
static bool refundAfterRecover = false;

void servoAttach() {
  if (servoAttached) return;
  hopperServo.setPeriodHertz(kServoHz);
  hopperServo.attach(PIN_SERVO, kServoMinUs, kServoMaxUs);
  servoAttached = true;
}

void servoStop() {
  if (!servoAttached) return;
  hopperServo.writeMicroseconds(kServoNeutralUs);
  delay(10);
  hopperServo.detach();
  servoAttached = false;
}

void servoWriteUs(int us) {
  servoAttach();
  hopperServo.writeMicroseconds(us);
}

static void calibrateIrPin(int pin, int& thrOut) {
  long sum = 0;
  coopDelay(200);
  for (int i = 0; i < kCalibReads; i++) {
    sum += analogRead(pin);
    coopDelay(25);
  }
  const int baseline = (int)(sum / kCalibReads);
  thrOut = clampi(baseline + kIrThresholdOffset, 0, kIrThresholdMax);
}

void irCalibrate() {
  uiShow("IR Calibrating", "Keep chutes clear");
  calibrateIrPin(PIN_IR_DROP, irDropThreshold);
  calibrateIrPin(PIN_IR_DEP, irDepThreshold);
  dbgPrintf("IR thr drop=%d dep=%d\n", irDropThreshold, irDepThreshold);
}

static void startUnjam() {
  uiShow("LIMIT HIT", "UNJAM UP 40s");
  servoWriteUs(kServoUpUs);
  recovering = true;
  recoverUntilMs = millis() + kUnjamMs;
}

static void handleLimitPressed() {
  dbgPrintf("LIMIT pressed\n");

  if (dropActive()) {
    const bool allowRefund = (appMode() == TradeMode::Withdraw) && !dropIsDebugAll();
    if (allowRefund) {
      const int remainingPogs = dropRemaining();
      const int remainingDpogs = remainingPogs * kDpogsPerPogWithdraw;
      const long fromId = withdrawFromId();
      if (remainingDpogs > 0 && fromId > 0) {
        refundQueue(fromId, remainingDpogs);
        refundAfterRecover = true;
      } else {
        refundClear();
        refundAfterRecover = false;
      }
    } else {
      refundClear();
      refundAfterRecover = false;
    }

    dropAbort();
    inventoryRefill();
  }

  startUnjam();
}

static void finishRecovery() {
  servoStop();
  recovering = false;

  if (refundAfterRecover && refundIsPending()) {
    uiShow("Refunding...", "Please wait");
    if (refundTryNow()) {
      uiShow("Refund SENT", "OK", 1500);
    } else {
      refundScheduleRetry();
      uiShow("Refund FAILED", "Auto retry...", 1800);
    }
  } else {
    uiShow("Recovered", "Ready", 1200);
  }

  refundAfterRecover = false;
  appGoMenu();
}

bool hardwareRecovering() {
  return recovering;
}

void hardwareTick() {
  const int reading = digitalRead(PIN_LIMIT);
  if (reading != lastReading) {
    lastChangeMs = millis();
    lastReading = reading;
  }

  if (millis() - lastChangeMs > kDebounceMs && reading != stableState) {
    stableState = reading;
    limitSwitchPressed = kLimitActiveLow ? (stableState == LOW) : (stableState == HIGH);
  }

  digitalWrite(PIN_LED, limitSwitchPressed ? HIGH : LOW);

  if (limitSwitchPressed && !prevLimitPressed) {
    prevLimitPressed = true;
    handleLimitPressed();
  } else if (!limitSwitchPressed && prevLimitPressed) {
    prevLimitPressed = false;
  }

  if (recovering && (int32_t)(millis() - recoverUntilMs) >= 0) {
    finishRecovery();
  }
}

void hardwareInit() {
  analogReadResolution(12);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_LIMIT, kLimitActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);

  lastReading = digitalRead(PIN_LIMIT);
  stableState = lastReading;
  limitSwitchPressed = kLimitActiveLow ? (stableState == LOW) : (stableState == HIGH);
  prevLimitPressed = limitSwitchPressed;

  keypad.setDebounceTime(15);
  keypad.getKeys();
  delay(20);
  keypad.getKeys();
  for (int i = 0; i < LIST_MAX; i++) {
    const KeyState s = keypad.key[i].kstate;
    if (s == PRESSED || s == HOLD) {
      dbgPrintf("KEYPAD held at boot: %c\n", keypad.key[i].kchar);
    }
  }

  Wire.begin(PIN_SDA, PIN_SCL);
  delay(40);

  lcd.init();
  lcd.backlight();
  uiShow("BOOTING...", nullptr, 500);

  inventoryLoad();

  nfc.begin();
  const uint32_t ver = nfc.getFirmwareVersion();
  if (ver) {
    nfc.SAMConfig();
    dbgPrintf("PN532 OK fw=%lX\n", (unsigned long)ver);
  } else {
    dbgPrintf("PN532 NOT FOUND\n");
  }

  irCalibrate();

  if (limitSwitchPressed) {
    handleLimitPressed();
    while (hardwareRecovering()) {
      hardwareTick();
      delay(1);
    }
  }
}
