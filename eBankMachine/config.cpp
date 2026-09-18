#include "eBankMachine.h"

const char* WIFI_SSID = "robonet";
const char* WIFI_PASS = "formDog220!";

const char* TRANSFER_URL = "https://formbar.yorktechapps.com/api/digipogs/transfer";
const char* API_KEY = "7464fcaa5a799a7dd498ae63ee3fcfcc0a3afb07ce865d659d34e0eca6954c9e";

// NEW: user lookup base (append numeric id)
const char* USER_LOOKUP_BASE = "https://formbar.yorktechapps.com/api/user/";

const int KIOSK_ID = 1;
const int KIOSK_ACCOUNT_PIN = 1834;

const int DIGIPOGS_PER_POG_WITHDRAW = 110;
const int DIGIPOGS_PER_POG_DEPOSIT = 90;

const char* OTA_HOST = "digipog-kiosk";
const char* OTA_PASSWORD = "E_banks";

const unsigned long DEBOUNCE_MS = 20;

const unsigned long IR_SAMPLE_MS = 10;
const unsigned long DEP_SAMPLE_US = 2000;
const unsigned long DROP_COOLDOWN_MS = 500;
const unsigned long DEP_COOLDOWN_MS = 150;

const int CALIB_READS = 80;

const unsigned long DEP_BEAM_MAX_BLOCK_MS = 500;
const unsigned long DEP_BEAM_MIN_MS = 2;
const unsigned long DEP_BEAM_MAX_MS = 30;

const unsigned long D_WINDOW_MS = 5000;
const unsigned long NFC_POLL_MS = 60;

const unsigned long REFUND_RETRY_MS = 8000;

int neutral_us = 1403;
int SERVO_DOWN_US = 1403 + 250;
int SERVO_UP_US = 1500;