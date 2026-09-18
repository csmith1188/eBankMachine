#include "nfc.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "hardware.h"
#include "ui.h"
#include "util.h"

#include <cstring>

static void sanitize(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0) return;
  if (src != dst) copyFit(dst, dstLen, src);
  trimInPlace(dst);
  if (!dst[0] && dstLen >= 2) {
    dst[0] = '0';
    dst[1] = '\0';
  }
}

static bool addUriRecord(uint8_t* buf, int& idx, int cap, const char* uri, bool mb, bool me) {
  const int uriLen = (int)strlen(uri);
  const int need = 5 + uriLen;
  if (idx + need > cap) return false;

  uint8_t header = 0x11;  // TNF well-known + SR
  if (mb) header |= 0x80;
  if (me) header |= 0x40;

  buf[idx++] = header;
  buf[idx++] = 0x01;
  buf[idx++] = (uint8_t)(uriLen + 1);
  buf[idx++] = 0x55;  // 'U'
  buf[idx++] = 0x00;
  memcpy(buf + idx, uri, uriLen);
  idx += uriLen;
  return true;
}

static bool addTextRecord(uint8_t* buf, int& idx, int cap, const char* text, bool mb, bool me) {
  const int textLen = (int)strlen(text);
  const int need = 7 + textLen;
  if (idx + need > cap) return false;

  uint8_t header = 0x11;
  if (mb) header |= 0x80;
  if (me) header |= 0x40;

  buf[idx++] = header;
  buf[idx++] = 0x01;
  buf[idx++] = (uint8_t)(textLen + 3);
  buf[idx++] = 0x54;  // 'T'
  buf[idx++] = 0x02;
  buf[idx++] = 'e';
  buf[idx++] = 'n';
  memcpy(buf + idx, text, textLen);
  idx += textLen;
  return true;
}

bool ntagWriteFullCard(const char* website, const char* name, long formbarId, long formbarPin) {
  char w[48];
  char n[32];
  char idStr[12];
  char pinStr[12];
  sanitize(w, sizeof(w), website);
  sanitize(n, sizeof(n), name);
  snprintf(idStr, sizeof(idStr), "%ld", formbarId > 0 ? formbarId : 0L);
  snprintf(pinStr, sizeof(pinStr), "%ld", formbarPin > 0 ? formbarPin : 0L);

  uint8_t buf[200];
  int idx = 0;
  buf[idx++] = 0x03;
  const int lenPos = idx++;

  if (!addUriRecord(buf, idx, (int)sizeof(buf) - 2, w, true, false)) return false;
  if (!addTextRecord(buf, idx, (int)sizeof(buf) - 2, n, false, false)) return false;
  if (!addTextRecord(buf, idx, (int)sizeof(buf) - 2, idStr, false, false)) return false;
  if (!addTextRecord(buf, idx, (int)sizeof(buf) - 2, pinStr, false, true)) return false;
  if (idx + 1 > (int)sizeof(buf)) return false;

  buf[lenPos] = (uint8_t)(idx - (lenPos + 1));
  buf[idx++] = 0xFE;
  while (idx % 4 != 0) {
    if (idx >= (int)sizeof(buf)) return false;
    buf[idx++] = 0x00;
  }

  uint8_t zeros[4] = {0, 0, 0, 0};
  for (int p = 4; p < 4 + 40; p++) {
    if (!nfc.ntag2xx_WritePage(p, zeros)) return false;
  }
  for (int i = 0; i < idx; i += 4) {
    if (!nfc.ntag2xx_WritePage(4 + (i / 4), buf + i)) return false;
  }
  return true;
}

bool ntagWriteIdText(long id) {
  return ntagWriteFullCard("0", "0", id, 0);
}

static void appendAscii(char* dst, size_t cap, const uint8_t* src, int len) {
  size_t n = strlen(dst);
  for (int i = 0; i < len && n + 1 < cap; i++) {
    dst[n++] = (char)src[i];
  }
  dst[n] = '\0';
}

