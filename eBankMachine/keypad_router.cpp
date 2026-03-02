#include "eBankMachine.h"

void keypadTick() {
  if (!keypad.getKeys()) return;

  for (int i = 0; i < LIST_MAX; i++) {
    if (!keypad.key[i].stateChanged) continue;
    if (keypad.key[i].kstate != PRESSED) continue;

    char k = keypad.key[i].kchar;
    unsigned long now = millis();

    // B x3 -> show IP (but do NOT block normal B unless it actually hits 3)
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
        return;  // ONLY return when we actually did the IP action
      }

      // not 3 yet -> allow normal handling (menu deposit, etc.)
      // (no return here)
    }

    // Menu mode routing
    // Menu mode routing
    if (tradeMode == MODE_SELECT) {
      if (k == 'A') startWithdrawWizard();            // Digi -> Pogs
      else if (k == 'B') startDepositFlow();          // Pogs -> Digi
      else if (k == 'C') startStudentTransferFlow();  // Student -> Student
      else if (k == 'D') startNfcWriteFlow();         // NEW: write ID to card
      continue;
    }

    if (tradeMode == MODE_NFC_WRITE) {
      handleNfcWriteKey(k);
      continue;
    }

    if (tradeMode == MODE_DIGI_TO_REAL) {
      handleWithdrawKey(k);
      continue;
    }

    if (tradeMode == MODE_REAL_TO_DIGI) {
      handleDepositKey(k);
      continue;
    }

    if (tradeMode == MODE_UPDATE_CARD) {
      handleCardKey(k);
      continue;
    }

    // if you add MODE_STU_TO_STU:
    if (tradeMode == MODE_STU_TO_STU) {
      handleStudentTransferKey(k);
      continue;
    }
  }
}