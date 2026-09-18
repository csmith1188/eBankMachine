#pragma once

#include <Arduino.h>
#include "config.h"

enum class TradeMode : uint8_t {
  Select,
  Withdraw,
  Deposit,
  StudentTransfer,
  NfcWrite
};

TradeMode appMode();
void appSetMode(TradeMode mode);
void appGoMenu();
const char* appModeName();
bool appBusy();

namespace Entry {
  void reset();
  bool addDigit(char k);
  long value();
  uint8_t length();
  const char* text();
}

void stashName(const char* name);
const char* stashedName();

bool chordCount(char expected, char pressed, uint8_t need, uint32_t windowMs,
                uint8_t& count, uint32_t& windowStart, uint32_t now);

void coopDelay(uint32_t ms);
