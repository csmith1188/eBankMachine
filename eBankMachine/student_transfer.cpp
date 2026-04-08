// ============================
// student_transfer.cpp
// ============================
#include "eBankMachine.h"

static void stashNameToBuf(const String& name) {
  memset(idNameBuf, 0, sizeof(idNameBuf));
  if (!name.length()) return;
  String n = name;
  n.replace("\r", "");
  n.replace("\n", "");
  strncpy(idNameBuf, n.c_str(), sizeof(idNameBuf) - 1);
}

void startStudentTransferFlow() {
  tradeMode = MODE_STU_TO_STU;
  stFrom = stPin = stTo = stAmt = 0;
  stState = ST_ENTER_FROM;
  showEntry(F("From ID"));
}

static void goMenu() {
  tradeMode = MODE_SELECT;
  showModeMenu();
}

void handleStudentTransferKey(char k) {
  if (tradeMode != MODE_STU_TO_STU) return;

  // * behavior:
  // - On confirm screens: cancel/back
  // - On entry screens: if empty -> menu, else clear
  if (k == '*') {
    if (stState == ST_CONFIRM_FROM) {
      stState = ST_ENTER_FROM;
      showEntry(F("From ID"));
      clearEntryLine();
      return;
    }
    if (stState == ST_CONFIRM_TO) {
      stState = ST_ENTER_TO;
      showEntry(F("To ID"));
      clearEntryLine();
      return;
    }
    if (stState == ST_CONFIRM_SEND) {
      goMenu();
      return;
    }

    // entry screens
    if (numLen == 0) goMenu();
    else clearEntryLine();
    return;
  }

  // Confirm screens
  if (stState == ST_CONFIRM_FROM) {
    if (k == '#') {
      stState = ST_ENTER_PIN;
      showEntry(F("From PIN"));
      clearEntryLine();
    }
    return;
  }

  if (stState == ST_CONFIRM_TO) {
    if (k == '#') {
      stState = ST_ENTER_AMOUNT;
      showEntry(F("Amount"));
      clearEntryLine();
    }
    return;
  }

  if (stState == ST_CONFIRM_SEND) {
    if (k == '#') {

      wifiEnsureConnected();
      showMsg("Processing...", "Please wait", 0);

      // ---- Calculate tax ----
      long tax = (stAmt * 5 + 99) / 100;  // 5% rounded up
      long total = stAmt + tax;

      String resp;
      int httpc = 0;
      FbErr err;

      // 1) Main transfer (student -> student)
      bool okMain = formbarTransferEx(
        (int)stFrom,
        (int)stTo,
        (int)stAmt,
        "Stu -> Stu",
        (int)stPin,
        resp,
        httpc,
        err);

      if (!okMain) {
        showMsg("Transfer FAIL", fbErrMsg(err), 2500);
        goMenu();
        return;
      }

      // 2) Tax transfer (student -> you / kiosk id 28)
      bool okTax = formbarTransferEx(
        (int)stFrom,
        KIOSK_ID,  // your ID (28)
        (int)tax,
        "Transfer Tax",
        (int)stPin,
        resp,
        httpc,
        err);

      if (!okTax) {
        // Main succeeded but tax failed
        dbgPrintf("Tax FAIL err=%d http=%d\n", (int)err, httpc);
        showMsg("Tax Error", fbErrMsg(err), 2500);
        goMenu();
        return;
      }

      // Success
      char line1[17];
      snprintf(line1, sizeof(line1), "Tax:%ld Total:%ld", tax, total);
      showMsg("Transfer OK", line1, 2000);

      dbgPrintf("StuXfer OK from=%ld to=%ld amt=%ld tax=%ld total=%ld\n",
                stFrom, stTo, stAmt, tax, total);

      goMenu();
    }
    return;
  }

  // Digit entry (mask PIN)
  if (k >= '0' && k <= '9') {
    if (numLen < sizeof(numBuf) - 1) {
      numBuf[numLen++] = k;
      numBuf[numLen] = '\0';

      lcd.setCursor(7 + (numLen - 1), 1);
      if (stState == ST_ENTER_PIN) lcd.print('*');
      else lcd.print(k);
    }
    return;
  }

  // Next step
  if (k == '#') {
    long val = (numLen > 0) ? atol(numBuf) : 0;

    if (stState == ST_ENTER_FROM) {
      if (val <= 0) {
        showMsg("Invalid ID", nullptr, 900);
        showEntry(F("From ID"));
        clearEntryLine();
        return;
      }

      wifiEnsureConnected();
      if (WiFi.status() != WL_CONNECTED) {
        showMsg("No WiFi", "Try again", 1500);
        showEntry(F("From ID"));
        clearEntryLine();
        return;
      }

      showMsg("Checking ID", "Please wait", 0);
      String name;
      int httpc = 0;
      bool ok = formbarUserExists((int)val, name, httpc);

      if (!ok) {
        if (httpc == 404) showMsg("ID Not Found", "Try again", 1600);
        else showMsg("Bad ID/WiFi", "Try again", 1600);
        showEntry(F("From ID"));
        clearEntryLine();
        return;
      }

      stFrom = val;
      stashNameToBuf(name);
      stState = ST_CONFIRM_FROM;
      showConfirmId(nullptr, stFrom, idNameBuf);
      return;
    }

    if (stState == ST_ENTER_PIN) {
      if (val <= 0) {
        showMsg("Invalid PIN", nullptr, 900);
        showEntry(F("From PIN"));
        clearEntryLine();
        return;
      }
      stPin = val;
      stState = ST_ENTER_TO;
      showEntry(F("To ID"));
      clearEntryLine();
      return;
    }

    if (stState == ST_ENTER_TO) {
      if (val <= 0) {
        showMsg("Invalid ID", nullptr, 900);
        showEntry(F("To ID"));
        clearEntryLine();
        return;
      }

      wifiEnsureConnected();
      if (WiFi.status() != WL_CONNECTED) {
        showMsg("No WiFi", "Try again", 1500);
        showEntry(F("To ID"));
        clearEntryLine();
        return;
      }

      showMsg("Checking ID", "Please wait", 0);
      String name;
      int httpc = 0;
      bool ok = formbarUserExists((int)val, name, httpc);

      if (!ok) {
        if (httpc == 404) showMsg("ID Not Found", "Try again", 1600);
        else showMsg("Bad ID/WiFi", "Try again", 1600);
        showEntry(F("To ID"));
        clearEntryLine();
        return;
      }

      stTo = val;
      stashNameToBuf(name);
      stState = ST_CONFIRM_TO;
      showConfirmId(nullptr, stTo, idNameBuf);
      return;
    }

    if (stState == ST_ENTER_AMOUNT) {
      if (val <= 0) {
        showMsg("Invalid AMT", nullptr, 900);
        showEntry(F("Amount"));
        clearEntryLine();
        return;
      }

      stAmt = val;

      // confirm screen: "Send <amt>?"
      char l0[17];
      char l1[17];
      snprintf(l0, sizeof(l0), "Send %ld?", stAmt);
      snprintf(l1, sizeof(l1), "*=No  #=Yes");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(l0);
      lcd.setCursor(0, 1);
      lcd.print(l1);

      stState = ST_CONFIRM_SEND;
      return;
    }
  }
}