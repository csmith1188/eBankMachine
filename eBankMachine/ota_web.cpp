#include "eBankMachine.h"

extern const char* __dbgTextForWeb();  // from globals.cpp

static const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr><td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center><br>"
  "</td></tr>"
  "<tr><td>Username:</td><td><input type='text' size=25 name='userid'><br></td></tr>"
  "<tr><td>Password:</td><td><input type='Password' size=25 name='pwd'><br></td></tr>"
  "<tr><td><input type='submit' onclick='check(this.form)' value='Login'></td></tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form){"
  "if(form.userid.value=='admin' && form.pwd.value=='admin'){window.open('/serverIndex')}"
  "else{alert('Error Password or Username')}}"
  "</script>";

static const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<h3 style='text-align:center;'>ESP32 Web OTA Updater</h3>"
  "<div style='text-align:center; margin-bottom:10px;'>"
  "<a href='/debug' target='_blank'>Open Debug Log</a>"
  "</div>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form' style='text-align:center;'>"
  "<div style='margin:10px;'><input type='password' name='otapass' placeholder='OTA Password'/></div>"
  "<div style='margin:10px;'><input type='file' name='update'/></div>"
  "<div style='margin:10px;'><input type='submit' value='Update'/></div>"
  "</form>"
  "<div id='prg' style='text-align:center;'>progress: 0%</div>"
  "<div id='msg' style='text-align:center;'></div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form=$('#upload_form')[0];"
  "var data=new FormData(form);"
  "$.ajax({"
  "url:'/update',type:'POST',data:data,contentType:false,processData:false,"
  "xhr:function(){var xhr=new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress',function(evt){"
  "if(evt.lengthComputable){"
  "var per=evt.loaded/evt.total;"
  "$('#prg').html('progress: '+Math.round(per*100)+'%');"
  "}},false);return xhr;},"
  "success:function(d,s){$('#msg').html(d);},"
  "error:function(){$('#msg').html('Upload error');}"
  "});"
  "});"
  "</script>";

static bool otaUploadAuthed = false;

void otaDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    delay(1);
  }
}

