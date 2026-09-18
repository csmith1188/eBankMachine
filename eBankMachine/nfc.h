#pragma once

bool ntagWriteFullCard(const char* website, const char* name, long formbarId, long formbarPin);
bool ntagWriteIdText(long id);
bool ntagTryReadIdText(long& outId);

void startNfcWriteFlow();
void handleNfcWriteKey(char k);
void nfcWriteTick();
