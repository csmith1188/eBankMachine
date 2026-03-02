// ============================
// nfc_write.cpp
// ============================
#include "eBankMachine.h"

// Write "ID:xxxxx" as one NDEF Text record into NTAG user memory.
// This is basic and works for NTAG213/215/216 if tag is writable.
bool ntagWriteIdText(long id) {
  if (id <= 0) return false;

  String text = String("ID:") + String(id);

  // Build NDEF TLV containing a single Text record:
  // 03 [len] D1 01 [payloadLen] 54 02 'e' 'n' [text...] FE
  uint8_t buf[128];
  int idx = 0;

  buf[idx++] = 0x03; // NDEF TLV

  int lenPos = idx++; // placeholder for length

  buf[idx++] = 0xD1; // MB/ME/SR + TNF=WellKnown
  buf[idx++] = 0x01; // type length
  buf[idx++] = (uint8_t)(text.length() + 3); // payload length
  buf[idx++] = 0x54; // 'T'
  buf[idx++] = 0x02; // language code length
  buf[idx++] = 'e';
  buf[idx++] = 'n';

  memcpy(buf + idx, text.c_str(), text.length());
  idx += text.length();

  // length = everything after len byte up to terminator (exclude 0x03 and len itself)
  buf[lenPos] = (uint8_t)(idx - (lenPos + 1));

  buf[idx++] = 0xFE; // terminator TLV

  // Pad to 4 bytes so page writes don't read past buffer
  while (idx % 4 != 0) buf[idx++] = 0x00;

// ---- Wipe first 16 pages (64 bytes) ----
uint8_t zeros[4] = {0,0,0,0};
for (int p = 4; p < 4 + 16; p++) {
  if (!nfc.ntag2xx_WritePage(p, zeros)) {
    return false;
  }
}

  // Write from page 4 onward
  for (int i = 0; i < idx; i += 4) {
    if (!nfc.ntag2xx_WritePage(4 + (i / 4), buf + i)) {
      return false;
    }
  }
  return true;
}

void startNfcWriteFlow() {
  tradeMode = MODE_NFC_WRITE;
  nfcwState = NFCW_ENTER_ID;
  nfcwId = 0;
  showEntry(F("Write Card ID"));
}

void handleNfcWriteKey(char k) {
  if (tradeMode != MODE_NFC_WRITE) return;

  // Cancel/clear
  if (k == '*') {
    if (nfcwState == NFCW_WAIT_TAP) {
      tradeMode = MODE_SELECT;
      showModeMenu();
      return;
    }

    // entry mode: empty -> menu, else clear
    if (numLen == 0) {
      tradeMode = MODE_SELECT;
      showModeMenu();
    } else {
      clearEntryLine();
    }
    return;
  }

  // Entering ID
  if (nfcwState == NFCW_ENTER_ID) {
    if (k >= '0' && k <= '9') {
      if (numLen < sizeof(numBuf) - 1) {
        numBuf[numLen++] = k;
        numBuf[numLen] = '\0';
        lcd.setCursor(7 + (numLen - 1), 1);
        lcd.print(k);
      }
      return;
    }

    if (k == '#') {
      long val = (numLen > 0) ? atol(numBuf) : 0;
      if (val <= 0) {
        showMsg("Invalid ID", nullptr, 1200);
        showEntry(F("Write Card ID"));
        clearEntryLine();
        return;
      }

      nfcwId = val;
      nfcwState = NFCW_WAIT_TAP;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(F("Tap card now"));
      lcd.setCursor(0,1);
      lcd.print(F("*=cancel"));

      clearEntryLine();
      return;
    }
  }
}

// Poll for tag only when waiting
void nfcWriteTick() {
  if (!(tradeMode == MODE_NFC_WRITE && nfcwState == NFCW_WAIT_TAP)) return;

  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll < NFC_POLL_MS) return;
  lastPoll = now;

  uint8_t uid[8];
  uint8_t uidLen = 0;

  // If a tag is present:
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, PN532_TIMEOUT_MS)) {

    showMsg("Writing...", nullptr, 0);

    bool ok = ntagWriteIdText(nfcwId);

    if (ok) {
      showMsg("Card Write OK", nullptr, 1500);
      dbgPrintf("NFC write OK id=%ld\n", nfcwId);
    } else {
      showMsg("Write FAILED", "Tag locked?", 2000);
      dbgPrintf("NFC write FAIL id=%ld\n", nfcwId);
    }

    // Return to menu
    tradeMode = MODE_SELECT;
    showModeMenu();
  }
}