static bool ntagReadCard(char* website, size_t webLen, char* name, size_t nameLen, long& outId, long& outPin) {
  copyFit(website, webLen, "0");
  copyFit(name, nameLen, "0");
  outId = 0;
  outPin = 0;

  uint8_t raw[64];
  int ridx = 0;
  uint8_t page[4];
  for (int p = 4; p < 4 + 16; p++) {
    if (!nfc.ntag2xx_ReadPage(p, page)) return false;
    memcpy(raw + ridx, page, 4);
    ridx += 4;
  }

  int i = 0;
  while (i < ridx && raw[i] != 0x03) i++;
  if (i >= ridx - 2) return false;

  const int tlvLen = raw[i + 1];
  int pos = i + 2;
  int end = pos + tlvLen;
  if (end > ridx) end = ridx;

  int recNum = 0;
  while (pos < end && recNum < 4) {
    const uint8_t hdr = raw[pos++];
    if ((hdr & 0x10) == 0) return false;
    if (pos + 2 > end) return false;

    const uint8_t typeLen = raw[pos++];
    const uint8_t payloadLen = raw[pos++];
    if (pos + typeLen > end) return false;
    const uint8_t type0 = raw[pos];
    pos += typeLen;
    if (pos + payloadLen > end) return false;

    recNum++;
    if (type0 == 0x55 && payloadLen >= 1) {
      char uri[48] = {0};
      appendAscii(uri, sizeof(uri), raw + pos + 1, payloadLen - 1);
      sanitize(uri, sizeof(uri), uri);
      if (recNum == 1) copyFit(website, webLen, uri);
    } else if (type0 == 0x54 && payloadLen >= 1) {
      const uint8_t langLen = raw[pos] & 0x3F;
      const int textStart = 1 + langLen;
      if (textStart < payloadLen) {
        char text[48] = {0};
        appendAscii(text, sizeof(text), raw + pos + textStart, payloadLen - textStart);
        sanitize(text, sizeof(text), text);
        if (recNum == 2) copyFit(name, nameLen, text);
        else if (recNum == 3) outId = atol(text);
        else if (recNum == 4) outPin = atol(text);
      }
    }
    pos += payloadLen;
  }

  if (outId < 0) outId = 0;
  if (outPin < 0) outPin = 0;
  return true;
}

bool ntagTryReadIdText(long& outId) {
  char w[8], n[8];
  long id = 0, pin = 0;
  const bool ok = ntagReadCard(w, sizeof(w), n, sizeof(n), id, pin);
  outId = ok ? id : 0;
  return ok && outId > 0;
}

enum class NfcWriteState : uint8_t {
  EnterId,
  WaitHold,
  WaitRemove
};

static NfcWriteState nfcState = NfcWriteState::EnterId;
static long nfcId = 0;
static bool holdActive = false;
static uint32_t holdStartMs = 0;

void startNfcWriteFlow() {
  appSetMode(TradeMode::NfcWrite);
  nfcState = NfcWriteState::EnterId;
  nfcId = 0;
  holdActive = false;
  holdStartMs = 0;
  uiEntry(F("Write Card ID"));
}

void handleNfcWriteKey(char k) {
  if (appMode() != TradeMode::NfcWrite) return;

  if (k == '*') {
    appGoMenu();
    return;
  }
  if (nfcState != NfcWriteState::EnterId) return;

  if (k >= '0' && k <= '9') {
    uiAcceptDigit(k, false);
    return;
  }
  if (k != '#') return;

  const long val = Entry::value();
  if (val <= 0) {
    uiShow("Invalid ID", nullptr, 1200);
    uiEntry(F("Write Card ID"));
    return;
  }

  nfcId = val;
  nfcState = NfcWriteState::WaitHold;
  holdActive = false;
  holdStartMs = 0;
  uiNfcHold();
}

void nfcWriteTick() {
  if (appMode() != TradeMode::NfcWrite) return;

  static uint32_t lastPoll = 0;
  const uint32_t now = millis();
  if (now - lastPoll < kNfcPollMs) return;
  lastPoll = now;

  if (nfcState != NfcWriteState::WaitHold && nfcState != NfcWriteState::WaitRemove) return;

  uint8_t uid[8];
  uint8_t uidLen = 0;
  const bool present = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, kNfcTimeoutMs);

  if (nfcState == NfcWriteState::WaitHold) {
    if (!present) {
      holdActive = false;
      holdStartMs = 0;
      return;
    }
    if (!holdActive) {
      holdActive = true;
      holdStartMs = now;
    }
    if (now - holdStartMs < kNfcHoldMs) return;

    uiShow("Writing...", nullptr);
    const bool okWrite = ntagWriteFullCard("0", "0", nfcId, 0);
    long readId = 0;
    const bool okRead = ntagTryReadIdText(readId);

    if (!okWrite) {
      uiShow("WRITE FAIL", "Tag locked?", 2000);
      dbgPrintf("NFC write FAIL id=%ld\n", nfcId);
      appGoMenu();
      return;
    }
    if (!okRead || readId != nfcId) {
      char l1[kLcdLineCap];
      snprintf(l1, sizeof(l1), "read=%ld", readId);
      uiShow("VERIFY FAIL", l1, 2500);
      dbgPrintf("NFC verify FAIL wrote=%ld read=%ld okRead=%d\n", nfcId, readId, (int)okRead);
      appGoMenu();
      return;
    }

    uiShow("WRITE OK", "REMOVE CARD");
    dbgPrintf("NFC write+verify OK id=%ld\n", nfcId);
    nfcState = NfcWriteState::WaitRemove;
    holdActive = false;
    holdStartMs = 0;
    return;
  }

  if (nfcState == NfcWriteState::WaitRemove && !present) {
    appGoMenu();
  }
}
