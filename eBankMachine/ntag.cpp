// ============================
// ntag.cpp
// ============================
#include "eBankMachine.h"

// ----------------------------
// Helpers: sanitize missing fields
// ----------------------------
static String s0(const String& in) {
  String t = in;
  t.replace("\r", "");
  t.replace("\n", "");
  t.trim();
  if (t.length() == 0) return "0";
  return t;
}

// ----------------------------
// NDEF record builders (short records)
// ----------------------------

// Add a Well-Known URI record (Type 'U').
// Payload = [prefixCode][uriString...]
// We'll use prefixCode 0x00 (no prefix).
static void addUriRecord(uint8_t* buf, int& idx, const String& uri, bool mb, bool me) {
  String u = s0(uri);

  uint8_t header = 0x11;   // TNF=WellKnown
  header |= 0x10;          // SR (short record)
  if (mb) header |= 0x80;  // MB
  if (me) header |= 0x40;  // ME

  buf[idx++] = header;
  buf[idx++] = 0x01;                       // type length
  buf[idx++] = (uint8_t)(u.length() + 1);  // payload length
  buf[idx++] = 0x55;                       // 'U'
  buf[idx++] = 0x00;                       // prefix code: none

  memcpy(buf + idx, u.c_str(), u.length());
  idx += u.length();
}

// Add a Well-Known Text record (Type 'T') in English.
static void addTextRecord(uint8_t* buf, int& idx, const String& text, bool mb, bool me) {
  String t = s0(text);

  uint8_t header = 0x11;   // TNF=WellKnown
  header |= 0x10;          // SR
  if (mb) header |= 0x80;  // MB
  if (me) header |= 0x40;  // ME

  // Text payload: [status][lang...][text...]
  // status: UTF-8 + langLen=2 => 0x02
  buf[idx++] = header;
  buf[idx++] = 0x01;                           // type length
  buf[idx++] = (uint8_t)(t.length() + 1 + 2);  // payload length
  buf[idx++] = 0x54;                           // 'T'
  buf[idx++] = 0x02;                           // status: langLen=2, UTF-8
  buf[idx++] = 'e';
  buf[idx++] = 'n';

  memcpy(buf + idx, t.c_str(), t.length());
  idx += t.length();
}

// ----------------------------
// Write full 4-record card
// Record order:
// 1) Website URL (URI)
// 2) Owner name (Text)
// 3) Formbar ID (Text)
// 4) Formbar PIN (Text)
// Missing fields -> "0"
// ----------------------------
bool ntagWriteFullCard(const String& website, const String& name, long formbarId, long formbarPin) {
  String w = s0(website);
  String n = s0(name);
  String id = (formbarId > 0) ? String(formbarId) : String("0");
  String pin = (formbarPin > 0) ? String(formbarPin) : String("0");

  uint8_t buf[200];
  int idx = 0;

  // TLV start
  buf[idx++] = 0x03;   // NDEF TLV
  int lenPos = idx++;  // length placeholder

  // 4 records
  addUriRecord(buf, idx, w, true, false);     // MB
  addTextRecord(buf, idx, n, false, false);
  addTextRecord(buf, idx, id, false, false);
  addTextRecord(buf, idx, pin, false, true);  // ME

  // TLV length (1 byte)
  buf[lenPos] = (uint8_t)(idx - (lenPos + 1));

  // TLV terminator
  buf[idx++] = 0xFE;

  // pad to 4 bytes for page writes
  while (idx % 4 != 0) buf[idx++] = 0x00;

  // wipe first 40 pages (160 bytes) starting at page 4
  uint8_t zeros[4] = {0, 0, 0, 0};
  for (int p = 4; p < 4 + 40; p++) {
    if (!nfc.ntag2xx_WritePage(p, zeros)) return false;
  }

  // write new NDEF
  for (int i = 0; i < idx; i += 4) {
    if (!nfc.ntag2xx_WritePage(4 + (i / 4), buf + i)) return false;
  }

  return true;
}

// Convenience wrapper if you still call it elsewhere
bool ntagWriteIdText(long id) {
  return ntagWriteFullCard("0", "0", id, 0);
}

// ----------------------------
// Minimal readback: try to find record 3 (ID) as digits
// NOTE: This parser reads first 64 bytes only, fine for short cards.
// ----------------------------
static bool ntagReadCard(String& outWebsite, String& outName, long& outId, long& outPin) {
  outWebsite = "0";
  outName = "0";
  outId = 0;
  outPin = 0;

  uint8_t raw[64];
  int ridx = 0;
  uint8_t page[4];

  for (int p = 4; p < 4 + 16; p++) {
    if (!nfc.ntag2xx_ReadPage(p, page)) return false;
    for (int i = 0; i < 4; i++) raw[ridx++] = page[i];
  }

  // Find TLV 0x03
  int i = 0;
  while (i < ridx && raw[i] != 0x03) i++;
  if (i >= ridx - 2) return false;

  int tlvLen = raw[i + 1];
  int pos = i + 2;
  int end = pos + tlvLen;
  if (end > ridx) end = ridx;

  int recNum = 0;

  while (pos < end && recNum < 4) {
    uint8_t hdr = raw[pos++];
    bool sr = (hdr & 0x10) != 0;
    if (!sr) return false;

    if (pos + 2 > end) return false;
    uint8_t typeLen = raw[pos++];
    uint8_t payloadLen = raw[pos++];

    if (pos + typeLen > end) return false;

    uint8_t type0 = raw[pos];
    pos += typeLen;

    if (pos + payloadLen > end) return false;

    recNum++;

    if (type0 == 0x55) {  // 'U'
      if (payloadLen >= 1) {
        // skip prefix
        String uri;
        for (int k = 1; k < payloadLen; k++) uri += (char)raw[pos + k];
        if (recNum == 1) outWebsite = s0(uri);
      }
    } else if (type0 == 0x54) {  // 'T'
      if (payloadLen >= 1) {
        uint8_t status = raw[pos];
        uint8_t langLen = status & 0x3F;
        int textStart = 1 + langLen;
        if (textStart < payloadLen) {
          String text;
          for (int k = textStart; k < payloadLen; k++) text += (char)raw[pos + k];
          text = s0(text);

          if (recNum == 2) outName = text;
          else if (recNum == 3) outId = text.toInt();
          else if (recNum == 4) outPin = text.toInt();
        }
      }
    }

    pos += payloadLen;
  }

  outWebsite = s0(outWebsite);
  outName = s0(outName);
  if (outId < 0) outId = 0;
  if (outPin < 0) outPin = 0;

  return true;
}

