#include "web.h"

#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "drop.h"
#include "hardware.h"
#include "inventory.h"
#include "net.h"
#include "nfc.h"
#include "ui.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

WebServer server(80);

static bool started = false;
static bool otaAuthed = false;

enum class WebJob : uint8_t { None, ServoUp, IrScan, NfcScan };
static WebJob job = WebJob::None;
static uint32_t jobStartMs = 0;
static uint32_t jobEndMs = 0;
static uint32_t jobLastLogMs = 0;

static int irDropMin = 0, irDropMax = 0, irDepMin = 0, irDepMax = 0;
static long irDropSum = 0, irDepSum = 0, irSamples = 0;
static bool nfcFound = false;
static long nfcFoundId = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>
<title>eBank OTA</title></head>
<body style='font-family:sans-serif;text-align:center;'>
<h3>ESP32 Web OTA Updater</h3>
<div style='margin-bottom:10px;'><a href='/debug'>Open Debug Log</a></div>
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>
  <div style='margin:10px;'><input type='password' name='otapass' placeholder='OTA Password'/></div>
  <div style='margin:10px;'><input type='file' name='update'/></div>
  <div style='margin:10px;'><input type='submit' value='Update'/></div>
</form>
<div id='prg'>progress: 0%</div>
<div id='msg'></div>
<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>
<script>
$('form').submit(function(e){
  e.preventDefault();
  var data=new FormData($('#upload_form')[0]);
  $.ajax({
    url:'/update', type:'POST', data:data, contentType:false, processData:false,
    xhr:function(){
      var xhr=new window.XMLHttpRequest();
      xhr.upload.addEventListener('progress', function(evt){
        if(evt.lengthComputable) $('#prg').html('progress: '+Math.round(evt.loaded/evt.total*100)+'%');
      }, false);
      return xhr;
    },
    success:function(d){ $('#msg').html(d); },
    error:function(){ $('#msg').html('Upload error'); }
  });
});
</script>
</body></html>
)HTML";

static const char DEBUG_HTML[] PROGMEM = R"HTML(
<html><head><meta name='viewport' content='width=device-width, initial-scale=1'/>
<title>eBank Debug</title></head>
<body style='font-family:monospace;'>
<h3>Debug Log</h3>
<form method='POST' action='/debug/clear' style='display:inline;'><button type='submit'>Clear</button></form>
<form method='POST' action='/debug/up10' style='display:inline;'><button type='submit'>UP 10s</button></form>
<form method='POST' action='/debug/drop1' style='display:inline;'><button type='submit'>DROP 1</button></form>
<form method='POST' action='/debug/resetinventory' style='display:inline;'><button type='submit'>RESET INVENTORY</button></form>
<form method='POST' action='/debug/dropall' style='display:inline;'><button type='submit'>DROP ALL</button></form>
<form method='POST' action='/debug/irscan10' style='display:inline;'><button type='submit'>IR scan 10s</button></form>
<form method='POST' action='/debug/nfcid10' style='display:inline;'><button type='submit'>NFC ID 10s</button></form>
<form method='POST' action='/reboot' style='display:inline;'><button type='submit'>Reboot</button></form>
<pre id='log' style='white-space:pre-wrap;'></pre>
<script>
function tick(){fetch('/debug.txt').then(r=>r.text()).then(t=>{
  document.getElementById('log').innerText=t;
  window.scrollTo(0,document.body.scrollHeight);
});}
setInterval(tick,1000); tick();
</script>
</body></html>
)HTML";

bool webBusy() {
  return job != WebJob::None;
}

static bool requireAuth() {
  if (server.authenticate(WEB_USER, WEB_PASS)) return true;
  server.requestAuthentication();
  return false;
}

static bool machineBusy() {
  return dropActive() || hardwareRecovering() || webBusy();
}

static void redirectDebug() {
  server.sendHeader("Location", "/debug");
  server.send(303);
}

static void startJob(WebJob j, uint32_t ms) {
  job = j;
  jobStartMs = millis();
  jobEndMs = jobStartMs + ms;
  jobLastLogMs = 0;
}

static void endJob() {
  job = WebJob::None;
}

static void finishIrScan() {
  const long dropAvg = irSamples ? (irDropSum / irSamples) : 0;
  const long depAvg = irSamples ? (irDepSum / irSamples) : 0;
  dbgPrintf("WEB: IR scan done samples=%ld drop(min/avg/max)=%d/%ld/%d dep(min/avg/max)=%d/%ld/%d\n",
            irSamples, irDropMin, dropAvg, irDropMax, irDepMin, depAvg, irDepMax);
  endJob();
  uiShow("IR scan done", "Back to menu", 900);
  appGoMenu();
}

static void finishNfcScan() {
  const bool found = nfcFound;
  const long id = nfcFoundId;
  endJob();
  if (found) {
    char line1[kLcdLineCap];
    snprintf(line1, sizeof(line1), "ID:%ld", id);
    dbgPrintf("WEB: NFC found %s\n", line1);
    uiShow("Card ID", line1, 2000);
  } else {
    dbgPrintf("WEB: NFC scan timeout (no tag)\n");
    uiShow("NFC timeout", "Back to menu", 1200);
  }
  appGoMenu();
}

