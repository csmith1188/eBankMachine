#pragma once

#include <Arduino.h>
#include <cstring>

inline int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline void copyFit(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0) return;
  dst[0] = '\0';
  if (!src) return;

  size_t n = 0;
  while (src[n] && src[n] != '\r' && src[n] != '\n' && n + 1 < dstLen) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = '\0';
}

inline void trimInPlace(char* s) {
  if (!s) return;
  char* start = s;
  while (*start == ' ' || *start == '\t') start++;
  if (start != s) memmove(s, start, strlen(start) + 1);

  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
    s[--n] = '\0';
  }
}

inline long parseLong(const char* s) {
  if (!s || !s[0]) return 0;
  return atol(s);
}

inline int ceilPercent(long amount, int percent) {
  if (amount <= 0 || percent <= 0) return 0;
  return (int)((amount * percent + 99) / 100);
}