bool ntagTryReadIdText(long& outId) {
  String w, n;
  long id = 0, pin = 0;
  bool ok = ntagReadCard(w, n, id, pin);
  outId = ok ? id : 0;
  return ok && outId > 0;
}

// ----------------------------
// NFC Write Mode: D -> enter ID -> HOLD -> write -> verify -> REMOVE
// ----------------------------
void startNfcWriteFlow() {
  tradeMode = MODE_NFC_WRITE;
  nfcwState = NFCW_ENTER_ID;
  nfcwId = 0;
  nfcwHoldActive = false;
  nfcwHoldStartMs = 0;

  showEntry(F("Write Card ID"));
}

void handleNfcWriteKey(char k) {
  if (tradeMode != MODE_NFC_WRITE) return;

  // Cancel at any stage
  if (k == '*') {
    tradeMode = MODE_SELECT;
    nfcwState = NFCW_ENTER_ID;
    nfcwId = 0;
    nfcwHoldActive = false;
    nfcwHoldStartMs = 0;
    showModeMenu();
    return;
  }

  if (nfcwState != NFCW_ENTER_ID) return;

  // Digits
  if (k >= '0' && k <= '9') {
    if (numLen < (int)sizeof(numBuf) - 1) {
      numBuf[numLen++] = k;
      numBuf[numLen] = '\0';
      lcd.setCursor(7 + (numLen - 1), 1);
      lcd.print(k);
    }
    return;
  }

  // Confirm ID entry
  if (k == '#') {
    long val = (numLen > 0) ? atol(numBuf) : 0;
    if (val <= 0) {
      showMsg("Invalid ID", nullptr, 1200);
      showEntry(F("Write Card ID"));
      clearEntryLine();
      return;
    }

    nfcwId = val;
    clearEntryLine();

    nfcwState = NFCW_WAIT_HOLD;
    nfcwHoldActive = false;
    nfcwHoldStartMs = 0;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("HOLD CARD..."));
    lcd.setCursor(0, 1);
    lcd.print(F("*=cancel"));
    return;
  }
}

void nfcWriteTick() {
  if (tradeMode != MODE_NFC_WRITE) return;

  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll < NFC_POLL_MS) return;
  lastPoll = now;

  if (!(nfcwState == NFCW_WAIT_HOLD || nfcwState == NFCW_WAIT_REMOVE)) return;

  uint8_t uid[8];
  uint8_t uidLen = 0;

  bool present = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A, uid, &uidLen, PN532_TIMEOUT_MS
  );

  // WAIT_HOLD: must be present continuously for 600ms
  if (nfcwState == NFCW_WAIT_HOLD) {
    if (present) {
      if (!nfcwHoldActive) {
        nfcwHoldActive = true;
        nfcwHoldStartMs = now;
      }

      if (now - nfcwHoldStartMs >= 600) {
        showMsg("Writing...", nullptr, 0);

        // Write full format with placeholders
        bool okWrite = ntagWriteFullCard("0", "0", nfcwId, 0);

        // Verify by reading back record 3 (ID)
        long readId = 0;
        bool okRead = ntagTryReadIdText(readId);

        if (!okWrite) {
          showMsg("WRITE FAIL", "Tag locked?", 2000);
          dbgPrintf("NFC write FAIL id=%ld\n", nfcwId);
          tradeMode = MODE_SELECT;
          showModeMenu();
          return;
        }

        if (!okRead || readId != nfcwId) {
          char l1[17];
          snprintf(l1, sizeof(l1), "read=%ld", readId);
          showMsg("VERIFY FAIL", l1, 2500);
          dbgPrintf("NFC verify FAIL wrote=%ld read=%ld okRead=%d\n",
                    nfcwId, readId, (int)okRead);
          tradeMode = MODE_SELECT;
          showModeMenu();
          return;
        }

        showMsg("WRITE OK", "REMOVE CARD", 0);
        dbgPrintf("NFC write+verify OK id=%ld\n", nfcwId);

        nfcwState = NFCW_WAIT_REMOVE;
        nfcwHoldActive = false;
        nfcwHoldStartMs = 0;
        return;
      }
    } else {
      nfcwHoldActive = false;
      nfcwHoldStartMs = 0;
    }
    return;
  }

  // WAIT_REMOVE: wait until tag is gone
  if (nfcwState == NFCW_WAIT_REMOVE) {
    if (!present) {
      tradeMode = MODE_SELECT;
      showModeMenu();
    }
    return;
  }
}