// ============================
// withdraw.cpp
// ============================
#include "eBankMachine.h"

void startWithdrawWizard() {
  tradeMode = MODE_DIGI_TO_REAL;
  wzFrom = wzPin = wzPogs = 0;
  wzState = WZ_ENTER_FROM;
  showEntry(F("Enter FROM ID"));
}

static void stashNameToBuf(const String& name) {
  memset(idNameBuf, 0, sizeof(idNameBuf));
  if (!name.length()) return;

  String n = name;
  n.replace("\r", "");
  n.replace("\n", "");
  strncpy(idNameBuf, n.c_str(), sizeof(idNameBuf) - 1);
}

void handleWithdrawKey(char k) {
  if (tradeMode != MODE_DIGI_TO_REAL) return;

  unsigned long now = millis();

  // C x3 -> drop 1
  if (k == 'C') {
    if (motionState != MS_IDLE) {
      showMsg("Busy...", nullptr, 400);
      return;
    }
    if (cPressCount == 0 || (now - cWindowStart) > D_WINDOW_MS) {
      cPressCount = 0;
      cWindowStart = now;
    }
    cPressCount++;
    if (cPressCount >= 3) {
      cPressCount = 0;
      cWindowStart = 0;
      startDrop(1);
    }
    return;
  }

  // D x3 -> unjam 2s
  if (k == 'D') {
    if (motionState != MS_IDLE) {
      showMsg("Busy...", nullptr, 400);
      return;
    }
    if (dPressCount == 0 || (now - dWindowStart) > D_WINDOW_MS) {
      dPressCount = 0;
      dWindowStart = now;
    }
    dPressCount++;
    if (dPressCount >= 3) {
      dPressCount = 0;
      dWindowStart = 0;

      showMsg("UNJAM UP", nullptr, 300);
      servoAttach();
      myServo.writeMicroseconds(SERVO_UP_US);
      otaDelay(2000);
      servoStopDetach();

      showModeMenu();
      tradeMode = MODE_SELECT;
    }
    return;
  }

  // Cancel / clear
  if (k == '*') {
    // If we're on a confirm screen, * means "No / Cancel"
    if (wzState == WZ_CONFIRM_FROM) {
      wzState = WZ_ENTER_FROM;
      showEntry(F("Enter FROM ID"));
      return;
    }
    if (wzState == WZ_CONFIRM) {
      tradeMode = MODE_SELECT;
      showModeMenu();
      return;
    }

    // On entry screens:
    // If field is empty, go back to menu. Otherwise clear the field.
    if (numLen == 0) {
      tradeMode = MODE_SELECT;
      showModeMenu();
    } else {
      clearEntryLine();
    }
    return;
  }

  // CONFIRM FROM ID screen
  if (wzState == WZ_CONFIRM_FROM) {
    if (k == '#') {
      wzState = WZ_ENTER_PIN;
      showEntry(F("Enter PIN"));
      clearEntryLine();
    }
    return;
  }

  // Confirm withdraw screen (final)
  if (wzState == WZ_CONFIRM) {
    if (k == '#') {
      wifiEnsureConnected();
      showMsg("Transferring...", "Please wait", 0);

      int digipogs = (int)wzPogs * DIGIPOGS_PER_POG_WITHDRAW;
      String resp;
      int httpc = 0;
      FbErr err;

      bool ok = formbarTransferEx(
        (int)wzFrom,
        KIOSK_ID,
        digipogs,
        "Digi -> Pogs",
        (int)wzPin,
        resp,
        httpc,
        err);

      if (ok) {
        showMsg("Transfer OK", "Dropping...", 700);
        dbgPrintf("Withdraw OK from=%ld pogs=%ld\n", wzFrom, wzPogs);
        startDrop((int)wzPogs);
      } else {
        showMsg("Transfer FAIL", fbErrMsg(err), 2500);
        dbgPrintf("Withdraw FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
        tradeMode = MODE_SELECT;
        showModeMenu();
      }
    }
    return;
  }

  // Digit entry (mask PIN)
  if (k >= '0' && k <= '9') {
    if (numLen < sizeof(numBuf) - 1) {
      numBuf[numLen++] = k;
      numBuf[numLen] = '\0';

      lcd.setCursor(7 + (numLen - 1), 1);
      if (wzState == WZ_ENTER_PIN) lcd.print('*');
      else lcd.print(k);
    }
    return;
  }

  // Next step
  if (k == '#') {
    long val = (numLen > 0) ? atol(numBuf) : 0;

    if (wzState == WZ_ENTER_FROM) {
      if (val <= 0) {
        showMsg("Invalid FROM", nullptr, 900);
        showEntry(F("Enter FROM ID"));
      } else {
        // NEW: validate ID exists first
        wifiEnsureConnected();
        if (WiFi.status() != WL_CONNECTED) {
          showMsg("No WiFi", "Try again", 1500);
          showEntry(F("Enter FROM ID"));
        } else {
          showMsg("Checking ID", "Please wait", 0);

          String name;
          int httpc = 0;
          bool ok = formbarUserExists((int)val, name, httpc);

          if (!ok) {
            if (httpc == 404) showMsg("ID Not Found", "Try again", 1600);
            else showMsg("Bad ID/WiFi", "Try again", 1600);
            showEntry(F("Enter FROM ID"));
          } else {
            wzFrom = val;
            stashNameToBuf(name);
            wzState = WZ_CONFIRM_FROM;
            showConfirmId(nullptr, wzFrom, idNameBuf);
          }
        }
      }
    } else if (wzState == WZ_ENTER_PIN) {
      if (val <= 0) {
        showMsg("Invalid PIN", nullptr, 900);
        showEntry(F("Enter PIN"));
      } else {
        wzPin = val;
        wzState = WZ_ENTER_POGS;
        showEntry(F("Enter POGS"));
      }
    } else if (wzState == WZ_ENTER_POGS) {
      if (val <= 0) {
        showMsg("Invalid POGS", nullptr, 900);
        showEntry(F("Enter POGS"));
      }
      else {
        wzPogs = val;

        // Warn if estimated stock is lower than requested amount
        if (currencyCount < wzPogs) {
          char l0[17];
          char l1[17];

          snprintf(l0, sizeof(l0), "Low Stock Warn");
          snprintf(l1, sizeof(l1), "%d left, want %ld", currencyCount, wzPogs);

          showMsg(l0, l1, 1800);
        }

        wzState = WZ_CONFIRM;
        showConfirmWithdraw(wzPogs);
      }
    }

    // Only clear entry line if we’re still on an entry screen
    if (wzState == WZ_ENTER_FROM || wzState == WZ_ENTER_PIN || wzState == WZ_ENTER_POGS) {
      clearEntryLine();
    }
  }
}