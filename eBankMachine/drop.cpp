#include "eBankMachine.h"

void finishDrop(const char* why) {
  (void)why;

  servoStopDetach();
  motionState = MS_IDLE;

  // compute average BEFORE printing it
  unsigned long avgMs = 0;
  if (dbgDropTimingEnabled && dbgDropIntervals > 0) {
    avgMs = dbgDropSumIntervalsMs / dbgDropIntervals;
    dbgPrintf("DROP AVG: drops=%d intervals=%lu avg=%lums\n",
              (int)droppedCount, dbgDropIntervals, avgMs);
  }

  char line0[16], line1[16];
  snprintf(line0, sizeof(line0), "Dropped (%d)", (int)droppedCount);

  if (dbgDropTimingEnabled && dbgDropIntervals > 0) {
    snprintf(line1, sizeof(line1), "avg %lums", avgMs);
  } else {
    snprintf(line1, sizeof(line1), "pog%s", ((int)droppedCount) == 1 ? "" : "s");
  }

  showMsg(line0, line1, 1200);

  targetDrops = 0;
  droppedCount = 0;

  dbgDropTimingEnabled = false;
  dbgDropAllAutoStop = false;

  tradeMode = MODE_SELECT;
  showModeMenu();
}

void startDrop(int count) {
  if (count <= 0) return;
  if (motionState != MS_IDLE) return;

  // reset timing stats when enabled
  if (dbgDropTimingEnabled) {
    dbgDropLastEventMs = 0;
    dbgDropSumIntervalsMs = 0;
    dbgDropIntervals = 0;
  }

  if (limitSwitchPressed) {
    showMsg("Cannot drop", "Limit pressed", 900);
    return;
  }

  targetDrops = count;
  droppedCount = 0;
  nextCountAllowedAt = 0;
  irWasAbove = false;
  dropStartMs = millis();

  servoAttach();
  myServo.writeMicroseconds(SERVO_DOWN_US);
  motionState = MS_DROPPING;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Dropping: "));
  lcd.print((int)targetDrops);
  lcd.setCursor(0, 1);
  lcd.print(F("Done: 0/"));
  lcd.print((int)targetDrops);

  dbgPrintf("Drop start target=%d\n", count);
}

void dropTick() {
  if (motionState != MS_DROPPING) return;

  if (limitSwitchPressed) {
    finishDrop("limit");
    currencyCount = MAX_CURRENCY_CAPACITY;
    return;
  }

  unsigned long now = millis();
  if (now - irLastSample < IR_SAMPLE_MS) return;
  irLastSample = now;

  int v = analogRead(IR_DROP_PIN);
  bool above = (v > IR_DROP_THRESHOLD);
  bool armed = (now - dropStartMs > 200);

  if (armed && above && !irWasAbove && now >= nextCountAllowedAt) {
    droppedCount++;
    if (dbgDropTimingEnabled) {
      if (dbgDropLastEventMs != 0) {
        unsigned long dt = now - dbgDropLastEventMs;
        dbgDropSumIntervalsMs += dt;
        dbgDropIntervals++;
        dbgPrintf("DROP timing #%d dt=%lums\n", (int)droppedCount, dt);
      } else {
        dbgPrintf("DROP timing #1\n");
      }
      dbgDropLastEventMs = now;
    }
    nextCountAllowedAt = now + DROP_COOLDOWN_MS;

    lcd.setCursor(6, 1);
    lcd.print((int)droppedCount);
    lcd.print(F("/"));
    lcd.print((int)targetDrops);

    if ((int)droppedCount >= (int)targetDrops) {
      currencyCount -= droppedCount;

      // Prevent negative inventory
      if (currencyCount < 0) currencyCount = 0;

      finishDrop("done");
    }
  }

  irWasAbove = above;
}