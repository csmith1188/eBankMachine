#include "eBankMachine.h"

void keypadTick() {
  static char waitKey = NO_KEY;
  static unsigned long waitUntil = 0;
  static unsigned long lockoutUntil = 0;

  keypad.getKeys();

  unsigned long now = millis();
  char pressedKey = NO_KEY;
  int newlyPressed = 0;
  bool waitKeyDown = false;

  for (int i = 0; i < LIST_MAX; i++) {
    KeyState s = keypad.key[i].kstate;
    char c = keypad.key[i].kchar;

    if ((s == PRESSED || s == HOLD) && waitKey != NO_KEY && c == waitKey) {
      waitKeyDown = true;
    }

    if (keypad.key[i].stateChanged && s == PRESSED) {
      newlyPressed++;
      pressedKey = keypad.key[i].kchar;
    }
  }

  // Only wait for the key we just accepted. A ghost/stuck line that
  // never goes idle used to block the whole keypad after the first press.
  if (waitKey != NO_KEY) {
    if (!waitKeyDown || (long)(now - waitUntil) >= 0) {
      waitKey = NO_KEY;
    } else {
      return;
    }
  }

  if (now < lockoutUntil) return;

  if (newlyPressed > 1) {
    static unsigned long lastMultiLog = 0;
    if (now - lastMultiLog > 1000) {
      lastMultiLog = now;
      dbgPrintf("KEYPAD ignore multi=%d\n", newlyPressed);
    }
    return;
  }

  if (newlyPressed != 1) return;

  waitKey = pressedKey;
  waitUntil = now + 400;
  lockoutUntil = now + 80;

  char k = pressedKey;
  dbgPrintf("KEY %c\n", k);

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
