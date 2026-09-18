#include "net.h"

#include "debug_log.h"
#include "ui.h"
#include "web.h"

#include <WiFi.h>

static uint32_t lastAttemptMs = 0;
static uint32_t backoffMs = 2000;
static bool wasConnected = false;

static void logWifiOk() {
  dbgPrintf("WiFi OK IP=%s\n", WiFi.localIP().toString().c_str());
  dbgPrintf("MAC=%s\n", WiFi.macAddress().c_str());
}

void netInit() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastAttemptMs = millis();

  uiShow("WiFi...", "connecting");
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiBootMs) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wasConnected = true;
    logWifiOk();
    webStartOnce();
  } else {
    dbgPrintf("WiFi FAILED\n");
  }
}

bool netConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void netTick() {
  if (netConnected()) {
    if (!wasConnected) {
      wasConnected = true;
      backoffMs = 2000;
      logWifiOk();
    }
    webStartOnce();
    return;
  }

  wasConnected = false;
  const uint32_t now = millis();
  if (now - lastAttemptMs < backoffMs) return;
  lastAttemptMs = now;

  dbgPrintf("WiFi reconnect...\n");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (backoffMs < 30000UL) backoffMs *= 2;
}

void netEnsureConnected(uint32_t timeoutMs) {
  if (netConnected()) {
    webStartOnce();
    return;
  }

  uiShow("WiFi...", "reconnecting");
  dbgPrintf("WiFi reconnect...\n");

  lastAttemptMs = millis();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (!netConnected() && millis() - start < timeoutMs) {
    webTick();
    delay(50);
  }

  if (netConnected()) {
    wasConnected = true;
    backoffMs = 2000;
    logWifiOk();
  }
  webStartOnce();
}

void netShowIp() {
  if (netConnected()) {
    const IPAddress ip = WiFi.localIP();
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    uiShow("IP Address:", buf, 3000);
  } else {
    uiShow("WiFi Not", "Connected", 2000);
  }
}
