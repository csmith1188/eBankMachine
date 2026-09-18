#include "keypad_router.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "deposit.h"
#include "hardware.h"
#include "net.h"
#include "nfc.h"
#include "student_transfer.h"
#include "ui.h"
#include "web.h"
#include "withdraw.h"

void keypadTick() {
  static char waitKey = NO_KEY;
  static uint32_t waitUntil = 0;
  static uint32_t lockoutUntil = 0;
  static uint8_t bCount = 0;
  static uint32_t bWindow = 0;
  static bool menuBPending = false;

  keypad.getKeys();
  const uint32_t now = millis();

  if (menuBPending && appMode() == TradeMode::Select && (now - bWindow) > kMenuBChordMs) {
    menuBPending = false;
    const uint8_t n = bCount;
    bCount = 0;
    if (n == 1) startDepositFlow();
  }

  if (hardwareRecovering() || webBusy()) return;

  char pressedKey = NO_KEY;
  int newlyPressed = 0;
  bool waitKeyDown = false;

  for (int i = 0; i < LIST_MAX; i++) {
    const KeyState s = keypad.key[i].kstate;
    const char c = keypad.key[i].kchar;
    if ((s == PRESSED || s == HOLD) && waitKey != NO_KEY && c == waitKey) waitKeyDown = true;
    if (keypad.key[i].stateChanged && s == PRESSED) {
      newlyPressed++;
      pressedKey = keypad.key[i].kchar;
    }
  }

  if (waitKey != NO_KEY) {
    if (!waitKeyDown || (int32_t)(now - waitUntil) >= 0) waitKey = NO_KEY;
    else return;
  }

  if ((int32_t)(now - lockoutUntil) < 0) return;
  if (newlyPressed > 1) {
    static uint32_t lastMultiLog = 0;
    if (now - lastMultiLog > 1000) {
      lastMultiLog = now;
      dbgPrintf("KEYPAD ignore multi=%d\n", newlyPressed);
    }
    return;
  }
  if (newlyPressed != 1) return;

  waitKey = pressedKey;
  waitUntil = now + kKeyWaitMs;
  lockoutUntil = now + kKeyLockoutMs;

  const char k = pressedKey;
  dbgPrintf("KEY %c\n", k);

  if (k == 'B') {
    if (appMode() == TradeMode::Select) {
      if (bCount == 0 || (now - bWindow) > kMenuBChordMs) {
        bCount = 0;
        bWindow = now;
      }
      bCount++;
      bWindow = now;
      menuBPending = true;
      if (bCount >= 3) {
        menuBPending = false;
        bCount = 0;
        netShowIp();
        uiMenu();
      }
      return;
    }

    static uint8_t flowB = 0;
    static uint32_t flowBAt = 0;
    if (chordCount('B', k, 3, kDebugChordMs, flowB, flowBAt, now)) {
      netShowIp();
      if (appMode() == TradeMode::Select) uiMenu();
      return;
    }
  } else {
    menuBPending = false;
    bCount = 0;
  }

  switch (appMode()) {
    case TradeMode::Select:
      if (k == 'A') startWithdrawWizard();
      else if (k == 'C') startStudentTransferFlow();
      else if (k == 'D') startNfcWriteFlow();
      break;
    case TradeMode::NfcWrite:
      handleNfcWriteKey(k);
      break;
    case TradeMode::Withdraw:
      handleWithdrawKey(k);
      break;
    case TradeMode::Deposit:
      handleDepositKey(k);
      break;
    case TradeMode::StudentTransfer:
      handleStudentTransferKey(k);
      break;
  }
}
