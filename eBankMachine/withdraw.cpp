#include "withdraw.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "drop.h"
#include "formbar.h"
#include "hardware.h"
#include "inventory.h"
#include "net.h"
#include "ui.h"

enum class WizardState : uint8_t {
  EnterFrom,
  ConfirmFrom,
  EnterPin,
  EnterPogs,
  Confirm
};

static WizardState state = WizardState::EnterFrom;
static long fromId = 0;
static long pin = 0;
static long pogs = 0;
static uint8_t cCount = 0;
static uint32_t cWindow = 0;
static uint8_t dCount = 0;
static uint32_t dWindow = 0;

long withdrawFromId() {
  return fromId;
}

void startWithdrawWizard() {
  appSetMode(TradeMode::Withdraw);
  fromId = pin = pogs = 0;
  state = WizardState::EnterFrom;
  uiEntry(F("Enter FROM ID"));
}

void handleWithdrawKey(char k) {
  if (appMode() != TradeMode::Withdraw) return;

  const uint32_t now = millis();

  if (k == 'C') {
    if (dropActive()) {
      uiShow("Busy...", nullptr, 400);
      return;
    }
    if (chordCount('C', k, 3, kDebugChordMs, cCount, cWindow, now)) dropStart(1);
    return;
  }

  if (k == 'D') {
    if (dropActive()) {
      uiShow("Busy...", nullptr, 400);
      return;
    }
    if (chordCount('D', k, 3, kDebugChordMs, dCount, dWindow, now)) {
      uiShow("UNJAM UP", nullptr, 300);
      servoWriteUs(kServoUpUs);
      coopDelay(2000);
      servoStop();
      appGoMenu();
    }
    return;
  }

  if (k == '*') {
    if (state == WizardState::ConfirmFrom) {
      state = WizardState::EnterFrom;
      uiEntry(F("Enter FROM ID"));
      return;
    }
    if (state == WizardState::Confirm) {
      appGoMenu();
      return;
    }
    if (Entry::length() == 0) appGoMenu();
    else uiClearEntryLine();
    return;
  }

  if (state == WizardState::ConfirmFrom) {
    if (k == '#') {
      state = WizardState::EnterPin;
      uiEntry(F("Enter PIN"));
    }
    return;
  }

  if (state == WizardState::Confirm) {
    if (k != '#') return;

    netEnsureConnected();
    uiShow("Transferring...", "Please wait");

    const int digipogs = (int)pogs * kDpogsPerPogWithdraw;
    String resp;
    int httpc = 0;
    FbErr err;
    const bool ok = formbarTransfer(
      (int)fromId, KIOSK_ID, digipogs, "Digi -> Pogs", (int)pin, resp, httpc, err);

    if (ok) {
      uiShow("Transfer OK", "Dropping...", 700);
      dbgPrintf("Withdraw OK from=%ld pogs=%ld\n", fromId, pogs);
      dropStart((int)pogs);
    } else {
      uiShow("Transfer FAIL", fbErrMsg(err), 2500);
      dbgPrintf("Withdraw FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
      appGoMenu();
    }
    return;
  }

  if (k >= '0' && k <= '9') {
    uiAcceptDigit(k, state == WizardState::EnterPin);
    return;
  }

  if (k != '#') return;

  const long val = Entry::value();

  if (state == WizardState::EnterFrom) {
    if (val <= 0) {
      uiShow("Invalid FROM", nullptr, 900);
      uiEntry(F("Enter FROM ID"));
      return;
    }
    if (!lookupUserInteractive(val)) {
      uiEntry(F("Enter FROM ID"));
      return;
    }
    fromId = val;
    state = WizardState::ConfirmFrom;
    uiConfirmId(fromId, stashedName());
    return;
  }

  if (state == WizardState::EnterPin) {
    if (val <= 0) {
      uiShow("Invalid PIN", nullptr, 900);
      uiEntry(F("Enter PIN"));
      return;
    }
    pin = val;
    state = WizardState::EnterPogs;
    uiEntry(F("Enter POGS"));
    return;
  }

  if (state == WizardState::EnterPogs) {
    if (val <= 0) {
      uiShow("Invalid POGS", nullptr, 900);
      uiEntry(F("Enter POGS"));
      return;
    }

    pogs = val;
    if (inventoryCount() < pogs) {
      char l0[kLcdLineCap];
      char l1[kLcdLineCap];
      snprintf(l0, sizeof(l0), "Low Stock Warn");
      snprintf(l1, sizeof(l1), "%d left, want %ld", inventoryCount(), pogs);
      uiShow(l0, l1, 1800);
    }

    state = WizardState::Confirm;
    uiConfirmWithdraw(pogs);
  }
}
