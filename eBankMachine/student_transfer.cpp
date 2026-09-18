#include "student_transfer.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "formbar.h"
#include "hardware.h"
#include "net.h"
#include "ui.h"
#include "util.h"

enum class StuState : uint8_t {
  EnterFrom,
  ConfirmFrom,
  EnterPin,
  EnterTo,
  ConfirmTo,
  EnterAmount,
  ConfirmSend
};

static StuState state = StuState::EnterFrom;
static long fromId = 0;
static long pin = 0;
static long toId = 0;
static long amount = 0;

void startStudentTransferFlow() {
  appSetMode(TradeMode::StudentTransfer);
  fromId = pin = toId = amount = 0;
  state = StuState::EnterFrom;
  uiEntry(F("From ID"));
}

void handleStudentTransferKey(char k) {
  if (appMode() != TradeMode::StudentTransfer) return;

  if (k == '*') {
    if (state == StuState::ConfirmFrom) {
      state = StuState::EnterFrom;
      uiEntry(F("From ID"));
      return;
    }
    if (state == StuState::ConfirmTo) {
      state = StuState::EnterTo;
      uiEntry(F("To ID"));
      return;
    }
    if (state == StuState::ConfirmSend) {
      appGoMenu();
      return;
    }
    if (Entry::length() == 0) appGoMenu();
    else uiClearEntryLine();
    return;
  }

  if (state == StuState::ConfirmFrom) {
    if (k == '#') {
      state = StuState::EnterPin;
      uiEntry(F("From PIN"));
    }
    return;
  }

  if (state == StuState::ConfirmTo) {
    if (k == '#') {
      state = StuState::EnterAmount;
      uiEntry(F("Amount"));
    }
    return;
  }

  if (state == StuState::ConfirmSend) {
    if (k != '#') return;

    netEnsureConnected();
    uiShow("Processing...", "Please wait");

    const int tax = ceilPercent(amount, kTransferTaxPercent);
    const long total = amount + tax;

    String resp;
    int httpc = 0;
    FbErr err;

    const bool okMain = formbarTransfer(
      (int)fromId, (int)toId, (int)amount, "Stu -> Stu", (int)pin, resp, httpc, err);
    if (!okMain) {
      uiShow("Transfer FAIL", fbErrMsg(err), 2500);
      appGoMenu();
      return;
    }

    if (tax > 0) {
      const bool okTax = formbarTransfer(
        (int)fromId, KIOSK_ID, tax, "Transfer Tax", (int)pin, resp, httpc, err);
      if (!okTax) {
        dbgPrintf("Tax FAIL err=%d http=%d\n", (int)err, httpc);
        uiShow("Tax Error", fbErrMsg(err), 2500);
        appGoMenu();
        return;
      }
    }

    char line1[kLcdLineCap];
    snprintf(line1, sizeof(line1), "Tax:%d Total:%ld", tax, total);
    uiShow("Transfer OK", line1, 2000);
    dbgPrintf("StuXfer OK from=%ld to=%ld amt=%ld tax=%d total=%ld\n",
              fromId, toId, amount, tax, total);
    appGoMenu();
    return;
  }

  if (k >= '0' && k <= '9') {
    uiAcceptDigit(k, state == StuState::EnterPin);
    return;
  }

  if (k != '#') return;
  const long val = Entry::value();

  if (state == StuState::EnterFrom) {
    if (val <= 0) {
      uiShow("Invalid ID", nullptr, 900);
      uiEntry(F("From ID"));
      return;
    }
    if (!lookupUserInteractive(val)) {
      uiEntry(F("From ID"));
      return;
    }
    fromId = val;
    state = StuState::ConfirmFrom;
    uiConfirmId(fromId, stashedName());
    return;
  }

  if (state == StuState::EnterPin) {
    if (val <= 0) {
      uiShow("Invalid PIN", nullptr, 900);
      uiEntry(F("From PIN"));
      return;
    }
    pin = val;
    state = StuState::EnterTo;
    uiEntry(F("To ID"));
    return;
  }

  if (state == StuState::EnterTo) {
    if (val <= 0) {
      uiShow("Invalid ID", nullptr, 900);
      uiEntry(F("To ID"));
      return;
    }
    if (!lookupUserInteractive(val)) {
      uiEntry(F("To ID"));
      return;
    }
    toId = val;
    state = StuState::ConfirmTo;
    uiConfirmId(toId, stashedName());
    return;
  }

  if (state == StuState::EnterAmount) {
    if (val <= 0) {
      uiShow("Invalid AMT", nullptr, 900);
      uiEntry(F("Amount"));
      return;
    }
    amount = val;
    char l0[kLcdLineCap];
    snprintf(l0, sizeof(l0), "Send %ld?", amount);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(l0);
    lcd.setCursor(0, 1);
    lcd.print(F("*=No  #=Yes"));
    state = StuState::ConfirmSend;
  }
}
