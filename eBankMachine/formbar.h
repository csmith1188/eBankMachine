#pragma once

#include <Arduino.h>

enum FbErr {
  FB_OK = 0,
  FB_NO_WIFI,
  FB_BEGIN_FAIL,
  FB_POST_FAIL,
  FB_HTTP_FAIL,
  FB_JSON_FAIL,
  FB_API_REJECT
};

const char* fbErrMsg(FbErr e);

bool formbarTransfer(int from, int to, int amount, const char* reason, int pin,
                     String& outResp, int& outHttp, FbErr& outErr);

bool formbarUserExists(int id, char* nameOut, size_t nameLen, int& outHttp);
bool lookupUserInteractive(long id);
