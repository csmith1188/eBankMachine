#include "deposit.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "drop.h"
#include "formbar.h"
#include "hardware.h"
#include "net.h"
#include "ui.h"

enum class DepositState : uint8_t {
  EnterId,
  ConfirmId,
  Scanning
};

static DepositState state = DepositState::EnterId;
static long toId = 0;
static int depositCount = 0;
static bool beamTiming = false;
static uint32_t beamStartMs = 0;
static uint32_t startMs = 0;
static uint32_t lastSampleUs = 0;
static uint32_t nextAllowedAt = 0;

void startDepositFlow() {
  appSetMode(TradeMode::Deposit);
  state = DepositState::EnterId;
  toId = 0;
  depositCount = 0;
  beamTiming = false;
  beamStartMs = 0;
  nextAllowedAt = 0;
  uiEntry(F("Enter ID"));
}

void depositTick() {
  if (appMode() != TradeMode::Deposit || state != DepositState::Scanning || dropActive()) return;

  const uint32_t nowUs = micros();
  if (nowUs - lastSampleUs < kDepSampleUs) return;
  lastSampleUs = nowUs;

  const int v = analogRead(PIN_IR_DEP);
  const bool broken = (v > irDepThreshold);
  const uint32_t nowMs = millis();
  if (nowMs - startMs <= kDepositArmMs) return;

  if (broken) {
    if (!beamTiming) {
      beamTiming = true;
      beamStartMs = nowMs;
    }
    return;
  }

  if (!beamTiming) return;

  const uint32_t dur = nowMs - beamStartMs;
  beamTiming = false;
  beamStartMs = 0;
  dbgPrintf("Beam %lums\n", (unsigned long)dur);

  if (dur < kDepBeamMinMs || dur > kDepBeamMaxMs) {
    dbgPrintf("TAMPER dur=%lums (range %lu-%lums)\n",
              (unsigned long)dur, (unsigned long)kDepBeamMinMs, (unsigned long)kDepBeamMaxMs);
    uiShow("TAMPER", "Beam time bad", 2000);
    appGoMenu();
    return;
  }

  if (nowMs >= nextAllowedAt) {
    depositCount++;
    nextAllowedAt = nowMs + kDepCooldownMs;
    lcd.setCursor(7, 1);
    lcd.print("     ");
    lcd.setCursor(7, 1);
    lcd.print(depositCount);
  }
}

void handleDepositKey(char k) {
  if (appMode() != TradeMode::Deposit) return;

  if (state == DepositState::ConfirmId) {
    if (k == '*') {
      state = DepositState::EnterId;
      toId = 0;
      uiEntry(F("Enter ID"));
    } else if (k == '#') {
      state = DepositState::Scanning;
      depositCount = 0;
      beamTiming = false;
      beamStartMs = 0;
      nextAllowedAt = 0;
      startMs = millis();
      lastSampleUs = micros();
      uiDepositScanning();
    }
    return;
  }

  if (state == DepositState::EnterId) {
    if (k == '*') {
      if (Entry::length() == 0) appGoMenu();
      else uiEntry(F("Enter ID"));
      return;
    }
    if (k >= '0' && k <= '9') {
      uiAcceptDigit(k, false);
      return;
    }
    if (k == '#') {
      const long val = Entry::value();
      if (val <= 0) {
        uiShow("Invalid ID", nullptr, 900);
        uiEntry(F("Enter ID"));
        return;
      }
      if (!lookupUserInteractive(val)) {
        uiEntry(F("Enter ID"));
        return;
      }
      toId = val;
      state = DepositState::ConfirmId;
      uiConfirmId(toId, stashedName());
    }
    return;
  }

  if (state != DepositState::Scanning) return;

  if (k == '*') {
    startDepositFlow();
    return;
  }

  if (k != '#') return;

  if (depositCount <= 0) {
    uiShow("No pogs", "Insert first", 1400);
    uiDepositScanning();
    return;
  }

  netEnsureConnected();
  uiShow("Sending deposit", "Please wait");

  const int dp = depositCount * kDpogsPerPogDeposit;
  String resp;
  int httpc = 0;
  FbErr err;
  const bool ok = formbarTransfer(
    KIOSK_ID, (int)toId, dp, "Pogs -> Digi", KIOSK_ACCOUNT_PIN, resp, httpc, err);

  if (ok) {
    char l1[kLcdLineCap];
    snprintf(l1, sizeof(l1), "+%d dpogs", dp);
    uiShow("Deposit OK", l1, 1800);
    dbgPrintf("Deposit OK to=%ld dp=%d\n", toId, dp);
  } else {
    uiShow("Deposit FAIL", fbErrMsg(err), 2500);
    dbgPrintf("Deposit FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
  }

  appGoMenu();
}
