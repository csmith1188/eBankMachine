#include "drop.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "hardware.h"
#include "inventory.h"
#include "ui.h"

static bool active = false;
static int target = 0;
static int dropped = 0;
static uint32_t nextCountAt = 0;
static bool wasAbove = false;
static uint32_t startMs = 0;
static uint32_t lastSampleMs = 0;

static bool timingEnabled = false;
static bool debugAll = false;
static uint32_t lastEventMs = 0;
static uint32_t sumIntervalsMs = 0;
static uint32_t intervals = 0;

bool dropActive() {
  return active;
}

bool dropIsDebugAll() {
  return debugAll;
}

int dropTarget() {
  return target;
}

int dropDropped() {
  return dropped;
}

int dropRemaining() {
  const int left = target - dropped;
  return left > 0 ? left : 0;
}

void dropAbort() {
  servoStop();
  active = false;
  target = 0;
  dropped = 0;
  timingEnabled = false;
  debugAll = false;
}

static void finishDrop() {
  servoStop();
  active = false;

  uint32_t avgMs = 0;
  if (timingEnabled && intervals > 0) {
    avgMs = sumIntervalsMs / intervals;
    dbgPrintf("DROP AVG: drops=%d intervals=%lu avg=%lums\n",
              dropped, (unsigned long)intervals, (unsigned long)avgMs);
  }

  char line0[kLcdLineCap];
  char line1[kLcdLineCap];
  snprintf(line0, sizeof(line0), "Dropped (%d)", dropped);
  if (timingEnabled && intervals > 0) snprintf(line1, sizeof(line1), "avg %lums", (unsigned long)avgMs);
  else snprintf(line1, sizeof(line1), "pog%s", dropped == 1 ? "" : "s");

  uiShow(line0, line1, 1200);

  target = 0;
  dropped = 0;
  timingEnabled = false;
  debugAll = false;
  appGoMenu();
}

void dropStart(int count, bool timing, bool isDebugAll) {
  if (count <= 0 || active) return;

  timingEnabled = timing;
  debugAll = isDebugAll;
  if (timingEnabled) {
    lastEventMs = 0;
    sumIntervalsMs = 0;
    intervals = 0;
  }

  if (limitSwitchPressed) {
    uiShow("Cannot drop", "Limit pressed", 900);
    timingEnabled = false;
    debugAll = false;
    return;
  }

  target = count;
  dropped = 0;
  nextCountAt = 0;
  wasAbove = false;
  startMs = millis();
  lastSampleMs = 0;

  servoWriteUs(kServoDownUs);
  active = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Dropping: "));
  lcd.print(target);
  lcd.setCursor(0, 1);
  lcd.print(F("Done: 0/"));
  lcd.print(target);

  dbgPrintf("Drop start target=%d\n", count);
}

void dropTick() {
  if (!active) return;

  const uint32_t now = millis();
  if (now - lastSampleMs < kIrSampleMs) return;
  lastSampleMs = now;

  const int v = analogRead(PIN_IR_DROP);
  const bool above = (v > irDropThreshold);
  const bool armed = (now - startMs > kDropArmMs);

  if (armed && above && !wasAbove && now >= nextCountAt) {
    dropped++;
    if (timingEnabled) {
      if (lastEventMs != 0) {
        const uint32_t dt = now - lastEventMs;
        sumIntervalsMs += dt;
        intervals++;
        dbgPrintf("DROP timing #%d dt=%lums\n", dropped, (unsigned long)dt);
      } else {
        dbgPrintf("DROP timing #1\n");
      }
      lastEventMs = now;
    }

    nextCountAt = now + kDropCooldownMs;

    lcd.setCursor(6, 1);
    lcd.print(dropped);
    lcd.print(F("/"));
    lcd.print(target);

    if (dropped >= target) {
      inventoryConsume(dropped);
      finishDrop();
    }
  }

  wasAbove = above;
}
