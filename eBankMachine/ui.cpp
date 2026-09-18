#include "ui.h"

#include "app.h"
#include "config.h"
#include "hardware.h"
#include "util.h"

void uiShow(const char* line0, const char* line1, uint32_t ms) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (line0) lcd.print(line0);
  if (line1) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }
  if (ms) coopDelay(ms);
}

void uiMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("    Welcome!    "));
  lcd.setCursor(0, 1);
  lcd.print(F(" Select a mode"));
}

void uiClearEntryLine() {
  lcd.setCursor(7, 1);
  for (int j = 0; j < 9; j++) lcd.print(' ');
  lcd.setCursor(7, 1);
  Entry::reset();
}

void uiEntry(const __FlashStringHelper* prompt) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  lcd.setCursor(0, 1);
  lcd.print(F("Enter:       "));
  uiClearEntryLine();
}

void uiAcceptDigit(char k, bool mask) {
  if (!Entry::addDigit(k)) return;
  lcd.setCursor(7 + (Entry::length() - 1), 1);
  lcd.print(mask ? '*' : k);
}

void uiConfirmId(long id, const char* name, const char* line1) {
  char l0[kLcdLineCap];
  char l1[kLcdLineCap];
  char nm[kNameCap + 1];
  copyFit(nm, sizeof(nm), name);

  if (nm[0]) snprintf(l0, sizeof(l0), "ID %ld %s", id, nm);
  else snprintf(l0, sizeof(l0), "ID %ld", id);

  if (line1 && line1[0]) copyFit(l1, sizeof(l1), line1);
  else snprintf(l1, sizeof(l1), "*=No  #=Yes");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l0);
  lcd.setCursor(0, 1);
  lcd.print(l1);
}

void uiConfirmWithdraw(long pogs) {
  const long digipogs = pogs * kDpogsPerPogWithdraw;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Use "));
  lcd.print(digipogs);
  lcd.print(F(" dpogs?"));
  lcd.setCursor(0, 1);
  lcd.print(F("*=No   #=Yes"));
}

void uiDepositScanning() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Deposit mode"));
  lcd.setCursor(0, 1);
  lcd.print(F("Count: 0  #=done"));
}

void uiNfcHold() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("HOLD CARD..."));
  lcd.setCursor(0, 1);
  lcd.print(F("*=cancel"));
}
