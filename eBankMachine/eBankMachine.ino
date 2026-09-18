#include "debug_log.h"
#include "hardware.h"
#include "keypad_router.h"
#include "net.h"
#include "nfc.h"
#include "deposit.h"
#include "drop.h"
#include "refund.h"
#include "ui.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  dbgClear();
  dbgPrintf("BOOT\n");

  hardwareInit();
  netInit();
  uiMenu();
}

void loop() {
  webTick();
  netTick();
  hardwareTick();
  dropTick();
  depositTick();
  nfcWriteTick();
  refundTick();
  keypadTick();
  delay(1);
}
