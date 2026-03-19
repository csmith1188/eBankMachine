#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <Adafruit_PN532.h>

/* ===================== CONFIG ===================== */
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

extern const char* TRANSFER_URL;
extern const char* API_KEY;

// NEW: user lookup base URL (append numeric id)
extern const char* USER_LOOKUP_BASE;

extern const int KIOSK_ID;
extern const int KIOSK_ACCOUNT_PIN;

extern const int DIGIPOGS_PER_POG_WITHDRAW;
extern const int DIGIPOGS_PER_POG_DEPOSIT;

extern const char* OTA_HOST;
extern const char* OTA_PASSWORD;

static constexpr int SDA_PIN = 21;
static constexpr int SCL_PIN = 22;

static constexpr uint8_t LCD_ADDR = 0x27;

static constexpr int PN532_IRQ = -1;
static constexpr int PN532_RESET = -1;
static constexpr uint16_t PN532_TIMEOUT_MS = 100;

static constexpr int IR_DROP_PIN = 34;
static constexpr int IR_DEP_PIN = 35;

static constexpr int SERVO_PIN = 14;

static constexpr int SWITCH_PIN = 23;
static constexpr bool ACTIVE_LOW = true;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
static constexpr int LED_PIN = LED_BUILTIN;

extern const unsigned long DEBOUNCE_MS;

extern const unsigned long IR_SAMPLE_MS;
extern const unsigned long DEP_SAMPLE_US;
extern const unsigned long DROP_COOLDOWN_MS;
extern const unsigned long DEP_COOLDOWN_MS;

extern int bPressCount;
extern unsigned long bWindowStart;

enum FbErr {
  FB_OK = 0,
  FB_NO_WIFI,     // WiFi not connected
  FB_BEGIN_FAIL,  // https.begin failed (DNS/TLS/URL)
  FB_POST_FAIL,   // https.POST returned <= 0 (timeout/connection)
  FB_HTTP_FAIL,   // server responded but not 2xx
  FB_JSON_FAIL,   // response not JSON / parse error
  FB_API_REJECT   // JSON ok but success == false (pin/balance/etc)
};

const char* fbErrMsg(FbErr e);

// new enhanced transfer
bool formbarTransferEx(int from, int to, int amount, const char* reason, int pin,
                       String& outResp, int& outHttp, FbErr& outErr);

extern const int CALIB_READS;

extern const unsigned long D_WINDOW_MS;
extern const unsigned long NFC_POLL_MS;

extern const unsigned long REFUND_RETRY_MS;

extern int neutral_us;
extern int SERVO_DOWN_US;
extern int SERVO_UP_US;

/* ===================== GLOBALS ===================== */
extern WebServer server;
extern LiquidCrystal_I2C lcd;
extern Servo myServo;
extern Adafruit_PN532 nfc;
extern Keypad keypad;

extern bool otaStarted;

/* Debug buffer */
void dbgClear();
void dbgAppend(const char* s);
void dbgPrintf(const char* fmt, ...);

/* Modes */
enum TradeMode {
  MODE_SELECT,
  MODE_DIGI_TO_REAL,
  MODE_REAL_TO_DIGI,
  MODE_UPDATE_CARD,
  MODE_STU_TO_STU,
  MODE_NFC_WRITE     // NEW
};
extern TradeMode tradeMode;

extern char numBuf[10];
extern uint8_t numLen;

/* Withdraw wizard */
enum WizardState {
  WZ_ENTER_FROM,
  WZ_CONFIRM_FROM,  // NEW
  WZ_ENTER_PIN,
  WZ_ENTER_POGS,
  WZ_CONFIRM
};
extern WizardState wzState;
extern long wzFrom, wzPin, wzPogs;

/* Deposit */
enum DepositState {
  DEP_ENTER_ID,
  DEP_CONFIRM_ID,  // NEW
  DEP_SCANNING
};

/* Student->Student transfer wizard */
enum StuWizardState {
  ST_ENTER_FROM,
  ST_CONFIRM_FROM,
  ST_ENTER_PIN,
  ST_ENTER_TO,
  ST_CONFIRM_TO,
  ST_ENTER_AMOUNT,
  ST_CONFIRM_SEND
};

extern StuWizardState stState;
extern long stFrom, stPin, stTo, stAmt;

void startStudentTransferFlow();
void handleStudentTransferKey(char k);

/* NFC write */
enum NfcWriteState {
  NFCW_ENTER_ID,
  NFCW_WAIT_HOLD,     // waiting for tag + stable hold
  NFCW_WRITING,       // performing write
  NFCW_VERIFY,        // readback verify
  NFCW_WAIT_REMOVE    // wait until tag removed
};

