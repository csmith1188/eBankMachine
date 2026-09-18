#pragma once

void refundTick();
void refundQueue(long toId, int digipogs);
void refundClear();
bool refundIsPending();
bool refundTryNow();
void refundScheduleRetry();
