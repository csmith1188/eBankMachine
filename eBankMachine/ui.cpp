#include "eBankMachine.h"

void showMsg(const char* line0, const char* line1, unsigned long ms) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line0);
  if (line1) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }
  if (ms) otaDelay(ms);
}

void showModeMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("    Welcome!    "));
  lcd.setCursor(0, 1);
  lcd.print(F(" Select a mode"));
}

void clearEntryLine() {
  lcd.setCursor(7, 1);
  for (int j = 0; j < 9; j++) lcd.print(' ');
  lcd.setCursor(7, 1);
  numLen = 0;
  numBuf[0] = '\0';
}

void showEntry(const __FlashStringHelper* prompt) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  lcd.setCursor(0, 1);
  lcd.print(F("Enter:       "));
  lcd.setCursor(7, 1);
  clearEntryLine();
}

void showConfirmWithdraw(long pogs) {
  long digipogs = pogs * DIGIPOGS_PER_POG_WITHDRAW;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Use "));
  lcd.print(digipogs);
  lcd.print(F(" dpogs?"));
  lcd.setCursor(0, 1);
  lcd.print(F("*=No   #=Yes"));
}

void showDepositEnterId() { showEntry(F("Enter ID")); }

void showDepositScanning() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Deposit mode"));
  lcd.setCursor(0, 1);
  lcd.print(F("Count: 0  #=done"));
}

/* ================== NEW CONFIRM SCREEN ================== */

static void fit16(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0) return;
  dst[0] = '\0';
  if (!src) return;

  size_t n = 0;
  while (src[n] && src[n] != '\r' && src[n] != '\n' && n < dstLen - 1) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = '\0';
}

void showConfirmId(const char* line1, long id, const char* name) {
  char l0[17];
  char l1[17];

  char nm[17];
  fit16(nm, sizeof(nm), name);

  if (nm[0]) snprintf(l0, sizeof(l0), "ID %ld %s", id, nm);
  else       snprintf(l0, sizeof(l0), "ID %ld", id);

  if (line1 && line1[0]) fit16(l1, sizeof(l1), line1);
  else                   snprintf(l1, sizeof(l1), "*=No  #=Yes");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l0);
  lcd.setCursor(0, 1);
  lcd.print(l1);
}