extern NfcWriteState nfcwState;
extern long nfcwId;

extern unsigned long nfcwHoldStartMs;
extern bool nfcwHoldActive;

void startNfcWriteFlow();
void handleNfcWriteKey(char k);
void nfcWriteTick();

/* writes "ID:xxxxx" as NDEF Text record */
bool ntagWriteIdText(long id);

bool ntagWriteFullCard(const String& website, const String& name, long formbarId, long formbarPin);

/* reads first NTAG user pages and looks for ASCII "ID:####"; returns true if found */
bool ntagTryReadIdText(long& outId);

extern DepositState depState;
extern long depToId;
extern int depositCount;

/* Inventory */
extern int currencyCount;
extern const int MAX_CURRENCY_CAPACITY;
extern const int LOW_STOCK_THRESHOLD;

// Debug: drop timing
extern bool dbgDropTimingEnabled;
extern bool dbgDropAllAutoStop;
extern unsigned long dbgDropLastEventMs;
extern unsigned long dbgDropSumIntervalsMs;
extern unsigned long dbgDropIntervals;

/* temp display buffer for user name during ID confirmation */
extern char idNameBuf[17];

/* Limit switch debounce */
extern int lastReading;
extern int stableState;
extern unsigned long lastChange;
extern bool limitSwitchPressed;
extern bool prevLimitSwitchPressed;

/* IR thresholds/state */
extern int IR_DROP_THRESHOLD;
extern int IR_DEP_THRESHOLD;

extern unsigned long irLastSample;
extern unsigned long nextCountAllowedAt;
extern bool irWasAbove;
extern unsigned long dropStartMs;

void showNfcWriteEnterId();
void showNfcWriteTap();

extern bool depWasAbove;
extern unsigned long depNextAllowedAt;
extern unsigned long depStartMs;
extern unsigned long depLastSampleUs;

/* Drop counters */
extern volatile int targetDrops;
extern volatile int droppedCount;

enum MotionState { MS_IDLE,
                   MS_DROPPING };
extern MotionState motionState;

/* Refund */
extern bool refundPending;
extern long refundToId;
extern int refundDigipogs;
extern unsigned long nextRefundTryAt;

/* Debug keypad shortcuts */
extern int dPressCount;
extern unsigned long dWindowStart;
extern int cPressCount;
extern unsigned long cWindowStart;

/* ===================== PROTOTYPES ===================== */
/* ui */
void otaDelay(unsigned long ms);
void showMsg(const char* line0, const char* line1 = nullptr, unsigned long ms = 0);
void showModeMenu();
void clearEntryLine();
void showEntry(const __FlashStringHelper* prompt);
void showConfirmWithdraw(long pogs);
void showDepositEnterId();
void showDepositScanning();

// NEW: confirm screen used for ID confirmation
void showConfirmId(const char* line1, long id, const char* name);

/* ota web */
void setupWebOtaOnce();
void otaTick();

/* net */
void wifiEnsureConnected();

/* formbar */
bool formbarTransfer(int from, int to, int amount, const char* reason, int pin, String& outResp, int& outHttp);
bool trySendRefundNow();

// NEW: ID existence check (GET /api/user/{id})
bool formbarUserExists(int id, String& outName, int& outHttp);

extern unsigned long depBeamStartMs;
extern bool depBeamTiming;
extern unsigned long depLastBeamMs;
extern unsigned long depMaxBeamMs;
extern const unsigned long DEP_BEAM_MIN_MS;
extern const unsigned long DEP_BEAM_MAX_MS;

extern const unsigned long DEP_BEAM_MAX_BLOCK_MS;

/* hardware */
void hardwareInit();
void servoAttach();
void servoStopDetach();
void IR_Calibration();
void limitSwitchTick();
void handleLimitPressed();

/* drop */
void startDrop(int count);
void finishDrop(const char* why);
void dropTick();

/* Inventory */
extern int currencyCount;
extern const int MAX_CURRENCY_CAPACITY;
extern const int LOW_STOCK_THRESHOLD;

/* deposit */
void startDepositFlow();
void depositTick();
void handleDepositKey(char k);

/* withdraw */
void startWithdrawWizard();
void handleWithdrawKey(char k);

/* card (stub) */
void startCardUpdateFlow();
void cardTick();
void handleCardKey(char k);

/* keypad router */
void keypadTick();

/* refund */
void refundTick();