void setupWebOtaOnce() {
  if (otaStarted) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!MDNS.begin(OTA_HOST)) {
    dbgPrintf("mDNS start FAILED\n");
  } else {
    dbgPrintf("mDNS started\n");
  }

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  // ===== Debug endpoints (same port 80) =====
server.on("/debug.txt", HTTP_GET, []() {
  String t = __dbgTextForWeb();

  t += "\n\nINVENTORY\n";
  t += "Inventory: ";
  t += String(currencyCount);
  t += " / ";
  t += String(MAX_CURRENCY_CAPACITY);
  t += "\n";

  t += "LowStock: ";
  t += (currencyCount <= LOW_STOCK_THRESHOLD) ? "YES" : "NO";
  t += "\n";

  t += "\nDROP DEBUG\n";
  t += "DroppedCount: ";
  t += String(droppedCount);
  t += "\n";

  t += "TargetDrops: ";
  t += String(targetDrops);
  t += "\n";

  t += "MotionState: ";
  t += String((int)motionState);
  t += "\n";

  server.send(200, "text/plain", t);
});

  server.on("/debug/clear", HTTP_POST, []() {
    dbgClear();
    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  server.on("/reboot", HTTP_POST, []() {
    server.send(200, "text/plain", "rebooting");
    delay(150);
    ESP.restart();
  });

  // Servo UP for 10 seconds
  server.on("/debug/up10", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: servo UP 10s\n");
    showMsg("Servo UP", "10 seconds", 600);

    servoAttach();
    myServo.writeMicroseconds(SERVO_UP_US);

    unsigned long start = millis();
    while (millis() - start < 10000UL) {
      delay(1);
    }

    servoStopDetach();

    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  // Drop exactly 1 pog
  server.on("/debug/drop1", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: drop 1 requested\n");
    showMsg("WEB DROP 1", "Starting...", 600);

    startDrop(1);

    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  server.on("/debug/resetinventory", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: reset inventory requested\n");
    showMsg("RESET INVENTORY", "Starting...", 600);

    currencyCount = MAX_CURRENCY_CAPACITY;

    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);

  });

  // Drop a huge count (effectively "all")
  server.on("/debug/dropall", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: drop all requested (timed)\n");
    showMsg("WEB DROP ALL", "Timing...", 600);

    dbgDropTimingEnabled = true;
    dbgDropAllAutoStop = true;

    startDrop(999);  // big number; auto-stop will end it when hopper is done

    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  // Sample IR sensors for 10 seconds, log values, then return to normal menu
  server.on("/debug/irscan10", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: IR scan 10s start (thr drop=%d dep=%d)\n", IR_DROP_THRESHOLD, IR_DEP_THRESHOLD);
    showMsg("IR scan", "10s...", 0);

    unsigned long startMs = millis();
    unsigned long lastLog = 0;
    int dropMin = 4096, dropMax = 0;
    int depMin = 4096, depMax = 0;
    long dropSum = 0, depSum = 0;
    long samples = 0;

    while (millis() - startMs < 10000UL) {
      int vd = analogRead(IR_DROP_PIN);
      int vp = analogRead(IR_DEP_PIN);

      if (vd < dropMin) dropMin = vd;
      if (vd > dropMax) dropMax = vd;
      if (vp < depMin) depMin = vp;
      if (vp > depMax) depMax = vp;

      dropSum += vd;
      depSum += vp;
      samples++;

      unsigned long now = millis();
      if (now - lastLog >= 250UL) {
        lastLog = now;
        dbgPrintf("IR live drop=%d dep=%d\n", vd, vp);
      }

      otaDelay(25);
    }

    long dropAvg = (samples > 0) ? (dropSum / samples) : 0;
    long depAvg = (samples > 0) ? (depSum / samples) : 0;

    dbgPrintf("WEB: IR scan done samples=%ld drop(min/avg/max)=%d/%ld/%d dep(min/avg/max)=%d/%ld/%d\n",
              samples, dropMin, dropAvg, dropMax, depMin, depAvg, depMax);

    showMsg("IR scan done", "Back to menu", 900);
    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  // Look for NFC for 10 seconds; show/log ID:#### if found; back out on timeout
  server.on("/debug/nfcid10", HTTP_POST, []() {
    if (motionState != MS_IDLE) {
      server.send(409, "text/plain", "busy");
      return;
    }

    dbgPrintf("WEB: NFC ID scan 10s start\n");
    showMsg("Tap NFC card", "10s timeout", 0);

    unsigned long startMs = millis();
    bool found = false;
    long id = 0;

    while (millis() - startMs < 10000UL) {
      uint8_t uid[8];
      uint8_t uidLen = 0;

      if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, PN532_TIMEOUT_MS)) {
        long parsed = 0;
        if (ntagTryReadIdText(parsed)) {
          id = parsed;
          found = true;
          break;
        } else {
          dbgPrintf("NFC: tag present but no ID:#### found\n");
          showMsg("No ID on tag", "Try another", 1200);
        }
      }

      otaDelay(50);
    }

    if (found) {
      char line1[16];
      snprintf(line1, sizeof(line1), "ID:%ld", id);
      dbgPrintf("WEB: NFC found %s\n", line1);
      showMsg("Card ID", line1, 2000);
    } else {
      dbgPrintf("WEB: NFC scan timeout (no tag)\n");
      showMsg("NFC timeout", "Back to menu", 1200);
    }

    tradeMode = MODE_SELECT;
    showModeMenu();

    server.sendHeader("Location", "/debug");
    server.send(303);
  });

  server.on("/debug", HTTP_GET, []() {
    String page =
      "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'/>"
      "<title>eBank Debug</title></head>"
      "<body style='font-family:monospace;'>"
      "<h3>Debug Log</h3>"
      "<form method='POST' action='/debug/clear' style='display:inline;'>"
      "<button type='submit'>Clear</button></form> "
      "<form method='POST' action='/debug/up10' style='display:inline;'>"
      "<button type='submit'>UP 10s</button></form> "
      "<form method='POST' action='/debug/drop1' style='display:inline;'>"
      "<button type='submit'>DROP 1</button></form> "
      "<form method='POST' action='/debug/resetinventory' style='display:inline;'>"
      "<button type='submit'>RESET INVENTORY</button></form> "
      "<form method='POST' action='/debug/dropall' style='display:inline;'>"
      "<button type='submit'>DROP ALL</button></form> "
      "<form method='POST' action='/debug/irscan10' style='display:inline;'>"
      "<button type='submit'>IR scan 10s</button></form> "
      "<form method='POST' action='/debug/nfcid10' style='display:inline;'>"
      "<button type='submit'>NFC ID 10s</button></form> "
      "<form method='POST' action='/reboot' style='display:inline;'>"
      "<button type='submit'>Reboot</button></form>"
      "<pre id='log' style='white-space:pre-wrap;'></pre>"
      "<script>"
      "function tick(){fetch('/debug.txt').then(r=>r.text()).then(t=>{"
      "document.getElementById('log').innerText=t;"
      "window.scrollTo(0,document.body.scrollHeight);"
      "});}"
      "setInterval(tick,1000); tick();"
      "</script>"
      "</body></html>";

    server.send(200, "text/html", page);
  });

  // ===== OTA upload endpoint =====
  server.on(
    "/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      delay(150);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        otaUploadAuthed = server.hasArg("otapass") && (server.arg("otapass") == OTA_PASSWORD);
        if (!otaUploadAuthed) {
          dbgPrintf("OTA denied: bad password\n");
          return;
        }

        dbgPrintf("Update: %s\n", upload.filename.c_str());
        servoStopDetach();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (!otaUploadAuthed) {
        if (Update.isRunning()) Update.abort();
        return;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) dbgPrintf("Update Success: %u\nRebooting...\n", (unsigned)upload.totalSize);
        else Update.printError(Serial);
      }
    });

  server.begin();
  otaStarted = true;

  dbgPrintf("HTTP: http://%s/\n", WiFi.localIP().toString().c_str());
  dbgPrintf("Debug: http://%s/debug\n", WiFi.localIP().toString().c_str());
}

void otaTick() {
  server.handleClient();
}