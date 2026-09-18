#include "debug_log.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstring>

static char gDbgBuf[4096];
static size_t gDbgLen = 0;

void dbgClear() {
  gDbgLen = 0;
  gDbgBuf[0] = '\0';
}

void dbgAppend(const char* s) {
  if (!s) return;
  const size_t sl = strlen(s);
  if (sl == 0) return;

  const size_t cap = sizeof(gDbgBuf) - 1;

  if (sl >= cap) {
    memcpy(gDbgBuf, s + (sl - cap), cap);
    gDbgBuf[cap] = '\0';
    gDbgLen = cap;
    return;
  }

  if (gDbgLen + sl > cap) {
    size_t drop = (gDbgLen + sl) - cap;
    if (drop > gDbgLen) drop = gDbgLen;
    memmove(gDbgBuf, gDbgBuf + drop, gDbgLen - drop);
    gDbgLen -= drop;
    gDbgBuf[gDbgLen] = '\0';
  }

  memcpy(gDbgBuf + gDbgLen, s, sl);
  gDbgLen += sl;
  gDbgBuf[gDbgLen] = '\0';
}

void dbgPrintf(const char* fmt, ...) {
  char tmp[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);

  Serial.print(tmp);
  dbgAppend(tmp);
}

const char* dbgText() {
  return gDbgBuf;
}
