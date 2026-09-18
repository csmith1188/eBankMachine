#pragma once

void dropStart(int count, bool timing = false, bool debugAll = false);
void dropTick();
void dropAbort();
bool dropActive();
bool dropIsDebugAll();
int dropTarget();
int dropDropped();
int dropRemaining();
