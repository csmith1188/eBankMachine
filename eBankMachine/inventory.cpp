#include "inventory.h"

#include "config.h"
#include "util.h"

#include <Preferences.h>

static Preferences prefs;
static int gCount = kMaxCurrency;

void inventoryLoad() {
  gCount = kMaxCurrency;
  if (prefs.begin("ebank", true)) {
    gCount = prefs.getInt("inv", kMaxCurrency);
    prefs.end();
  }
  gCount = clampi(gCount, 0, kMaxCurrency);
}

void inventorySave() {
  if (prefs.begin("ebank", false)) {
    prefs.putInt("inv", gCount);
    prefs.end();
  }
}

int inventoryCount() {
  return gCount;
}

void inventorySet(int n) {
  gCount = clampi(n, 0, kMaxCurrency);
  inventorySave();
}

void inventoryConsume(int n) {
  gCount = clampi(gCount - n, 0, kMaxCurrency);
  inventorySave();
}

void inventoryRefill() {
  inventorySet(kMaxCurrency);
}

bool inventoryLow() {
  return gCount <= kLowStockThreshold;
}
