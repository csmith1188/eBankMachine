#include "eBankMachine.h"

#include "eBankMachine.h"

void keypadTick() {
  static bool waitForRelease = false;
  static unsigned long lockoutUntil = 0;

  keypad.getKeys();

  bool anyDown = false;
  char pressedKey = NO_KEY;
  int newlyPressed = 0;

  for (int i = 0; i < LIST_MAX; i++) {
    KeyState s = keypad.key[i].kstate;

    if (s == PRESSED || s == HOLD) {
      anyDown = true;
    }

    if (keypad.key[i].stateChanged && s == PRESSED) {
      newlyPressed++;
      pressedKey = keypad.key[i].kchar;
    }
  }

  // reset when all keys are released
  if (!anyDown) {
    waitForRelease = false;
  }

  unsigned long now = millis();

  // ignore presses while locked out or until full release
  if (waitForRelease) return;
  if (now < lockoutUntil) return;

  // if scan says multiple keys were newly pressed at once, ignore that scan
  if (newlyPressed != 1) return;

  waitForRelease = true;
  lockoutUntil = now + 80;

  char k = pressedKey;

  // B x3 -> show IP
  if (k == 'B') {
    if (bPressCount == 0 || (now - bWindowStart) > D_WINDOW_MS) {
      bPressCount = 0;
      bWindowStart = now;
    }

    bPressCount++;

    if (bPressCount >= 3) {
      bPressCount = 0;
      bWindowStart = 0;

      if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        showMsg("IP Address:", ip.c_str(), 3000);
      } else {
        showMsg("WiFi Not", "Connected", 2000);
      }

      if (tradeMode == MODE_SELECT) showModeMenu();
      return;
    }
  }

  if (tradeMode == MODE_SELECT) {
    if (k == 'A') startWithdrawWizard();
    else if (k == 'B') startDepositFlow();
    else if (k == 'C') startStudentTransferFlow();
    else if (k == 'D') startNfcWriteFlow();
    return;
  }

  if (tradeMode == MODE_NFC_WRITE) {
    handleNfcWriteKey(k);
    return;
  }

  if (tradeMode == MODE_DIGI_TO_REAL) {
    handleWithdrawKey(k);
    return;
  }

  if (tradeMode == MODE_REAL_TO_DIGI) {
    handleDepositKey(k);
    return;
  }

  if (tradeMode == MODE_UPDATE_CARD) {
    handleCardKey(k);
    return;
  }

  if (tradeMode == MODE_STU_TO_STU) {
    handleStudentTransferKey(k);
    return;
  }
}