#include "eBankMachine.h"

const char* fbErrMsg(FbErr e) {
  switch (e) {
    case FB_NO_WIFI: return "Network issue";
    case FB_BEGIN_FAIL: return "Network issue";
    case FB_POST_FAIL: return "Network issue";
    case FB_HTTP_FAIL: return "Server issue";
    case FB_JSON_FAIL: return "Bad reply";
    case FB_API_REJECT: return "PIN/ID/balance";
    default: return "OK";
  }
}

bool formbarTransferEx(int from, int to, int amount, const char* reason, int pin,
                       String& outResp, int& outHttp, FbErr& outErr) {
  outResp = "";
  outHttp = 0;
  outErr = FB_OK;

  if (WiFi.status() != WL_CONNECTED) {
    outErr = FB_NO_WIFI;
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(10000);

  if (!https.begin(client, TRANSFER_URL)) {
    outResp = "begin_failed";
    outHttp = -1;
    outErr = FB_BEGIN_FAIL;
    return false;
  }

  https.addHeader("API", API_KEY);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> body;
  body["from"] = from;
  body["to"] = to;
  body["amount"] = amount;
  body["reason"] = reason;
  body["pin"] = pin;
  body["pool"] = false;

  String payload;
  serializeJson(body, payload);

  outHttp = https.POST(payload);
  outResp = https.getString();
  https.end();

  // <=0 = connection failure / timeout / could not reach
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

  bool success = (doc["success"] | false);
  if (!success) {
    outErr = FB_API_REJECT;
    return false;
  }

  outErr = FB_OK;
  return true;
}

// OPTIONAL: keep old signature so you don't have to update everything at once.
// This will still work, but you lose the specific reason unless you use Ex().
bool formbarTransfer(int from, int to, int amount, const char* reason, int pin, String& outResp, int& outHttp) {
  FbErr err;
  return formbarTransferEx(from, to, amount, reason, pin, outResp, outHttp, err);
}

bool trySendRefundNow() {
  if (!refundPending || refundToId <= 0 || refundDigipogs <= 0) return true;

  wifiEnsureConnected();
  if (WiFi.status() != WL_CONNECTED) return false;

  String resp;
  int httpc = 0;
  FbErr err;

  bool ok = formbarTransferEx(
    KIOSK_ID,
    (int)refundToId,
    refundDigipogs,
    "refund",
    KIOSK_ACCOUNT_PIN,
    resp,
    httpc,
    err);

  if (ok) {
    refundPending = false;
    refundToId = 0;
    refundDigipogs = 0;
    dbgPrintf("Refund OK\n");
    return true;
  }

  dbgPrintf("Refund FAIL err=%d http=%d resp=%s\n", (int)err, httpc, resp.c_str());
  return false;
}

/*
  NEW: ID validation
  GET: {USER_LOOKUP_BASE}{id}
  Return true if 2xx. Try to extract name/username if JSON contains it.
*/
bool formbarUserExists(int id, String& outName, int& outHttp) {
  outName = "";
  outHttp = 0;

  if (id <= 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String(USER_LOOKUP_BASE) + String(id);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(8000);

  if (!https.begin(client, url)) {
    outHttp = -1;
    return false;
  }

  // If your API doesn’t require this header for /api/user, it can ignore it.
  https.addHeader("API", API_KEY);

  outHttp = https.GET();
  String resp = https.getString();
  https.end();

  if (outHttp <= 0) return false;
  if (outHttp < 200 || outHttp >= 300) return false;

  // Try parse JSON and grab a name field
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

    if (name) outName = String(name);
  }

  return true;
}