#include "app.h"

#include "ui.h"
#include "web.h"
#include "drop.h"
#include "hardware.h"
#include "util.h"

#include <cstring>

static TradeMode gMode = TradeMode::Select;

static char gEntry[kEntryCap + 1] = {0};
static uint8_t gEntryLen = 0;

static char gName[kNameCap + 1] = {0};

TradeMode appMode() {
  return gMode;
}

void appSetMode(TradeMode mode) {
  gMode = mode;
}

void appGoMenu() {
  gMode = TradeMode::Select;
  Entry::reset();
  uiMenu();
}

const char* appModeName() {
  switch (gMode) {
    case TradeMode::Withdraw: return "withdraw";
    case TradeMode::Deposit: return "deposit";
    case TradeMode::StudentTransfer: return "stu-transfer";
    case TradeMode::NfcWrite: return "nfc-write";
    default: return "select";
  }
}

bool appBusy() {
  return dropActive() || hardwareRecovering() || webBusy();
}

namespace Entry {
  void reset() {
    gEntryLen = 0;
    gEntry[0] = '\0';
  }

  bool addDigit(char k) {
    if (k < '0' || k > '9') return false;
    if (gEntryLen >= kEntryCap) return false;
    gEntry[gEntryLen++] = k;
    gEntry[gEntryLen] = '\0';
    return true;
  }

  long value() {
    return parseLong(gEntry);
  }

  uint8_t length() {
    return gEntryLen;
  }

  const char* text() {
    return gEntry;
  }
}

void stashName(const char* name) {
  copyFit(gName, sizeof(gName), name);
  trimInPlace(gName);
}

const char* stashedName() {
  return gName;
}

bool chordCount(char expected, char pressed, uint8_t need, uint32_t windowMs,
                uint8_t& count, uint32_t& windowStart, uint32_t now) {
  if (pressed != expected) return false;

  if (count == 0 || (now - windowStart) > windowMs) {
    count = 0;
    windowStart = now;
  }

  count++;
  if (count < need) return false;

  count = 0;
  windowStart = 0;
  return true;
}

void coopDelay(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    webTick();
    delay(1);
  }
}