static void jobTick() {
  if (job == WebJob::None) return;
  const uint32_t now = millis();

  if (job == WebJob::ServoUp) {
    if ((int32_t)(now - jobEndMs) >= 0) {
      servoStop();
      appGoMenu();
      endJob();
    }
    return;
  }

  if (job == WebJob::IrScan) {
    const int vd = analogRead(PIN_IR_DROP);
    const int vp = analogRead(PIN_IR_DEP);
    if (vd < irDropMin) irDropMin = vd;
    if (vd > irDropMax) irDropMax = vd;
    if (vp < irDepMin) irDepMin = vp;
    if (vp > irDepMax) irDepMax = vp;
    irDropSum += vd;
    irDepSum += vp;
    irSamples++;
    if (now - jobLastLogMs >= 250) {
      jobLastLogMs = now;
      dbgPrintf("IR live drop=%d dep=%d\n", vd, vp);
    }
    if ((int32_t)(now - jobEndMs) >= 0) finishIrScan();
    return;
  }

  if (job == WebJob::NfcScan) {
    uint8_t uid[8];
    uint8_t uidLen = 0;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, kNfcTimeoutMs)) {
      long parsed = 0;
      if (ntagTryReadIdText(parsed)) {
        nfcFoundId = parsed;
        nfcFound = true;
        finishNfcScan();
        return;
      }
      dbgPrintf("NFC: tag present but no ID:#### found\n");
      uiShow("No ID on tag", "Try another");
    }
    if ((int32_t)(now - jobEndMs) >= 0) finishNfcScan();
  }
}

void webTick() {
  if (started) server.handleClient();
  jobTick();
}

void webStartOnce() {
  if (started || !netConnected()) return;

  if (!MDNS.begin(OTA_HOST)) dbgPrintf("mDNS start FAILED\n");
  else dbgPrintf("mDNS started\n");

  server.on("/", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.sendHeader("Connection", "close");
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/debug.txt", HTTP_GET, []() {
    if (!requireAuth()) return;
    String t = dbgText();
    t += "\n\nSTATUS\n";
    t += "Mode: ";
    t += appModeName();
    t += "\nWiFi: ";
    t += netConnected() ? "up" : "down";
    t += "\nLimit: ";
    t += limitSwitchPressed ? "PRESSED" : "open";
    t += "\nRecovering: ";
    t += hardwareRecovering() ? "YES" : "NO";
    t += "\nWebJob: ";
    t += webBusy() ? "YES" : "NO";
    t += "\n\nINVENTORY\nInventory: ";
    t += String(inventoryCount());
    t += " / ";
    t += String(kMaxCurrency);
    t += "\nLowStock: ";
    t += inventoryLow() ? "YES" : "NO";
    t += "\n\nDROP DEBUG\nDroppedCount: ";
    t += String(dropDropped());
    t += "\nTargetDrops: ";
    t += String(dropTarget());
    t += "\nDropActive: ";
    t += dropActive() ? "YES" : "NO";
    t += "\n";
    server.send(200, "text/plain", t);
  });

  server.on("/debug", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send_P(200, "text/html", DEBUG_HTML);
  });

  server.on("/debug/clear", HTTP_POST, []() {
    if (!requireAuth()) return;
    dbgClear();
    redirectDebug();
  });

  server.on("/reboot", HTTP_POST, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", "rebooting");
    delay(150);
    ESP.restart();
  });

  server.on("/debug/up10", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: servo UP 10s\n");
    uiShow("Servo UP", "10 seconds");
    servoWriteUs(kServoUpUs);
    startJob(WebJob::ServoUp, 10000);
    redirectDebug();
  });

  server.on("/debug/drop1", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: drop 1 requested\n");
    dropStart(1);
    redirectDebug();
  });

  server.on("/debug/resetinventory", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: reset inventory requested\n");
    inventoryRefill();
    redirectDebug();
  });

  server.on("/debug/dropall", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: drop all requested (timed)\n");
    dropStart(999, true, true);
    redirectDebug();
  });

  server.on("/debug/irscan10", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: IR scan 10s start (thr drop=%d dep=%d)\n", irDropThreshold, irDepThreshold);
    uiShow("IR scan", "10s...");
    irDropMin = irDepMin = 4096;
    irDropMax = irDepMax = 0;
    irDropSum = irDepSum = 0;
    irSamples = 0;
    startJob(WebJob::IrScan, 10000);
    redirectDebug();
  });

  server.on("/debug/nfcid10", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (machineBusy()) {
      server.send(409, "text/plain", "busy");
      return;
    }
    dbgPrintf("WEB: NFC ID scan 10s start\n");
    uiShow("Tap NFC card", "10s timeout");
    nfcFound = false;
    nfcFoundId = 0;
    startJob(WebJob::NfcScan, 10000);
    redirectDebug();
  });

  server.on(
    "/update", HTTP_POST,
    []() {
      if (!requireAuth()) return;
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
      delay(150);
      ESP.restart();
    },
    []() {
      if (!server.authenticate(WEB_USER, WEB_PASS)) return;
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        otaAuthed = server.hasArg("otapass") && (server.arg("otapass") == OTA_PASSWORD);
        if (!otaAuthed) {
          dbgPrintf("OTA denied: bad password\n");
          return;
        }
        dbgPrintf("Update: %s\n", upload.filename.c_str());
        servoStop();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (!otaAuthed) {
        if (Update.isRunning()) Update.abort();
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) dbgPrintf("Update Success: %u\nRebooting...\n", (unsigned)upload.totalSize);
        else Update.printError(Serial);
      }
    });

  server.begin();
  started = true;
  dbgPrintf("HTTP: http://%s/\n", WiFi.localIP().toString().c_str());
  dbgPrintf("Debug: http://%s/debug\n", WiFi.localIP().toString().c_str());
}
