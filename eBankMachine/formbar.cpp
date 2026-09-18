#include "formbar.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "net.h"
#include "ui.h"
#include "util.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* fbErrMsg(FbErr e) {
  switch (e) {
    case FB_NO_WIFI:
    case FB_BEGIN_FAIL:
    case FB_POST_FAIL:
      return "Network issue";
    case FB_HTTP_FAIL:
      return "Server issue";
    case FB_JSON_FAIL:
      return "Bad reply";
    case FB_API_REJECT:
      return "PIN/ID/balance";
    default:
      return "OK";
  }
}

static bool httpsBegin(HTTPClient& https, WiFiClientSecure& client, const char* url, uint32_t timeoutMs) {
  client.setInsecure();
  https.setTimeout(timeoutMs);
  if (!https.begin(client, url)) return false;
  https.addHeader("API", API_KEY);
  return true;
}

bool formbarTransfer(int from, int to, int amount, const char* reason, int pin,
                     String& outResp, int& outHttp, FbErr& outErr) {
  outResp = "";
  outHttp = 0;
  outErr = FB_OK;

  if (!netConnected()) {
    outErr = FB_NO_WIFI;
    return false;
  }

  WiFiClientSecure client;
  HTTPClient https;
  if (!httpsBegin(https, client, TRANSFER_URL, kHttpTimeoutMs)) {
    outResp = "begin_failed";
    outHttp = -1;
    outErr = FB_BEGIN_FAIL;
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> body;
  body["from"] = from;
  body["to"] = to;
  body["amount"] = amount;
  body["reason"] = reason ? reason : "";
  body["pin"] = pin;
  body["pool"] = false;

  String payload;
  serializeJson(body, payload);

  outHttp = https.POST(payload);
  outResp = https.getString();
  https.end();

  if (outHttp <= 0) {
    outErr = FB_POST_FAIL;
    return false;
  }
  if (outHttp < 200 || outHttp >= 300) {
    outErr = FB_HTTP_FAIL;
    return false;
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, outResp)) {
    outErr = FB_JSON_FAIL;
    return false;
  }

  if (!(doc["success"] | false)) {
    outErr = FB_API_REJECT;
    return false;
  }

  outErr = FB_OK;
  return true;
}

bool formbarUserExists(int id, char* nameOut, size_t nameLen, int& outHttp) {
  if (nameOut && nameLen) nameOut[0] = '\0';
  outHttp = 0;

  if (id <= 0 || !netConnected()) return false;

  char url[160];
  snprintf(url, sizeof(url), "%s%d", USER_LOOKUP_BASE, id);

  WiFiClientSecure client;
  HTTPClient https;
  if (!httpsBegin(https, client, url, kHttpLookupTimeoutMs)) {
    outHttp = -1;
    return false;
  }

  outHttp = https.GET();
  const String resp = https.getString();
  https.end();

  if (outHttp <= 0 || outHttp < 200 || outHttp >= 300) return false;

  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, resp) == DeserializationError::Ok) {
    const char* name = nullptr;
    if (doc["username"].is<const char*>()) name = doc["username"].as<const char*>();
    else if (doc["name"].is<const char*>()) name = doc["name"].as<const char*>();
    else if (doc["user"].is<JsonObject>()) {
      JsonObject u = doc["user"].as<JsonObject>();
      if (u["username"].is<const char*>()) name = u["username"].as<const char*>();
      else if (u["name"].is<const char*>()) name = u["name"].as<const char*>();
    }
    if (name && nameOut && nameLen) copyFit(nameOut, nameLen, name);
  }

  return true;
}

bool lookupUserInteractive(long id) {
  netEnsureConnected();
  if (!netConnected()) {
    uiShow("No WiFi", "Try again", 1500);
    return false;
  }

  uiShow("Checking ID", "Please wait");

  char name[kNameCap + 1];
  int httpc = 0;
  if (!formbarUserExists((int)id, name, sizeof(name), httpc)) {
    if (httpc == 404) uiShow("ID Not Found", "Try again", 1600);
    else uiShow("Bad ID/WiFi", "Try again", 1600);
    return false;
  }

  stashName(name);
  return true;
}
