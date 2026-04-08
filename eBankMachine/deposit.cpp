// ============================
// deposit.cpp
// ============================
#include "eBankMachine.h"

void startDepositFlow() {
  tradeMode = MODE_REAL_TO_DIGI;
  depState = DEP_ENTER_ID;
  depToId = 0;
  depositCount = 0;
  depBeamTiming = false;
  depBeamStartMs = 0;
  depWasAbove = false;
  depNextAllowedAt = 0;
  showDepositEnterId();
}

void depositTick() {
  if (!(tradeMode == MODE_REAL_TO_DIGI && depState == DEP_SCANNING && motionState == MS_IDLE)) return;

  unsigned long nowUs = micros();
  if (nowUs - depLastSampleUs < DEP_SAMPLE_US) return;
  depLastSampleUs = nowUs;

  int v = analogRead(IR_DEP_PIN);
  bool broken = (v > IR_DEP_THRESHOLD); // beam broken?

  unsigned long nowMs = millis();
  bool armed = (nowMs - depStartMs > 100);

  if (!armed) {
    depWasAbove = broken;
    return;
  }

  // Strict beam timing window
  if (broken) {
    if (!depBeamTiming) {
      depBeamTiming = true;
      depBeamStartMs = nowMs;
    }
  } else {
    // beam cleared
    if (depBeamTiming) {
      unsigned long dur = nowMs - depBeamStartMs;

      depLastBeamMs = dur;
      if (dur > depMaxBeamMs) depMaxBeamMs = dur;

      dbgPrintf("Beam %lums\n", dur);

      // Strict range check
      if (dur < DEP_BEAM_MIN_MS || dur > DEP_BEAM_MAX_MS) {
        dbgPrintf("TAMPER dur=%lums (range %lu-%lums)\n",
                  dur, DEP_BEAM_MIN_MS, DEP_BEAM_MAX_MS);

        showMsg("TAMPER", "Beam time bad", 2000);

        // Reset deposit scanning state cleanly
        depBeamTiming = false;
        depBeamStartMs = 0;
        depWasAbove = false;
        depNextAllowedAt = 0;
        depositCount = 0;
        depToId = 0;
        depState = DEP_ENTER_ID;

        tradeMode = MODE_SELECT;
        showModeMenu();
        return;
      }

      // Valid pog -> count it (cooldown protected)
      if (nowMs >= depNextAllowedAt) {
        depositCount++;
        depNextAllowedAt = nowMs + DEP_COOLDOWN_MS;

        lcd.setCursor(7, 1);
        lcd.print("     ");
        lcd.setCursor(7, 1);
        lcd.print(depositCount);
      }

      depBeamTiming = false;
      depBeamStartMs = 0;
    }
  }

  depWasAbove = broken; // keep this updated even if you don't use it now
}

static void stashNameToBuf(const String& name) {
  memset(idNameBuf, 0, sizeof(idNameBuf));
  if (!name.length()) return;

  String n = name;
  n.replace("\r", "");
  n.replace("\n", "");
  strncpy(idNameBuf, n.c_str(), sizeof(idNameBuf) - 1);
}

void handleDepositKey(char k) {
  if (tradeMode != MODE_REAL_TO_DIGI) return;

  // ============================
  // Confirm ID screen
  // ============================
  if (depState == DEP_CONFIRM_ID) {
    if (k == '*') {
      depState = DEP_ENTER_ID;
      depToId = 0;
      showDepositEnterId();
      clearEntryLine();
      return;
    }
    if (k == '#') {
      depState = DEP_SCANNING;
      depositCount = 0;

      depWasAbove = false;
      depNextAllowedAt = 0;
      depBeamTiming = false;
      depBeamStartMs = 0;

      depStartMs = millis();
      depLastSampleUs = micros();

      showDepositScanning();
      return;
    }
    return;
  }

  // ============================
  // Enter ID mode
  // ============================
  if (depState == DEP_ENTER_ID) {

    // * = back/clear
    if (k == '*') {
      if (numLen == 0) {
        tradeMode = MODE_SELECT;
        showModeMenu();
      } else {
        showDepositEnterId();
        clearEntryLine();
      }
      return;
    }

    // digits = type ID
    if (k >= '0' && k <= '9') {
      if (numLen < sizeof(numBuf) - 1) {
        numBuf[numLen++] = k;
        numBuf[numLen] = '\0';

        lcd.setCursor(7, 1);
        lcd.print("         ");
        lcd.setCursor(7, 1);
        lcd.print(numBuf);
      }
      return;
    }

    // # = confirm ID
    if (k == '#') {
      long val = (numLen > 0) ? atol(numBuf) : 0;

      if (val <= 0) {
        showMsg("Invalid ID", nullptr, 900);
        showDepositEnterId();
        clearEntryLine();
        return;
      }

      wifiEnsureConnected();
      if (WiFi.status() != WL_CONNECTED) {
        showMsg("No WiFi", "Try again", 1500);
        showDepositEnterId();
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
        showDepositEnterId();
        clearEntryLine();
        return;
      }

      depToId = val;
      stashNameToBuf(name);
      depState = DEP_CONFIRM_ID;
      showConfirmId(nullptr, depToId, idNameBuf);
      return;
    }

    return;
  }

  // ============================
  // Scanning mode
  // ============================

  // OPTIONAL but recommended: * cancels scanning
  if (depState == DEP_SCANNING && k == '*') {
    depState = DEP_ENTER_ID;
    depToId = 0;
    depositCount = 0;
    depBeamTiming = false;
    depBeamStartMs = 0;
    showDepositEnterId();
    clearEntryLine();
    return;
  }

  // # sends deposit
  if (depState == DEP_SCANNING && k == '#') {
    wifiEnsureConnected();
    showMsg("Sending deposit", "Please wait", 0);

    int dp = depositCount * DIGIPOGS_PER_POG_DEPOSIT;
    String resp;
    int httpc = 0;
    FbErr err;

    bool ok = formbarTransferEx(
      KIOSK_ID,
      (int)depToId,
      dp,
      "Pogs -> Digi",
      KIOSK_ACCOUNT_PIN,
      resp,
      httpc,
      err);

    if (ok) {
      char l1[17];
      snprintf(l1, sizeof(l1), "+%d dpogs", dp);
      showMsg("Deposit OK", l1, 1800);
      dbgPrintf("Deposit OK to=%ld dp=%d\n", depToId, dp);
    } else {
      showMsg("Deposit FAIL", fbErrMsg(err), 2500);
      dbgPrintf("Deposit FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
    }

    tradeMode = MODE_SELECT;
    depState = DEP_ENTER_ID;
    showModeMenu();
    return;
  }
}