#pragma once

#include "config.h"

void netInit();
void netTick();
bool netConnected();
void netEnsureConnected(uint32_t timeoutMs = kWifiRetryWaitMs);
void netShowIp();
