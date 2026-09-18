#pragma once

#include <Arduino.h>

void uiShow(const char* line0, const char* line1 = nullptr, uint32_t ms = 0);
void uiMenu();
void uiClearEntryLine();
void uiEntry(const __FlashStringHelper* prompt);
void uiAcceptDigit(char k, bool mask);
void uiConfirmId(long id, const char* name, const char* line1 = nullptr);
void uiConfirmWithdraw(long pogs);
void uiDepositScanning();
void uiNfcHold();
