#include "refund.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "drop.h"
#include "formbar.h"
#include "net.h"
#include "ui.h"

static bool pending = false;
static long toId = 0;
static int dpogs = 0;
static uint32_t nextTryAt = 0;

void refundClear() {
  pending = false;
  toId = 0;
  dpogs = 0;
  nextTryAt = 0;
}

void refundQueue(long id, int amount) {
  pending = true;
  toId = id;
  dpogs = amount;
  nextTryAt = millis();
  dbgPrintf("Refund pending to=%ld dpogs=%d\n", toId, dpogs);
}

bool refundIsPending() {
  return pending;
}

void refundScheduleRetry() {
  nextTryAt = millis() + kRefundRetryMs;
}

bool refundTryNow() {
  if (!pending || toId <= 0 || dpogs <= 0) return true;

  netEnsureConnected();
  if (!netConnected()) return false;

  String resp;
  int httpc = 0;
  FbErr err;
  const bool ok = formbarTransfer(
    KIOSK_ID, (int)toId, dpogs, "refund", KIOSK_ACCOUNT_PIN, resp, httpc, err);

  if (ok) {
    dbgPrintf("Refund OK\n");
    refundClear();
    return true;
  }

  dbgPrintf("Refund FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
  return false;
}

void refundTick() {
  if (!pending || dropActive()) return;
  if ((int32_t)(millis() - nextTryAt) < 0) return;

  if (refundTryNow()) {
    uiShow("Refund SENT", "OK", 1200);
    appGoMenu();
  } else {
    refundScheduleRetry();
  }
}
