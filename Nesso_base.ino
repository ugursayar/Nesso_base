/*
  Arduino Nesso N1 - Nesso_base

  ...

  created: February 3 2026
  by: Ugur Sayar
*/

#include <Arduino_Nesso_N1.h>
#include <WiFi.h>
#include <NetworkUdp.h>
#include <time.h>
#include <SPI.h>
#include <RadioLib.h>
#include <mbedtls/aes.h>
#include "Adafruit_seesaw.h"
#include "arduino_secrets.h"
#include "vader_art.h"
#include "obiwan_art.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <Preferences.h>
#include "esp_sleep.h"
#define DISABLE_CODE_FOR_RECEIVER
#include <IRremote.hpp>
#include <LittleFS.h>
#include <WebServer.h>

// IR type definitions placed before #include <ArduinoJson.h> so the
// Arduino preprocessor sees them before inserting auto-generated prototypes.
#define IR_MAX_FILES   32
#define IR_MAX_BUTTONS 48
#define IR_MAX_RAW_LEN 128

enum IRFileProto : uint8_t {
  IRP_NEC, IRP_SAMSUNG, IRP_SIRC12, IRP_SIRC15, IRP_SIRC20,
  IRP_RC5, IRP_RC6, IRP_LG, IRP_JVC, IRP_RAW, IRP_UNKNOWN
};

struct IRButton {
  char        label[20];
  IRFileProto proto;
  uint8_t     _pad;
  uint16_t    rawFreq;
  uint16_t    rawLen;
  uint32_t    address;
  uint32_t    command;
  uint16_t    rawData[IR_MAX_RAW_LEN];
};

struct IRFileEntry {
  char name[40];
  char path[64];
};

#define IR_DIR_MAX 48
struct IRDirEntry {
  char name[40];
  char path[64];
  bool isDir;
};

#include <ArduinoJson.h>

// ----------------------------------------------------------------
// Display
// ----------------------------------------------------------------

NessoDisplay display;
LGFX_Sprite  statusSprite(&display);
NessoTouch   touch;

// Y-offset where statusSprite is pushed; area above holds the WiFi icon
const int SPRITE_Y = 22;

const uint16_t COLOR_TEAL   = 0x0410;
const uint16_t COLOR_BLACK  = 0x0000;
const uint16_t COLOR_GREEN  = 0x1e85;
const uint16_t COLOR_ORANGE = 0xed03;
const uint16_t COLOR_RED    = 0xe841;
const uint16_t COLOR_WHITE  = 0xffff;
const uint16_t COLOR_GRAY   = 0x4208;
#define BG_COLOR   TFT_BLACK
#define TIME_COLOR TFT_CYAN
#define DATE_COLOR TFT_YELLOW
#define WIFI_COLOR TFT_GREEN

// Splash boot log — 3 scrolling lines, used only during setup()
static char     splashBuf[3][52];
static uint16_t splashBufColor[3];
static int      splashBufCount = 0;

// ----------------------------------------------------------------
// Matrix rain
// ----------------------------------------------------------------

#define NUM_COLUMNS 15
#define CHAR_HEIGHT 16

int dropPositions[NUM_COLUMNS];
int dropSpeeds[NUM_COLUMNS];
int dropLengths[NUM_COLUMNS];

// ── Shared non-blocking buzzer sequencer ────────────────────────
struct BuzzNote { uint16_t freq; uint16_t ms; };


static const BuzzNote VADER_MELODY[] = {
  // Imperial March — A minor, ~BPM 104
  // q=500ms  d.e=375ms  .e=125ms  h=1000ms
  {440,500},{0,80}, {440,500},{0,80}, {440,500},{0,80},
  {349,375},{0,50}, {523,125},{0,50},
  {440,500},{0,80}, {349,375},{0,50}, {523,125},{0,50},
  {440,1000},{0,200},
  {659,500},{0,80}, {659,500},{0,80}, {659,500},{0,80},
  {698,375},{0,50}, {523,125},{0,50},
  {440,500},{0,80}, {349,375},{0,50}, {523,125},{0,50},
  {440,1000},{0,300},
};
static const int VADER_MELODY_LEN = (int)(sizeof(VADER_MELODY)/sizeof(VADER_MELODY[0]));

static const BuzzNote OBIWAN_MELODY[] = {
  // Star Wars Main Title — ~BPM 108
  // q=500ms  h=1000ms
  {392,500},{0,80}, {392,500},{0,80}, {392,500},{0,80},
  {523,1000},{0,100}, {784,1000},{0,100},
  {698,500},{0,80}, {659,500},{0,80}, {622,500},{0,80}, {587,500},{0,80},
  {1047,1000},{0,100}, {784,500},{0,80},
  {698,500},{0,80}, {659,500},{0,80}, {622,500},{0,80}, {587,500},{0,80},
  {1047,1000},{0,100}, {784,500},{0,80},
  {698,500},{0,80}, {659,500},{0,80}, {698,500},{0,80},
  {523,1000},{0,400},
};
static const int OBIWAN_MELODY_LEN = (int)(sizeof(OBIWAN_MELODY)/sizeof(OBIWAN_MELODY[0]));

// Shared buzzer state — one melody plays at a time
const BuzzNote* buzzerMelody    = nullptr;
int             buzzerMelodyLen = 0;
bool            buzzerPlaying   = false;
int             buzzerNoteIdx   = 0;
unsigned long   buzzerNoteEndMs = 0;

// ----------------------------------------------------------------
// WiFi & UDP
// ----------------------------------------------------------------

// Compiled-in fallback credentials (arduino_secrets.h)
const char* const secretSsid = SECRET_SSID;
const char* const secretPass = SECRET_PASS;

// User-persisted credentials (NVS) — override the compiled-in ones when set.
// wifiUserSsid is empty until the user provides credentials via serial.
char wifiUserSsid[33] = "";
char wifiUserPass[65] = "";

// Aliases used by the rest of the code
const char* ssid     = secretSsid;
const char* password = secretPass;

bool wifiAuthFailed = false;  // set when wrong-password disconnect is detected

char ntpServer[64]        = "pool.ntp.org";
long gmtOffset_sec        = 3600 * 3;
int  daylightOffset_sec   = 0;
int  udpPort              = 8889;
char targetIpAddress[16]  = "192.168.1.27";

NetworkUDP udp;
struct tm  timeinfo;
int        lastMinute = -1;  // clock updates once per minute

// ----------------------------------------------------------------
// Battery
// ----------------------------------------------------------------

NessoBattery battery;

float batteryVoltage = 0.0;
float chargeLevel    = 0.0;  // raw library value — debug only
float voltagePercent = 0.0;  // percentage derived from voltage curve
char  uptimeString[12];         // "HH:MM:SS"
bool  ledStatus       = false;
bool  batteryCharging = false;
bool  onExternalPower = false;

unsigned long lastLEDflipTime  = 0;
unsigned long batteryCheckTime = 0;
unsigned long buttonCheckTime  = 0;
unsigned long touchCheckTime   = 0;

// ----------------------------------------------------------------
// Power management / idle dim-sleep
// ----------------------------------------------------------------

static const uint32_t DIM_TIMEOUTS_MS[]   = {30000, 60000, 120000, UINT32_MAX};  // 30s/60s/2min/OFF
static const char* const DIM_TIMEOUT_LABELS[]  = {"30s", "60s", "2min", "OFF"};
static const uint32_t SLEEP_TIMEOUTS_MS[] = {120000, 300000, 600000, UINT32_MAX}; // 2min/5min/10min/OFF
static const char* const SLEEP_TIMEOUT_LABELS[] = {"2min", "5min", "10min", "OFF"};
static const int POWER_TIMEOUT_COUNT = 4;
static const uint8_t LOW_BAT_THRESHOLDS[] = {5, 10, 0}; // 0 = OFF
static const char* const LOW_BAT_LABELS[] = {"5%", "10%", "OFF"};
static const int LOW_BAT_COUNT = 3;

int  dimTimeoutIdx   = 0;    // index → DIM_TIMEOUTS_MS  (default 30s)
int  sleepTimeoutIdx = 0;    // index → SLEEP_TIMEOUTS_MS (default 2min)
int  lowBatIdx       = 0;    // index → LOW_BAT_THRESHOLDS (default 5%)
bool uiClickEnabled  = true; // buzzer click on key/tap events

const uint8_t BRIGHTNESS_FULL = 15;
const uint8_t BRIGHTNESS_DIM  =  3;

unsigned long lastActivityMs = 0;
bool          displayDimmed  = false;
bool          displayOff     = false;

// ── Touch tracking ───────────────────────────────────────────────
bool          touchReady   = false;
bool          touchActive  = false;
int16_t       touchDownX   = 0, touchDownY   = 0;
int16_t       touchCurrX   = 0, touchCurrY   = 0;
unsigned long touchDownMs  = 0;

int  progressPos       = 0;
bool progressExpanding = true;
NessoBattery::ChargeStatus chargeStatus;

// ----------------------------------------------------------------
// IMU / Orientation
// ----------------------------------------------------------------

// IMU.begin() must be called explicitly after Wire.begin() in setup().
// Rotation mapping (BMI270 axes, values in G):
//   ay < 0  → landscape normal  (rotation 1)
//   ay > 0  → landscape flipped (rotation 3)
//   ax > 0  → portrait normal   (rotation 0)
//   ax < 0  → portrait flipped  (rotation 2)
uint8_t       currentRotation   = 0;  // default portrait
unsigned long lastOrientationMs = 0;
bool          imuDebugEnabled   = false;  // stream IMU readings to serial

// ----------------------------------------------------------------
// Gamepad (Adafruit seesaw mini)
// ----------------------------------------------------------------

Adafruit_seesaw ss = Adafruit_seesaw(&Wire);

#define BUTTON_X      6
#define BUTTON_Y      2
#define BUTTON_A      5
#define BUTTON_B      1
#define BUTTON_SELECT 0
#define BUTTON_START  16
#define JOY1_X        14
#define JOY1_Y        15

uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

int      zero_x = 0, zero_y = 0;
bool     joystickAvailable   = false;  // set at boot by probing seesaw I2C address
bool     controllerConnected = false;
bool     calibrationComplete = false;
int16_t  joyDisplayX = 0;       // -255..255, stored for the joystick visualization
int16_t  joyDisplayY = 0;
uint32_t gamepadButtons = 0xFFFFFFFF;  // all bits 1 = all released (active LOW)

struct ControlCommand {
  int leftMotor;   // -255 to 255
  int rightMotor;  // -255 to 255
};
ControlCommand transmitCmd;

// ----------------------------------------------------------------
// LoRa (SX1262)
// ----------------------------------------------------------------

// SPI shared with display (CS=23, IRQ=15, no reset, BUSY=19)
SX1262 lora = new Module(LORA_CS, LORA_IRQ, RADIOLIB_NC, LORA_BUSY);

#define LORA_FREQ        869.525f // MHz — Meshtastic EU_868 default channel
#define LORA_BW          250.0f   // kHz — Meshtastic LONG_FAST preset
#define LORA_SF          11       // spreading factor — Meshtastic LONG_FAST
#define LORA_CR          5        // coding rate 4/5
#define LORA_SYNC_WORD   0x2B     // Meshtastic private sync word
#define LORA_LOG_SIZE    5

struct LoRaEntry {
  int16_t  rssi;
  float    snr;
  uint8_t  size;
  uint32_t ms;          // millis() at reception
  uint32_t srcNode;     // Meshtastic source node ID (bytes 4-7 of header)
  uint32_t pktId;       // Meshtastic packet ID (for dedup)
  char     text[52];    // decoded message text, or portnum label for non-text packets
};

// Modem presets (name, bandwidth kHz, spreading factor)
struct LoRaPreset { const char* name; float bw; uint8_t sf; };
static const LoRaPreset LORA_PRESETS[] = {
  {"LONG_FAST",  250.0f, 11},
  {"LONG_SLOW",  125.0f, 12},
  {"MED_FAST",   250.0f,  9},
  {"SHORT_FAST", 250.0f,  7},
};
static const int LORA_PRESET_COUNT = 4;

// Common Meshtastic frequencies (EU868 + US915)
static const float LORA_FREQ_LIST[] = {869.525f, 868.1f, 868.3f, 868.5f, 869.1f, 915.0f};
static const int   LORA_FREQ_COUNT  = 6;

int  loraPresetIdx  = 0;     // index → LORA_PRESETS
int  loraFreqIdx    = 0;     // index → LORA_FREQ_LIST
bool loraAutoReply  = false;
bool loraDedup      = true;  // suppress retransmit duplicates (same packet ID)

// Meshtastic packet format constants
// Channel hash = XOR(PSK bytes) ^ XOR(name chars); for "LongFast" default = 0x08
// AES-128-CTR key = base64 "1PG7OiApB1nwvP+rz05pAQ==" (default channel PSK expansion)
// Nonce: [packetId: 4B LE][0x00 × 4][fromNode: 4B LE][0x00 × 4]
static const uint8_t MESH_DEFAULT_KEY[16] = {
  0xD4, 0xF1, 0xBB, 0x3A, 0x20, 0x29, 0x07, 0x59,
  0xF0, 0xBC, 0xFF, 0xAB, 0xCF, 0x4E, 0x69, 0x01
};
static const uint8_t MESH_CHANNEL_HASH = 0x08;  // "LongFast" default
static const uint32_t MESH_MY_NODE_ID  = 0x4E455353;  // "NESS" in ASCII

struct __attribute__((packed)) MeshHeader {
  uint32_t to;
  uint32_t from;
  uint32_t id;
  uint8_t  flags;       // bits 2:0 = hop_limit, bit 3 = want_ack, bits 7:5 = hop_start
  uint8_t  channel;     // channel hash for quick filtering
  uint8_t  next_hop;
  uint8_t  relay_node;
};

bool     loraInitialized    = false;
bool     loraInitFailed     = false;
bool     loraListening      = false;  // true only after user explicitly starts listen
volatile bool loraPacketFlag = false;
LoRaEntry loraLog[LORA_LOG_SIZE];
int      loraLogCount       = 0;
uint32_t loraTotalPackets   = 0;
int      loraScrollOffset   = 0;   // 0 = newest first; swipe to scroll older

// Auto-reply ACK tracking
uint32_t      loraPendingAckId  = 0;      // sent packet ID we're waiting ACK for (0 = idle)
unsigned long loraAckDeadlineMs = 0;      // give up waiting after this timestamp
bool          loraLastAckOk     = false;  // did last auto-reply get acknowledged?

unsigned long previousMillis        = 0;
unsigned long previousMillisButtons = 0;

// ----------------------------------------------------------------
// WiFi scan screen
// ----------------------------------------------------------------

#define WIFI_SCAN_SIZE 20

struct WiFiScanEntry {
  char    ssid[33];
  int32_t rssi;
  uint8_t encType;   // wifi_auth_mode_t: 0 = OPEN
  uint8_t channel;
  char    bssid[18];
};

WiFiScanEntry wifiScanLog[WIFI_SCAN_SIZE];
int           wifiScanCount  = 0;
int           wifiScanOffset = 0;
bool          wifiScanning   = false;
bool          wifiDebugMode  = false;
bool          wifiAutoScan   = false;

// ----------------------------------------------------------------
// Bluetooth LE
// ----------------------------------------------------------------

#define BT_LOG_SIZE  10

struct BTEntry {
  char     name[24];
  char     addr[18];   // "AA:BB:CC:DD:EE:FF\0"
  int8_t   rssi;
  bool     connectable;
  uint32_t lastSeenMs;
  uint8_t  rawData[16];  // first 16 bytes manufacturer data (debug mode)
  uint8_t  rawLen;
};

static const int8_t      BT_RSSI_FILTERS[]  = {-70, -80, -90, -128}; // -128 = OFF
static const char* const BT_RSSI_LABELS[]   = {"-70dBm", "-80dBm", "-90dBm", "OFF"};
static const int         BT_RSSI_FILTER_COUNT = 4;

bool btScanRequested  = false; // true only after user explicitly taps SCAN
int  btScanModeIdx    = 0;     // 0=ACTIVE, 1=PASSIVE
int  btRssiFilterIdx  = 3;     // 3=OFF
bool btDebugMode      = false;
bool btAdvEnabled     = true;  // advertise as NESSO (default ON)
bool btStartupEnabled = true;  // auto-init BLE stack at boot (default ON)

bool          btInitialized  = false;
bool          btInitFailed   = false;
bool          btScanning     = false;
unsigned long btScanStartMs  = 0;
BTEntry btLog[BT_LOG_SIZE];
int     btLogCount     = 0;
int     btScrollOffset = 0;
int     btTotalSeen    = 0;
char    btSelectedAddr[18] = "";  // MAC of device in detail view; "" = list view
bool    btConnected        = false;
bool     btConnectPending    = false;  // set by RTOS callback, consumed by main loop
bool     btDisconnectPending = false;
bool     btWelcomePending    = false;  // delayed welcome — sent after client subscribes
uint32_t btWelcomePendingMs  = 0;
char    btConnectedAddr[18] = "";
char    btConnectedName[24] = "";

#define BT_SCAN_DURATION_S  8   // seconds per scan window before restart

BLEScan* pBLEScan = nullptr;

class BTServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    btConnected      = true;
    btConnectPending = true;
  }
  void onDisconnect(BLEServer*) override {
    btConnected         = false;
    btConnectedAddr[0]  = '\0';
    btConnectedName[0]  = '\0';
    btDisconnectPending = true;
  }
};
static BTServerCB btServerCB;

// Sort btLog[0..btLogCount-1] by RSSI descending (strongest signal first).
void btSortByRSSI() {
  for (int i = 1; i < btLogCount; i++) {
    BTEntry key = btLog[i];
    int j = i - 1;
    while (j >= 0 && btLog[j].rssi < key.rssi) {
      btLog[j + 1] = btLog[j];
      j--;
    }
    btLog[j + 1] = key;
  }
}

// BLE scan callback — runs from the BLE FreeRTOS task (not main loop)
class BTScanCB : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice dev) override {
    int8_t rssi      = (int8_t)dev.getRSSI();
    int8_t threshold = BT_RSSI_FILTERS[btRssiFilterIdx];
    if (threshold != -128 && rssi < threshold) return;

    String addrString = dev.getAddress().toString();
    const char* addrStr = addrString.c_str();

    // Update existing entry if MAC already known
    for (int i = 0; i < btLogCount; i++) {
      if (strcmp(btLog[i].addr, addrStr) == 0) {
        btLog[i].rssi       = rssi;
        btLog[i].lastSeenMs = millis();
        if (dev.haveName()) {
          strncpy(btLog[i].name, dev.getName().c_str(), 23);
          btLog[i].name[23] = '\0';
        }
        if (btDebugMode && dev.haveManufacturerData()) {
          String mfr = dev.getManufacturerData();
          btLog[i].rawLen = (uint8_t)min((int)mfr.length(), 16);
          memcpy(btLog[i].rawData, mfr.c_str(), btLog[i].rawLen);
        }
        btSortByRSSI();
        return;
      }
    }

    btTotalSeen++;

    // New device — find slot
    int slot;
    if (btLogCount < BT_LOG_SIZE) {
      slot = btLogCount++;
    } else {
      // Evict oldest-seen entry
      uint32_t oldest = 0xFFFFFFFF; slot = 0;
      for (int i = 0; i < BT_LOG_SIZE; i++) {
        if (btLog[i].lastSeenMs < oldest) { oldest = btLog[i].lastSeenMs; slot = i; }
      }
    }
    strncpy(btLog[slot].addr, addrStr, 17);
    btLog[slot].addr[17]    = '\0';
    btLog[slot].rssi        = rssi;
    btLog[slot].lastSeenMs  = millis();
    btLog[slot].connectable = dev.isConnectable();
    btLog[slot].name[0]     = '\0';
    if (dev.haveName()) {
      strncpy(btLog[slot].name, dev.getName().c_str(), 23);
      btLog[slot].name[23] = '\0';
    }
    btLog[slot].rawLen = 0;
    if (btDebugMode && dev.haveManufacturerData()) {
      String mfr = dev.getManufacturerData();
      btLog[slot].rawLen = (uint8_t)min((int)mfr.length(), 16);
      memcpy(btLog[slot].rawData, mfr.c_str(), btLog[slot].rawLen);
    }
    btSortByRSSI();
  }
};

static BTScanCB btScanCB;

// Media sub-screen: 0=matrix, 1=vader, 2=obiwan
int  mediaSubScreen     = 0;
int  lastMediaSubScreen = -1;

// ----------------------------------------------------------------
// IR Remote
// IR LED must be wired to IR_SEND_PIN (adjust for your hardware).
// On Nesso N1 the Grove connector exposes free GPIOs — pin 2 is a
// safe default; change if your IR LED is on a different pin.
// ----------------------------------------------------------------

#define IR_SEND_PIN IR_TX_PIN   // GPIO 9 — built-in IR blaster on Nesso N1

// ── Flat scan — used by serial commands (ir list / ir send) ──────
static IRFileEntry irFiles[IR_MAX_FILES];
static int         irFileCount   = 0;
static int         irSelectedIdx = -1;  // index in irFiles[] of loaded device

// ── Directory browser ─────────────────────────────────────────────
static IRDirEntry  irDir[IR_DIR_MAX];
static int         irDirCount    = 0;
static char        irBrowsePath[64];

// ── Currently loaded device ───────────────────────────────────────
static IRButton    irBtns[IR_MAX_BUTTONS];
static int         irBtnCount    = 0;
static char        irLoadedPath[64];  // full path of loaded .ir file
static char        irLoadedName[40];  // display name (no path, no .ir)
static char        irSavedPath[64];   // NVS-persisted path

// IrSender is a global provided by IRremote.hpp; initialised in initIR().

// ── IR UI state ───────────────────────────────────────────────────
enum IRLevel : uint8_t { IR_LEVEL_LIST = 0, IR_LEVEL_REMOTE = 1 };
IRLevel  irLevel      = IR_LEVEL_LIST;
int      irListOff    = 0;   // scroll offset — directory list
int      irBtnPageOff = 0;   // scroll offset — remote button grid
int      irFlashX     = -1;
int      irFlashY     = -1;
uint32_t irFlashMs    = 0;
uint32_t irTxMs       = 0;

// ----------------------------------------------------------------
// Navigation
// ----------------------------------------------------------------

enum MainFunctions {
  FUNCTION_MAIN       = 0,
  FUNCTION_CONTROLLER = 1,
  FUNCTION_BT         = 2,
  FUNCTION_WIFI       = 3,
  FUNCTION_LORA       = 4,
  FUNCTION_IR         = 5,   // IR remote — after LoRa
  FUNCTION_MEDIA      = 6,   // matrix + vader + obiwan merged; sub-screen via swipe up/down
  FUNCTION_BATTERY    = 7,   // moved to end; long-press opens device settings
} currentFunction;

const int  mainFunctionCount = 8;
int        lastFunction;

// ── Web file manager ─────────────────────────────────────────────
static bool   webFMRunning      = false;
static bool   webFMPendingStart = false;

// ── Button press detection ───────────────────────────────────────
const long LONG_PRESS_MS   = 700;   // ms held → long press
const long BTN_DEBOUNCE_MS = 50;    // ignore bounces shorter than this

bool          key1Down      = false, key2Down      = false;
bool          key1LongFired = false, key2LongFired = false;
unsigned long key1PressedAt = 0,     key2PressedAt = 0;

// ── Settings overlay ─────────────────────────────────────────────
enum NavState { NAV_NORMAL, NAV_SETTINGS, NAV_RESET } navState = NAV_NORMAL;
int settingsCursor       = 0;   // selected settings item index (absolute, 0-based)
int settingsScrollOffset = 0;   // first visible settings row index
uint32_t resetFeedbackMs  = 0;  // millis() when last reset button was tapped
int      resetFeedbackBtn = -1; // which reset button was tapped (-1 = none)

// ----------------------------------------------------------------
// Debug
// ----------------------------------------------------------------

#define DEBUG 0
#if DEBUG == 1
  #define debug(x)   Serial.print(x)
  #define debugln(x) Serial.println(x)
#else
  #define debug(x)
  #define debugln(x)
#endif


// ================================================================
// Splash screen
// ================================================================

static void drawSplashCorners(int arm) {
  const int x0 = 8,  y0 = 8;
  const int x1 = display.width()  - 8;
  const int y1 = display.height() - 8;
  const int t  = 2;

  display.fillRect(x0,       y0,       arm, t,   COLOR_TEAL);
  display.fillRect(x0,       y0,       t,   arm, COLOR_TEAL);
  display.fillRect(x1 - arm, y0,       arm, t,   COLOR_TEAL);
  display.fillRect(x1 - t,   y0,       t,   arm, COLOR_TEAL);
  display.fillRect(x0,       y1 - t,   arm, t,   COLOR_TEAL);
  display.fillRect(x0,       y1 - arm, t,   arm, COLOR_TEAL);
  display.fillRect(x1 - arm, y1 - t,   arm, t,   COLOR_TEAL);
  display.fillRect(x1 - t,   y1 - arm, t,   arm, COLOR_TEAL);
}

static void redrawSplashLog() {
  const int logY = display.height() - 44;
  display.fillRect(14, logY, display.width() - 28, 40, COLOR_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setFont(&fonts::Font0);
  display.setTextSize(1);
  for (int i = 0; i < splashBufCount; i++) {
    bool latest = (i == splashBufCount - 1);
    display.setTextColor(latest ? splashBufColor[i] : display.color565(60, 60, 60));
    display.drawString(splashBuf[i], 16, logY + 2 + i * 13);
  }
}

void splashLog(const char* msg, uint16_t color) {
  if (splashBufCount < 3) {
    strncpy(splashBuf[splashBufCount], msg, 51);
    splashBufColor[splashBufCount] = color;
    splashBufCount++;
  } else {
    memcpy(splashBuf[0], splashBuf[1], 52);
    memcpy(splashBuf[1], splashBuf[2], 52);
    splashBufColor[0] = splashBufColor[1];
    splashBufColor[1] = splashBufColor[2];
    strncpy(splashBuf[2], msg, 51);
    splashBufColor[2] = color;
  }
  redrawSplashLog();
}

void runSplashAnimation() {
  display.setRotation(0);   // portrait
  currentRotation = 0;
  display.fillScreen(COLOR_BLACK);

  // Phase 1: CRT scanline sweep
  for (int y = 0; y < display.height(); y++) {
    display.drawFastHLine(0, y, display.width(), display.color565(0, 16, 0));
    if (y % 3 == 0) delay(2);
  }
  display.fillScreen(COLOR_BLACK);
  delay(40);

  // Phase 2: Corner brackets grow in
  const int ARM = 26;
  for (int arm = 1; arm <= ARM; arm++) {
    drawSplashCorners(arm);
    delay(10);
  }
  delay(60);

  // Phase 3: "NESSO" logo
  const int cx    = display.width()  / 2;
  const int logoY = (display.height() - 44) / 2 - 4;

  display.setTextDatum(MC_DATUM);
  display.setFont(&fonts::Font0);
  display.setTextSize(3);
  display.setTextColor(COLOR_TEAL);
  display.drawString("NESSO", cx, logoY);
  delay(70);
  display.setTextColor(COLOR_WHITE);
  display.drawString("NESSO", cx, logoY);

  // Divider line draws outward
  const int divY = logoY + 20;
  for (int dx = 0; dx <= 38; dx += 2) {
    display.drawFastHLine(cx - dx, divY, dx * 2, COLOR_TEAL);
    delay(6);
  }

  // Subtitle types out
  const char* subtitle = "BASE  STATION";
  display.setTextColor(COLOR_TEAL);
  display.setTextSize(1);
  int subX = cx - (strlen(subtitle) * 6) / 2;
  char charBuf[2] = {0, 0};
  for (int i = 0; subtitle[i]; i++) {
    charBuf[0] = subtitle[i];
    display.setTextDatum(TL_DATUM);
    display.drawString(charBuf, subX + i * 6, divY + 8);
    delay(40);
  }

  // Separator above log
  display.drawFastHLine(14, display.height() - 48,
                        display.width() - 28, display.color565(40, 40, 40));
  delay(80);
}


// ================================================================
// Persistent settings  (NVS via Preferences)
// ================================================================

void resetActivity();
void initMedia();
void renderMedia();
void renderBatterySettings();
void handleBatterySettingsTap(int16_t sx, int16_t sy);
void applyDeviceSettings();
void btInitStack();   // defined later with BT helpers

void loadSettings() {
  Preferences p;
  p.begin("nesso", true);   // read-only
  btScanModeIdx    = p.getUChar("btScanMode", 0);
  btRssiFilterIdx  = p.getUChar("btRssiFlt",  3);
  btDebugMode      = p.getBool ("btDebug",    false);
  btAdvEnabled     = p.getBool ("btAdv",      true);
  btStartupEnabled = p.getBool ("btStartup",  true);
  p.getString("irPath", irSavedPath, sizeof(irSavedPath));
  p.getString("wifiSsid", wifiUserSsid, sizeof(wifiUserSsid));
  p.getString("wifiPass", wifiUserPass, sizeof(wifiUserPass));
  wifiDebugMode    = p.getBool ("wifiDbg",    false);
  wifiAutoScan     = p.getBool ("wifiAuto",   false);
  loraPresetIdx    = p.getUChar("lPreset",    0);
  loraFreqIdx      = p.getUChar("lFreq",      0);
  loraAutoReply    = p.getBool ("lReply",     false);
  loraDedup        = p.getBool ("lDedup",     true);
  dimTimeoutIdx   = p.getUChar("dimTO",    0);
  sleepTimeoutIdx = p.getUChar("sleepTO",  0);
  lowBatIdx       = p.getUChar("lowBat",   0);
  uiClickEnabled  = p.getBool ("uiClick",  true);
  p.end();
}

void saveSettings() {
  Preferences p;
  p.begin("nesso", false);  // read-write
  p.putUChar("btScanMode", (uint8_t)btScanModeIdx);
  p.putUChar("btRssiFlt",  (uint8_t)btRssiFilterIdx);
  p.putBool ("btDebug",    btDebugMode);
  p.putBool ("btAdv",      btAdvEnabled);
  p.putBool ("btStartup",  btStartupEnabled);
  p.putString("irPath",  irSavedPath);
  p.putString("wifiSsid", wifiUserSsid);
  p.putString("wifiPass", wifiUserPass);
  p.putBool ("wifiDbg",    wifiDebugMode);
  p.putBool ("wifiAuto",   wifiAutoScan);
  p.putUChar("lPreset",    (uint8_t)loraPresetIdx);
  p.putUChar("lFreq",      (uint8_t)loraFreqIdx);
  p.putBool ("lReply",     loraAutoReply);
  p.putBool ("lDedup",     loraDedup);
  p.putUChar("dimTO",   (uint8_t)dimTimeoutIdx);
  p.putUChar("sleepTO", (uint8_t)sleepTimeoutIdx);
  p.putUChar("lowBat",  (uint8_t)lowBatIdx);
  p.putBool ("uiClick", uiClickEnabled);
  p.end();
}


// ================================================================
// LittleFS config  (/config.json)
// ================================================================

// Priority: NVS (runtime user-set) > config.json > compiled-in fallback.
// Called after LittleFS.begin() and after loadSettings() so NVS values are
// already in wifiUserSsid/wifiUserPass before we decide whether to apply FS creds.
void loadConfig() {
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  if (wifiUserSsid[0] == '\0') {
    strlcpy(wifiUserSsid, doc["wifi_ssid"] | "", sizeof(wifiUserSsid));
    strlcpy(wifiUserPass, doc["wifi_pass"] | "", sizeof(wifiUserPass));
  }
  strlcpy(ntpServer,       doc["ntp_server"] | "pool.ntp.org", sizeof(ntpServer));
  gmtOffset_sec      = doc["gmt_offset"]  | 10800L;
  daylightOffset_sec = doc["dst_offset"]  | 0;
  udpPort            = doc["udp_port"]    | 8889;
  strlcpy(targetIpAddress, doc["robot_ip"] | "192.168.1.27",  sizeof(targetIpAddress));
}

// Forward declarations for filesystem manager
void serialHandleFS(const char* arg);

// Forward declarations for IR remote helpers
void initIR();
void renderIR();
void irScanFiles();
void irOpenDir(const char* path);
void irLoadDevice(const char* path);
void irSendButton(int btnIdx);
void serialHandleIR(const char* arg);

// ================================================================
// Setup & Loop
// ================================================================

void setup() {
  Serial.setRxBufferSize(8192);
  Serial.begin(115200);
  debugln("N#1 initializing...");
  loadSettings();

  display.begin();
  display.setRotation(0);   // default portrait until IMU overrides
  display.setEpdMode(epd_mode_t::epd_fastest);
  display.setBrightness(15);

  runSplashAnimation();
  createStatusSprite();

  pinMode(VIN_DETECT, INPUT);
  battery.begin();   // AW32001 init — begin() enables charging internally
  // Initial battery read so the header icon is filled from the start
  chargeLevel    = battery.getChargeLevel();
  batteryVoltage = battery.getVoltage();
  voltagePercent = voltageToPercent(batteryVoltage);
  chargeStatus   = battery.getChargeStatus();
  // VIN_DETECT monitors USB-C VBUS presence (the Nesso N1 has no separate VIN pin;
  // USB-C is the only external power input).  AW32001 charge status is kept as a
  // belt-and-suspenders fallback in case VBUS detection is momentarily unreliable.
  onExternalPower = (digitalRead(VIN_DETECT) == HIGH) ||
                    (chargeStatus != NessoBattery::NOT_CHARGING);
  batteryCharging = (chargeStatus == NessoBattery::CHARGING ||
                     chargeStatus == NessoBattery::PRE_CHARGE);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(KEY1, INPUT_PULLUP);
  pinMode(KEY2, INPUT_PULLUP);

  lastLEDflipTime   = millis();
  batteryCheckTime  = millis();
  buttonCheckTime   = millis();
  lastOrientationMs = millis();
  lastActivityMs    = millis();

  Wire.begin();
  IMU.begin();
  touchReady = touch.begin();
  if (touchReady) splashLog("> Touch: ready", COLOR_GREEN);
  else            splashLog("> Touch: not found", COLOR_GRAY);

  joystickAvailable = ss.begin(0x50);
  if (joystickAvailable) splashLog("> Joystick: found", COLOR_GREEN);
  else                   splashLog("> Joystick: not found", COLOR_GRAY);

  if (LittleFS.begin(true)) {
    loadConfig();
    irScanFiles();
    splashLog("> FS: ready", COLOR_GREEN);
  } else {
    splashLog("> FS: mount failed", COLOR_RED);
  }

  splashLog("> WiFi: connecting...", COLOR_ORANGE);
  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    char ipMsg[52];
    String ipStr = WiFi.localIP().toString();
    snprintf(ipMsg, sizeof(ipMsg), "> WiFi: %s", ipStr.c_str());
    splashLog(ipMsg, COLOR_GREEN);

    splashLog("> Time: syncing...", COLOR_ORANGE);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);

    if (getLocalTime(&timeinfo)) {
      splashLog("> Time: synced", COLOR_GREEN);
    } else {
      splashLog("> Time: sync failed", COLOR_RED);
    }
  } else {
    splashLog("> WiFi: connection failed", COLOR_RED);
  }

  splashLog("> Ready.", COLOR_GREEN);
  delay(900);

  // Poll until the IMU has its first sample (up to 200 ms), then orient
  for (int i = 0; i < 20 && !IMU.accelerationAvailable(); i++) delay(10);
  updateOrientation();

  currentFunction = FUNCTION_MAIN;
  lastFunction    = -1;

  debugln("N#1 initialized.");
  serialPrintHelp();
  // BLE stack init is deferred — happens silently ~2 s after first clock render.
  // Controlled by btStartupEnabled (loaded from NVS above).
}

void loop() {
  unsigned long msNow = millis();

  if (msNow - buttonCheckTime > 20) {
    buttonCheckTime = msNow;
    checkButtons(msNow);
    checkTouch(msNow);
    updateBuzzer(msNow);
  }

  {
    bool lowBat      = (voltagePercent > 0 && voltagePercent <= 20);
    bool anyScanning = (wifiScanning || btScanning || loraListening);
    if (lowBat) {
      if (msNow - lastLEDflipTime > 250) {
        ledStatus = !ledStatus;
        digitalWrite(LED_BUILTIN, ledStatus);
        lastLEDflipTime = msNow;
      }
    } else if (anyScanning) {
      if (msNow - lastLEDflipTime > 1000) {
        ledStatus = !ledStatus;
        digitalWrite(LED_BUILTIN, ledStatus);
        lastLEDflipTime = msNow;
      }
    } else {
      if (ledStatus) {
        ledStatus = false;
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
  }

  if (msNow - batteryCheckTime > 30333) {
    batteryCheckTime = msNow;
    chargeLevel      = battery.getChargeLevel();
    batteryVoltage   = battery.getVoltage();
    voltagePercent   = voltageToPercent(batteryVoltage);
    batteryCheck();

    unsigned long s = millis() / 1000;
    sprintf(uptimeString, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

    debug(batteryVoltage);
    debug(" ");
    debug(chargeLevel);
    debugln("%");
  }

  if (msNow - lastOrientationMs > 300) {
    lastOrientationMs = msNow;
    updateOrientation();
  }

  serialCheckInput();
  btProcessPendingEvents();
  checkPowerManagement(msNow);
  renderFunction();
  if (webFMPendingStart) { webFMPendingStart = false; webFMStart(); }
  webFMHandle();

  // Deferred BLE init: silently start the stack ~2 s after boot if enabled.
  // This runs once — after the clock has already rendered at least one frame.
  static bool btDeferDone = false;
  if (!btDeferDone && btStartupEnabled && msNow > 2000) {
    btDeferDone = true;
    if (!btInitialized && !btInitFailed) btInitStack();
  }
}


// ================================================================
// IMU Orientation
// ================================================================

void createStatusSprite() {
  statusSprite.deleteSprite();
  statusSprite.createSprite(display.width(), display.height() - SPRITE_Y);
}

void updateOrientation() {
  if (key1Down || key2Down) return;  // suppress during button hold — avoids flicker on long press
  float ax, ay, az;
  if (!IMU.accelerationAvailable()) return;
  IMU.readAcceleration(ax, ay, az);

  float xyTotal = fabsf(ax) + fabsf(ay);

  if (imuDebugEnabled) {
    char buf[120];
    snprintf(buf, sizeof(buf),
      "[IMU] ax=%.3f ay=%.3f az=%.3f xy=%.3f rot=%d",
      ax, ay, az, xyTotal, currentRotation);
    Serial.println(buf);
  }

  // When flat on a table the XY components are weak and IMU bias can trigger
  // a spurious rotation.  Only act when the device is meaningfully tilted.
  if (xyTotal < 0.5f) return;

  // High threshold to enter landscape (requires deliberate sideways tilt).
  // Lower threshold to return to portrait so it snaps back easily.
  const float landscapeThreshold = 0.65f;
  const float portraitThreshold  = 0.35f;
  uint8_t newRotation = currentRotation;

  bool currentlyLandscape = (currentRotation == 1 || currentRotation == 3);
  if (!currentlyLandscape) {
    // Portrait → only go landscape if strongly tilted sideways
    if (fabsf(ay) > fabsf(ax) + landscapeThreshold) {
      newRotation = (ay < 0) ? 1 : 3;
    }
  } else {
    // Landscape → return to portrait at lower threshold, or stay landscape
    if (fabsf(ax) > fabsf(ay) + portraitThreshold) {
      newRotation = (ax > 0) ? 0 : 2;
    } else if (fabsf(ay) > fabsf(ax) + landscapeThreshold) {
      newRotation = (ay < 0) ? 1 : 3;
    }
  }

  if (newRotation != currentRotation) {
    if (imuDebugEnabled) {
      char buf[80];
      snprintf(buf, sizeof(buf), "[IMU] rotation change: %d -> %d", currentRotation, newRotation);
      Serial.println(buf);
    }
    currentRotation = newRotation;
    display.setRotation(currentRotation);
    createStatusSprite();
    lastFunction = -1;
    lastMinute   = -1;  // force clock redraw
    debugln("Orientation changed");
  }
}


// ================================================================
// Display helpers
// ================================================================

// Header icon layout (right to left):
//   [BT icon][WiFi icon][Battery icon|cap]  — all right-aligned
// Battery right edge at display.width()-2, body 22px wide + 3px cap.
// WiFi center 14px left of battery left edge (4px gap).
// BT center 13px left of WiFi left edge (2px gap).

// Battery icon — rightmost, body flush to right edge.
void drawBatteryIcon() {
  int bx = display.width() - 27;  // body left edge; cap ends at display.width()-2
  int by = 7;

  display.fillRect(bx - 2, 0, 29, 22, BG_COLOR);

  uint16_t fillColor = (voltagePercent > 50) ? COLOR_GREEN  :
                       (voltagePercent > 20) ? COLOR_ORANGE : COLOR_RED;

  display.drawRect(bx, by, 22, 10, COLOR_GRAY);
  display.fillRect(bx + 22, by + 3, 3, 4, COLOR_GRAY);
  int fillW = max(0, (int)(18.0f * voltagePercent / 100.0f));
  if (fillW > 0)
    display.fillRect(bx + 2, by + 2, fillW, 6, fillColor);
}

// WiFi icon — to the left of the battery.
void drawWiFiStatus() {
  int cx = display.width() - 47;  // extra gap between WiFi and battery
  int cy = 12;

  display.fillRect(cx - 12, 0, 25, 22, BG_COLOR);

  if (WiFi.status() == WL_CONNECTED) {
    display.fillArc(cx, cy, 8, 11, 315, 45, WIFI_COLOR);
    display.fillArc(cx, cy, 3,  6, 315, 45, WIFI_COLOR);
    display.fillCircle(cx, cy, 2, WIFI_COLOR);
  } else {
    uint16_t dim = display.color565(70, 70, 70);
    display.fillArc(cx, cy, 8, 11, 315, 45, dim);
    display.fillArc(cx, cy, 3,  6, 315, 45, dim);
    display.fillCircle(cx, cy, 2, dim);
    display.drawLine(cx - 9, cy + 3, cx + 5, cy - 12, COLOR_RED);
    display.drawLine(cx - 8, cy + 3, cx + 6, cy - 12, COLOR_RED);
  }
}

// Bluetooth icon — to the left of the WiFi icon.
// Classic BT symbol: vertical bar + upper and lower right-pointing chevrons.
void drawBTIcon() {
  int cx = display.width() - 70;  // shifted left to match new WiFi position
  int cy = 11;

  display.fillRect(cx - 9, 0, 19, 22, BG_COLOR);

  uint16_t color = btScanning  ? display.color565(0, 120, 255) :
                   btInitialized ? display.color565(0, 60, 120) :
                                   display.color565(50, 50, 50);

  // Vertical bar
  display.drawLine(cx, cy - 7, cx, cy + 7, color);
  // Upper chevron: bar top → right → bar center
  display.drawLine(cx,     cy - 7, cx + 5, cy - 3, color);
  display.drawLine(cx + 5, cy - 3, cx,     cy,      color);
  // Lower chevron: bar center → right → bar bottom
  display.drawLine(cx,     cy,     cx + 5, cy + 3, color);
  display.drawLine(cx + 5, cy + 3, cx,     cy + 7, color);
}

void renderHeader() {
  display.fillScreen(BG_COLOR);
  drawBatteryIcon();
  drawWiFiStatus();
  drawBTIcon();
}

void IRAM_ATTR loraISR() {
  loraPacketFlag = true;
}


// ================================================================
// Touch handling
// ================================================================

// Transform raw FT6x36 panel coordinates (portrait-native, 135×240)
// into screen coordinates for the current display rotation.
static void transformTouch(int16_t& tx, int16_t& ty) {
  const int PW = 135, PH = 240;
  int16_t sx, sy;
  switch (currentRotation) {
    default:
    case 0: sx = tx;          sy = ty;          break;  // portrait
    case 1: sx = ty;          sy = PW - 1 - tx; break;  // landscape CW
    case 2: sx = PW - 1 - tx; sy = PH - 1 - ty; break;  // portrait flipped
    case 3: sx = PH - 1 - ty; sy = tx;          break;  // landscape CCW
  }
  tx = sx; ty = sy;
}

// Forward declarations
void onTap(int16_t sx, int16_t sy);
void onSwipe(int16_t dx, int16_t dy);

void checkTouch(unsigned long msNow) {
  if (!touchReady) return;

  bool down = touch.isTouched();

  if (down && !touchActive) {
    // Touch started
    int16_t tx, ty;
    if (touch.read(tx, ty)) {
      transformTouch(tx, ty);
      touchDownX  = touchCurrX = tx;
      touchDownY  = touchCurrY = ty;
      touchActive = true;
      touchDownMs = msNow;
    }
  } else if (down && touchActive) {
    // Touch moved — update current position
    int16_t tx, ty;
    if (touch.read(tx, ty)) {
      transformTouch(tx, ty);
      touchCurrX = tx;
      touchCurrY = ty;
    }
  } else if (!down && touchActive) {
    // Touch released — classify gesture
    touchActive = false;
    int16_t dx = touchCurrX - touchDownX;
    int16_t dy = touchCurrY - touchDownY;
    unsigned long dur = msNow - touchDownMs;

    if (abs(dx) < 15 && abs(dy) < 15 && dur < 400) {
      onTap(touchCurrX, touchCurrY);
    } else if (abs(dx) > 35 || abs(dy) > 35) {
      onSwipe(dx, dy);
    }
  }
}

// Forward declarations for key actions (needed by touch handlers below)
void onKey1Short();
void onKey1Long();
void onKey2Short();
void onKey2Long();
void uiClick(uint16_t freq, uint16_t durationMs);

// Forward declarations for redraw flags (defined later with their render functions)
extern bool vaderNeedsRedraw;
extern bool obiwanNeedsRedraw;

// Forward declarations for WiFi scan helpers
void initWiFi();
void renderWiFi();
void applyWiFiSettings();
void handleWiFiSettingsTap(int16_t sx, int16_t sy);
void renderWiFiReset();
void handleWiFiResetTap(int16_t sx, int16_t sy);

// Forward declarations for BT helpers
void btStartScan();
void btStopScan();
void applyBTSettings();
void btInitStack();
void initBT();
void renderBT();
void handleBTSettingsTap(int16_t sx, int16_t sy);
void renderBTDetail();
void renderBTConnected();
void renderBTReset();
void handleBTResetTap(int16_t sx, int16_t sy);

// Forward declarations for reset pages
void renderLoraReset();
void handleLoraResetTap(int16_t sx, int16_t sy);
void renderBatteryReset();
void handleBatteryResetTap(int16_t sx, int16_t sy);

// Handle tap on settings rows and apply/cancel buttons
// Must mirror the layout constants in renderLoraSettings() exactly.
void handleLoraSettingsTap(int16_t sx, int16_t sy) {
  int  sw          = statusSprite.width();
  int  sh          = statusSprite.height();
  bool isLandscape = sw > sh;
  int  sprite_y    = sy - SPRITE_Y;

  static const int ITEM_COUNT = 5;  // 4 items + RESET
  int divY     = isLandscape ? 22 : 26;
  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int btnH     = isLandscape ? 14 : 22;
  int btnY     = sh - 6 - btnH;
  int sepY     = btnY - 6;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);
  int offset   = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);

  // Tap on a settings row
  for (int i = 0; i < visRows; i++) {
    int idx  = i + offset;
    int rowY = startY + i * rowH;
    if (sprite_y >= rowY && sprite_y < rowY + rowH) {
      if (settingsCursor == idx) onKey1Short();  // already selected → cycle value
      else                       settingsCursor = idx;
      return;
    }
  }

  // Tap on APPLY / CANCEL buttons (bottom, side-by-side in both orientations)
  if (sprite_y >= btnY && sprite_y < btnY + btnH) {
    if (sx < sw / 2) onKey1Long();   // left = APPLY
    else             onKey2Long();   // right = CANCEL
  }
}

void handleBTSettingsTap(int16_t sx, int16_t sy) {
  int  sw          = statusSprite.width();
  int  sh          = statusSprite.height();
  bool isLandscape = sw > sh;
  int  sprite_y    = sy - SPRITE_Y;

  static const int ITEM_COUNT = 6;  // 5 items + RESET (was 4 — bug fix)
  int divY     = isLandscape ? 22 : 26;
  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int btnH     = isLandscape ? 14 : 22;
  int btnY     = sh - 6 - btnH;
  int sepY     = btnY - 6;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);
  int offset   = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);

  for (int i = 0; i < visRows; i++) {
    int idx  = i + offset;
    int rowY = startY + i * rowH;
    if (sprite_y >= rowY && sprite_y < rowY + rowH) {
      if (settingsCursor == idx) onKey1Short();
      else                       settingsCursor = idx;
      return;
    }
  }
  if (sprite_y >= btnY && sprite_y < btnY + btnH) {
    if (sx < sw / 2) onKey1Long();
    else             onKey2Long();
  }
}

void onTap(int16_t sx, int16_t sy) {
  resetActivity();
  uiClick(1200, 18);
  if (navState == NAV_RESET && currentFunction == FUNCTION_LORA) {
    handleLoraResetTap(sx, sy);
  } else if (navState == NAV_RESET && currentFunction == FUNCTION_BT) {
    handleBTResetTap(sx, sy);
  } else if (navState == NAV_RESET && currentFunction == FUNCTION_WIFI) {
    handleWiFiResetTap(sx, sy);
  } else if (navState == NAV_RESET && currentFunction == FUNCTION_BATTERY) {
    handleBatteryResetTap(sx, sy);
  } else if (navState == NAV_SETTINGS && currentFunction == FUNCTION_LORA) {
    handleLoraSettingsTap(sx, sy);
  } else if (navState == NAV_SETTINGS && currentFunction == FUNCTION_BT) {
    handleBTSettingsTap(sx, sy);
  } else if (navState == NAV_SETTINGS && currentFunction == FUNCTION_WIFI) {
    handleWiFiSettingsTap(sx, sy);
  } else if (navState == NAV_SETTINGS && currentFunction == FUNCTION_BATTERY) {
    handleBatterySettingsTap(sx, sy);
  } else if (currentFunction == FUNCTION_BT && navState == NAV_NORMAL) {
    if (btSelectedAddr[0] != '\0') {
      btSelectedAddr[0] = '\0';   // tap in detail view → back to list
    } else if (!btScanning && btLogCount == 0 && !btConnected) {
      // Tap on the SCAN button (shown when idle and no results yet)
      btScanRequested = true;
      btStartScan();
    } else if (!btScanning && btLogCount > 0) {
      // Tap the scan-dot area at top to restart scan
      int sprite_y = sy - SPRITE_Y;
      bool isLand  = statusSprite.width() > statusSprite.height();
      int dotHitY  = isLand ? 10 : 29;
      if (sprite_y < dotHitY + 16) {
        btScanRequested = true;
        btStartScan();
      } else {
        // Tap on a device row
        int sw      = statusSprite.width(), sh = statusSprite.height();
        int rowH    = isLand ? 20 : 22;
        int listTop = isLand ? 42 : 64;
        int visRows = (sh - listTop - 12) / rowH;
        for (int i = 0; i < visRows; i++) {
          int idx  = i + btScrollOffset;
          if (idx >= btLogCount) break;
          int rowY = listTop + i * rowH;
          if (sprite_y >= rowY && sprite_y < rowY + rowH) {
            strncpy(btSelectedAddr, btLog[idx].addr, 17);
            btSelectedAddr[17] = '\0';
            break;
          }
        }
      }
    } else if (btScanning) {
      // Tap the scan-dot to stop
      int sprite_y = sy - SPRITE_Y;
      bool isLand  = statusSprite.width() > statusSprite.height();
      int dotHitY  = isLand ? 10 : 29;
      if (sprite_y < dotHitY + 16) {
        btScanRequested = false;
        btStopScan();
        // Clear log so SCAN button reappears
        btLogCount = 0; btScrollOffset = 0; btTotalSeen = 0;
      } else {
        // Tap on a device row
        int sw      = statusSprite.width(), sh = statusSprite.height();
        int rowH    = isLand ? 20 : 22;
        int listTop = isLand ? 42 : 64;
        int visRows = (sh - listTop - 12) / rowH;
        for (int i = 0; i < visRows; i++) {
          int idx  = i + btScrollOffset;
          if (idx >= btLogCount) break;
          int rowY = listTop + i * rowH;
          if (sprite_y >= rowY && sprite_y < rowY + rowH) {
            strncpy(btSelectedAddr, btLog[idx].addr, 17);
            btSelectedAddr[17] = '\0';
            break;
          }
        }
      }
    }
  } else if (currentFunction == FUNCTION_IR && navState == NAV_NORMAL) {
    int sprite_y = sy - SPRITE_Y;
    int sw = statusSprite.width(), sh = statusSprite.height();
    bool isLand = sw > sh;

    if (irLevel == IR_LEVEL_LIST) {
      // ── Directory browser ───────────────────────────────────────
      int titleH  = isLand ? 20 : 24;
      // Tap title = go back to parent directory
      if (sprite_y < titleH) {
        if (strcmp(irBrowsePath, "/irdb") != 0) {
          char parent[64]; strlcpy(parent, irBrowsePath, sizeof(parent));
          char* last = strrchr(parent, '/');
          if (last && last != parent) *last = '\0';
          else strlcpy(parent, "/irdb", sizeof(parent));
          irOpenDir(parent);
        }
        return;
      }
      int rowH    = isLand ? 18 : 20;
      int listTop = titleH + 4;
      int visRows = max(1, (sh - listTop - 4) / rowH);
      for (int i = 0; i < visRows; i++) {
        int n = i + irListOff;
        if (n >= irDirCount) break;
        int rowY = listTop + i * rowH;
        if (sprite_y >= rowY && sprite_y < rowY + rowH) {
          if (irDir[n].isDir) {
            irOpenDir(irDir[n].path);
          } else {
            irLoadDevice(irDir[n].path);
            irLevel = IR_LEVEL_REMOTE;
            irBtnPageOff = 0;
            strlcpy(irSavedPath, irDir[n].path, sizeof(irSavedPath));
            saveSettings();
          }
          return;
        }
      }

    } else if (irLevel == IR_LEVEL_REMOTE) {
      // ── Level 1: Remote buttons ─────────────────────────────────
      int titleH = isLand ? 22 : 26;
      if (sprite_y < titleH) { irLevel = IR_LEVEL_LIST; return; }
      int pad   = 3;
      int areaY = titleH + pad;
      int cols  = 2;
      int btnH  = isLand ? 22 : 24;
      int btnW  = (sw - pad * (cols + 1)) / cols;
      int visRows = max(1, (sh - areaY - pad) / (btnH + pad));
      for (int row = irBtnPageOff; row < irBtnPageOff + visRows; row++) {
        for (int col = 0; col < cols; col++) {
          int idx = row * cols + col;
          if (idx >= irBtnCount) break;
          int bx   = pad + col * (btnW + pad);
          int rowY = areaY + (row - irBtnPageOff) * (btnH + pad);
          if (sx >= bx && sx < bx + btnW && sprite_y >= rowY && sprite_y < rowY + btnH) {
            irFlashX = sx; irFlashY = sprite_y; irFlashMs = millis();
            irSendButton(idx);
            return;
          }
        }
      }
    }
  } else if (currentFunction == FUNCTION_WIFI && navState == NAV_NORMAL) {
    int sprite_y = sy - SPRITE_Y;
    bool isLand  = statusSprite.width() > statusSprite.height();
    int dotHitY  = isLand ? 10 : 29;
    if (!wifiScanning && wifiScanCount == 0) {
      // Tap the SCAN button
      wifiScanOffset = 0;
      WiFi.scanNetworks(true);   // async scan
      wifiScanning = true;
    } else if (wifiScanning && sprite_y < dotHitY + 16) {
      // Tap the blinking dot to cancel
      WiFi.scanDelete();
      wifiScanning  = false;
      wifiScanCount = 0;
    } else if (!wifiScanning && wifiScanCount > 0) {
      // Tap dot area at top → restart scan
      if (sprite_y < dotHitY + 16) {
        WiFi.scanDelete();
        wifiScanCount = 0;
        wifiScanOffset = 0;
        WiFi.scanNetworks(true);
        wifiScanning = true;
      } else {
        // Tap on a list row → connect to open network
        int rowH    = isLand ? 20 : wifiDebugMode ? 30 : 22;
        int listTop = isLand ? 42 : 64;
        int sh      = statusSprite.height();
        int visRows = (sh - listTop - 12) / rowH;
        for (int i = 0; i < visRows; i++) {
          int idx  = i + wifiScanOffset;
          if (idx >= wifiScanCount) break;
          int rowY = listTop + i * rowH;
          if (sprite_y >= rowY && sprite_y < rowY + rowH) {
            if (wifiScanLog[idx].encType == 0) {  // OPEN
              strncpy(wifiUserSsid, wifiScanLog[idx].ssid, 32); wifiUserSsid[32] = '\0';
              wifiUserPass[0] = '\0';
              wifiAuthFailed  = false;
              saveSettings();
              WiFi.begin(wifiUserSsid);
              char msg[64];
              snprintf(msg, sizeof(msg), "Connecting to %s — credentials saved.", wifiUserSsid);
              serialWritelnAll(msg);
            } else {
              serialWritelnAll("Secured network — use: wifi connect ssid <SSID> pass <PASSWORD>");
            }
            break;
          }
        }
      }
    }
  } else if (currentFunction == FUNCTION_LORA && navState == NAV_NORMAL) {
    // Tap on LISTEN button or scan-dot to toggle listening
    int sprite_y = sy - SPRITE_Y;
    bool isLand  = statusSprite.width() > statusSprite.height();
    int dotHitY  = isLand ? 10 : 29;
    if (!loraListening) {
      // Re-arm SPI before startReceive in case display rendered since last entry
      SPI.begin(SCK, MISO, MOSI, LORA_CS);
      loraListening = true;
      lora.startReceive();
    } else if (sprite_y < dotHitY + 16) {
      // Tap the indicator dot to stop
      loraListening = false;
      lora.standby();
    }
    // Taps elsewhere (on log rows) are handled by the existing swipe scroll — no action
  } else if (currentFunction == FUNCTION_MEDIA) {
    const BuzzNote* mel  = nullptr; int len = 0;
    switch (mediaSubScreen) {
      case 1: mel=VADER_MELODY;  len=VADER_MELODY_LEN;  vaderNeedsRedraw=true;  break;
      case 2: mel=OBIWAN_MELODY; len=OBIWAN_MELODY_LEN; obiwanNeedsRedraw=true; break;
    }
    if (buzzerPlaying) stopBuzzer(); else if (mel) startBuzzer(mel, len);
  }
}

void onSwipe(int16_t dx, int16_t dy) {
  if (abs(dx) >= abs(dy)) {
    // Horizontal swipe → screen navigation (blocked inside settings)
    if (navState != NAV_NORMAL) return;
    if (dx > 0) onKey2Short();   // swipe right = previous screen
    else        onKey1Short();   // swipe left  = next screen
  } else {
    // Vertical swipe
    if (navState == NAV_SETTINGS &&
        (currentFunction == FUNCTION_LORA || currentFunction == FUNCTION_BT ||
         currentFunction == FUNCTION_WIFI  || currentFunction == FUNCTION_BATTERY)) {
      if (dy < 0) settingsScrollOffset++;
      else        settingsScrollOffset--;
      settingsScrollOffset = constrain(settingsScrollOffset, 0, 4);
    } else if (currentFunction == FUNCTION_MEDIA && navState == NAV_NORMAL) {
      // Vertical swipe inside media screen → change sub-screen
      if (dy < 0) mediaSubScreen = (mediaSubScreen + 1) % 3;
      else        mediaSubScreen = (mediaSubScreen + 2) % 3;
      stopBuzzer();
      lastMediaSubScreen = -1;   // force reinit
    } else if (currentFunction == FUNCTION_LORA && navState == NAV_NORMAL) {
      if (dy < 0) loraScrollOffset = min(loraScrollOffset + 1, loraLogCount - 1);
      else        loraScrollOffset = max(loraScrollOffset - 1, 0);
      loraScrollOffset = constrain(loraScrollOffset, 0, max(0, loraLogCount - 1));
    } else if (currentFunction == FUNCTION_BT && navState == NAV_NORMAL) {
      if (dy < 0) btScrollOffset = min(btScrollOffset + 1, btLogCount - 1);
      else        btScrollOffset = max(btScrollOffset - 1, 0);
      btScrollOffset = constrain(btScrollOffset, 0, max(0, btLogCount - 1));
    } else if (currentFunction == FUNCTION_WIFI && navState == NAV_NORMAL) {
      if (dy < 0) wifiScanOffset = min(wifiScanOffset + 1, wifiScanCount - 1);
      else        wifiScanOffset = max(wifiScanOffset - 1, 0);
      wifiScanOffset = constrain(wifiScanOffset, 0, max(0, wifiScanCount - 1));
    } else if (currentFunction == FUNCTION_IR && navState == NAV_NORMAL) {
      if (irLevel == IR_LEVEL_LIST) {
        if (dy < 0) irListOff = min(irListOff + 1, max(0, irDirCount - 1));
        else        irListOff = max(irListOff - 1, 0);
      } else if (irLevel == IR_LEVEL_REMOTE) {
        bool isLandScroll = statusSprite.width() > statusSprite.height();
        int btnH = isLandScroll ? 22 : 24;
        int titleH = isLandScroll ? 22 : 26;
        int areaH = statusSprite.height() - titleH - 6;
        int visRows = max(1, areaH / (btnH + 3));
        int totalRows = (irBtnCount + 1) / 2;
        if (dy < 0) irBtnPageOff = min(irBtnPageOff + 1, max(0, totalRows - visRows));
        else        irBtnPageOff = max(irBtnPageOff - 1, 0);
      }
    }
  }
}


// ================================================================
// Button handling — short/long press detection
// ================================================================

void checkButtons(unsigned long msNow) {
  bool anyKey = (digitalRead(KEY1) == LOW) || (digitalRead(KEY2) == LOW);
  if (anyKey) resetActivity();
  bool k1 = (digitalRead(KEY1) == LOW);
  bool k2 = (digitalRead(KEY2) == LOW);

  // ── KEY1 ──────────────────────────────────────────────────────
  if (k1 && !key1Down) {
    key1Down      = true;
    key1LongFired = false;
    key1PressedAt = msNow;
  } else if (!k1 && key1Down) {
    key1Down = false;
    if (!key1LongFired && (msNow - key1PressedAt) >= BTN_DEBOUNCE_MS)
      onKey1Short();
  } else if (k1 && key1Down && !key1LongFired && (msNow - key1PressedAt) >= LONG_PRESS_MS) {
    key1LongFired = true;
    onKey1Long();
  }

  // ── KEY2 ──────────────────────────────────────────────────────
  if (k2 && !key2Down) {
    key2Down      = true;
    key2LongFired = false;
    key2PressedAt = msNow;
  } else if (!k2 && key2Down) {
    key2Down = false;
    if (!key2LongFired && (msNow - key2PressedAt) >= BTN_DEBOUNCE_MS)
      onKey2Short();
  } else if (k2 && key2Down && !key2LongFired && (msNow - key2PressedAt) >= LONG_PRESS_MS) {
    key2LongFired = true;
    onKey2Long();
  }
}

// ── Key event handlers ────────────────────────────────────────────

// KEY1 short — navigate forward / cycle setting value
void onKey1Short() {
  uiClick(800, 25);
  if (navState == NAV_NORMAL) {
    do {
      currentFunction = static_cast<MainFunctions>((currentFunction + 1) % mainFunctionCount);
    } while (currentFunction == FUNCTION_CONTROLLER && !joystickAvailable);
    debugln("KEY1 short: next function");
  } else if (navState == NAV_SETTINGS) {
    // RESET item is always the last entry in each function's list — navigate to reset page
    if ((currentFunction == FUNCTION_WIFI    && settingsCursor == 2) ||
        (currentFunction == FUNCTION_LORA    && settingsCursor == 4) ||
        (currentFunction == FUNCTION_BT      && settingsCursor == 5) ||
        (currentFunction == FUNCTION_BATTERY && settingsCursor == 4)) {
      navState = NAV_RESET;
      resetFeedbackMs = 0; resetFeedbackBtn = -1;
      return;
    }
    if (currentFunction == FUNCTION_WIFI) {
      switch (settingsCursor) {
        case 0: wifiDebugMode = !wifiDebugMode; break;
        case 1: wifiAutoScan  = !wifiAutoScan;  break;
      }
    } else if (currentFunction == FUNCTION_LORA) {
      switch (settingsCursor) {
        case 0: loraPresetIdx = (loraPresetIdx + 1) % LORA_PRESET_COUNT;       break;
        case 1: loraFreqIdx   = (loraFreqIdx   + 1) % LORA_FREQ_COUNT;         break;
        case 2: loraAutoReply = !loraAutoReply;                                 break;
        case 3: loraDedup     = !loraDedup;                                     break;
      }
    } else if (currentFunction == FUNCTION_BT) {
      switch (settingsCursor) {
        case 0: btScanModeIdx    = (btScanModeIdx   + 1) % 2;                    break;
        case 1: btRssiFilterIdx  = (btRssiFilterIdx + 1) % BT_RSSI_FILTER_COUNT; break;
        case 2: btDebugMode      = !btDebugMode;                                  break;
        case 3: btAdvEnabled     = !btAdvEnabled;                                 break;
        case 4: btStartupEnabled = !btStartupEnabled;                             break;
      }
    } else if (currentFunction == FUNCTION_BATTERY) {
      switch (settingsCursor) {
        case 0: dimTimeoutIdx   = (dimTimeoutIdx   + 1) % POWER_TIMEOUT_COUNT; break;
        case 1: sleepTimeoutIdx = (sleepTimeoutIdx + 1) % POWER_TIMEOUT_COUNT; break;
        case 2: lowBatIdx       = (lowBatIdx       + 1) % LOW_BAT_COUNT;       break;
        case 3: uiClickEnabled  = !uiClickEnabled;                              break;
      }
    }
  }
}

// KEY2 short — navigate backward / move to next settings item
void onKey2Short() {
  uiClick(800, 25);
  if (navState == NAV_NORMAL) {
    do {
      currentFunction = static_cast<MainFunctions>((currentFunction - 1 + mainFunctionCount) % mainFunctionCount);
    } while (currentFunction == FUNCTION_CONTROLLER && !joystickAvailable);
    debugln("KEY2 short: prev function");
  } else if (navState == NAV_SETTINGS) {
    if (currentFunction == FUNCTION_WIFI)
      settingsCursor = (settingsCursor + 1) % 3;  // 2 items + RESET
    else if (currentFunction == FUNCTION_LORA)
      settingsCursor = (settingsCursor + 1) % 5;  // 4 items + RESET
    else if (currentFunction == FUNCTION_BT)
      settingsCursor = (settingsCursor + 1) % 6;  // 5 items + RESET
    else if (currentFunction == FUNCTION_BATTERY)
      settingsCursor = (settingsCursor + 1) % 5;  // 4 items + RESET
  }
}

// KEY1 long — enter settings / apply settings
void onKey1Long() {
  uiClick(500, 35);
  if (navState == NAV_NORMAL) {
    if (currentFunction == FUNCTION_LORA || currentFunction == FUNCTION_BT ||
        currentFunction == FUNCTION_WIFI  || currentFunction == FUNCTION_BATTERY) {
      navState             = NAV_SETTINGS;
      settingsCursor       = 0;
      settingsScrollOffset = 0;
      debugln("Entered settings");
    }
  } else if (navState == NAV_SETTINGS) {
    if (currentFunction == FUNCTION_WIFI) applyWiFiSettings();
    else if (currentFunction == FUNCTION_LORA) applyLoraSettings();
    else if (currentFunction == FUNCTION_BT) applyBTSettings();
    else if (currentFunction == FUNCTION_BATTERY) applyDeviceSettings();
    navState = NAV_NORMAL;
    // No lastFunction = -1 here: the sprite-based screens redraw every frame
    // without needing a full display.fillScreen() call, so clearing lastFunction
    // would trigger initBT()/initLora() which calls display.fillScreen() → flicker.
    debugln("Settings applied");
  }
}

// KEY2 long — exit settings without applying; exit reset page back to settings
void onKey2Long() {
  uiClick(500, 35);
  if (navState == NAV_RESET) {
    navState = NAV_SETTINGS;
    resetFeedbackMs = 0; resetFeedbackBtn = -1;
    debugln("Reset page closed");
  } else if (navState == NAV_SETTINGS) {
    navState = NAV_NORMAL;
    debugln("Settings cancelled");
  }
}

// ================================================================
// Serial command interface  (USB UART + BLE Nordic UART Service)
// ================================================================

#define BLE_UART_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UART_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UART_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLECharacteristic* pBLETxChar   = nullptr;
static bool               bleUartReady = false;

static char sSerialBuf[160];  static int sSerialLen = 0;
static char sBleBuf[160];     static int sBleLen    = 0;

// ── Web file manager upload state ────────────────────────────────
static File   webFMUploadFile;
static String webFMUploadPath;
static bool   webFMUploadOk     = false;

// ── FS upload state (active between "fs upload" cmd and "---END---") ─
static bool     fsUploadActive  = false;
static File     fsUploadFile;
static char     fsUploadPath[64];
static uint32_t fsUploadBytes   = 0;
static char     fsLineBuf[12];   // only needs to hold "---END---" (9 chars)
static int      fsLineBufLen    = 0;
static bool     fsLineOverflow  = false;

// Receives bytes from the connected BLE UART client (runs on BLE RTOS task)
class BLERxCB : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic* pChar) override {
    String v = pChar->getValue();
    for (int i = 0; i < (int)v.length(); i++) {
      char c = v[i];
      if (sBleLen < (int)sizeof(sBleBuf) - 1) sBleBuf[sBleLen++] = c;
    }
  }
};
static BLERxCB bleRxCB;

// ── Output ───────────────────────────────────────────────────────

void serialWriteAll(const char* s) {
  Serial.print(s);
  if (bleUartReady && pBLETxChar && btConnected) {
    pBLETxChar->setValue((uint8_t*)s, strlen(s));
    pBLETxChar->notify();
  }
}
void serialWritelnAll(const char* s) { serialWriteAll(s); serialWriteAll("\r\n"); }

// ── Per-section static command lists ─────────────────────────────

static void printHelpNav() {
  serialWritelnAll("Navigation:");
  serialWritelnAll("  next | prev           next/prev screen");
  serialWritelnAll("  goto <screen>         main|controller|bt|wifi|lora|ir|media|battery");
  serialWritelnAll("  status                current state summary");
  serialWritelnAll("Information:");
  serialWritelnAll("  clock                 time and date (NTP UTC+3)");
  serialWritelnAll("  battery               voltage, level, charge status, uptime");
  serialWritelnAll("  controller            joystick position and last motor values");
  serialWritelnAll("  send <L> <R>          transmit motor command  (-255..255)");
}

static void printHelpWifi() {
  serialWritelnAll("WiFi:");
  serialWritelnAll("  wifi scan             start a network scan");
  serialWritelnAll("  wifi list             print scan results");
  serialWritelnAll("  wifi status           connection status + IP");
  serialWritelnAll("  wifi connect <N>      connect to open network N from scan list");
  serialWritelnAll("  wifi connect ssid <SSID> pass <PASS>   connect to any network");
  serialWritelnAll("  wifi disconnect       disconnect");
  serialWritelnAll("  wifi debug on|off     toggle debug info (channel, BSSID)");
}

static void printHelpBT() {
  serialWritelnAll("Bluetooth:");
  serialWritelnAll("  bt list               list discovered BLE devices (sorted by RSSI)");
  serialWritelnAll("  bt detail <N>         full detail for device N  (0-based)");
  serialWritelnAll("  bt scan on|off        start / stop BLE scan");
  serialWritelnAll("  bt adv on|off         advertise as NESSO (BLE UART)");
  serialWritelnAll("  bt startup on|off     auto-init BLE at boot (persisted)");
  serialWritelnAll("  bt mode active|passive");
  serialWritelnAll("  bt filter -70|-80|-90|off");
}

static void printHelpLora() {
  serialWritelnAll("LoRa:");
  serialWritelnAll("  lora list             received packets");
  serialWritelnAll("  lora send <text>      transmit a Meshtastic text message");
  serialWritelnAll("  lora reply on|off     auto-reply ACK");
  serialWritelnAll("  lora dedup on|off     duplicate suppression");
  serialWritelnAll("  lora preset 0-3       0=LONG_FAST 1=LONG_SLOW 2=MED_FAST 3=SHORT_FAST");
}

static void printHelpFS() {
  serialWritelnAll("Filesystem:");
  serialWritelnAll("  fs info               LittleFS total/used/free");
  serialWritelnAll("  fs ls [path]          list directory (default /)");
  serialWritelnAll("  fs cat <path>         print file contents");
  serialWritelnAll("  fs rm <path>          delete file");
  serialWritelnAll("  fs mkdir <path>       create directory");
  serialWritelnAll("  fs mv <src> <dst>     rename / move");
  serialWritelnAll("  fs upload <path>      paste file, end with ---END--- on its own line");
}

static void printHelpIR() {
  serialWritelnAll("IR Remote:");
  serialWritelnAll("  ir list               list all .ir files in /irdb/ (with index)");
  serialWritelnAll("  ir select <N>         load device N and open remote UI");
  serialWritelnAll("  ir send <N> <label>   send one button from device N");
  serialWritelnAll("  ir reload             re-scan /irdb/ for new files");
  serialWritelnAll("  ir pin                show IR blaster pin");
}

static void printHelpMusic() {
  serialWritelnAll("Music (matrix / vader / obiwan screens):");
  serialWritelnAll("  music on|off");
}

// ── Contextual help — status line + section commands ─────────────

void serialPrintFunctionHelp(int fn) {
  char buf[200];
  switch (fn) {
    case FUNCTION_MAIN:
      printHelpNav();
      break;
    case FUNCTION_BATTERY:
      snprintf(buf, sizeof(buf),
        "[BATTERY] dim:%s  sleep:%s  lowbat:%s  (long-press KEY1=device settings)",
        DIM_TIMEOUT_LABELS[dimTimeoutIdx],
        SLEEP_TIMEOUT_LABELS[sleepTimeoutIdx],
        LOW_BAT_LABELS[lowBatIdx]);
      serialWritelnAll(buf);
      break;
    case FUNCTION_CONTROLLER:
      snprintf(buf, sizeof(buf),
        "[CONTROLLER] WiFi:%s | send <L> <R>  (-255..255)",
        WiFi.isConnected() ? "CONNECTED" : "off");
      serialWritelnAll(buf);
      break;
    case FUNCTION_IR: {
      snprintf(buf, sizeof(buf),
        "[IR] files:%d  loaded:%s  level:%s",
        irFileCount,
        irLoadedName[0] ? irLoadedName : "none",
        irLevel == IR_LEVEL_LIST ? "list" : "remote");
      serialWritelnAll(buf);
      printHelpIR();
      break;
    }
    case FUNCTION_WIFI:
      snprintf(buf, sizeof(buf),
        "[WIFI] connected:%s  scanning:%s  debug:%s  auto-scan:%s",
        WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "no",
        wifiScanning  ? "yes" : "no",
        wifiDebugMode ? "ON"  : "OFF",
        wifiAutoScan  ? "ON"  : "OFF");
      serialWritelnAll(buf);
      printHelpWifi();
      break;
    case FUNCTION_BT:
      snprintf(buf, sizeof(buf),
        "[BT] scan:%s  adv:%s  startup:%s  mode:%s  filter:%s",
        btScanning         ? "ON"     : "off",
        btAdvEnabled       ? "ON"     : "off",
        btStartupEnabled   ? "ON"     : "off",
        btScanModeIdx == 0 ? "ACTIVE" : "passive",
        BT_RSSI_LABELS[btRssiFilterIdx]);
      serialWritelnAll(buf);
      printHelpBT();
      break;
    case FUNCTION_LORA:
      snprintf(buf, sizeof(buf),
        "[LORA] preset:%s  reply:%s  dedup:%s",
        LORA_PRESETS[loraPresetIdx].name,
        loraAutoReply ? "ON" : "off",
        loraDedup     ? "ON" : "off");
      serialWritelnAll(buf);
      printHelpLora();
      break;
    case FUNCTION_MEDIA: {
      static const char* subName[] = {"MATRIX","VADER","OBI-WAN"};
      snprintf(buf, sizeof(buf), "[MEDIA:%s] music:%s  (swipe up/down to change)",
        subName[mediaSubScreen], buzzerPlaying ? "ON" : "off");
      serialWritelnAll(buf);
      printHelpMusic();
      break;
    }
  }
}

// ── General help — full formatted dump ───────────────────────────

void serialPrintHelp() {
  serialWritelnAll("========= NESSO N1 SERIAL =========");
  printHelpNav();
  printHelpWifi();
  printHelpBT();
  printHelpLora();
  printHelpFS();
  printHelpIR();
  printHelpMusic();
  serialWritelnAll("IMU / Orientation:");
  serialWritelnAll("  imu                    single accelerometer snapshot");
  serialWritelnAll("  imu debug on|off       stream readings every 300 ms");
  serialWritelnAll("Web File Manager:");
  serialWritelnAll("  webfm                  print web file manager URL (WiFi required)");
  serialWritelnAll("===================================");
}

// ── Command helpers ───────────────────────────────────────────────

static bool cmdIs(const char* line, const char* cmd) {
  int n = strlen(cmd);
  return strncasecmp(line, cmd, n) == 0 && (line[n] == '\0' || line[n] == ' ');
}
static const char* cmdArg(const char* line, const char* cmd) {
  int n = strlen(cmd);
  if (strncasecmp(line, cmd, n) != 0) return "";
  const char* r = line + n;
  while (*r == ' ') r++;
  return r;
}

// ── Per-command handlers ──────────────────────────────────────────

void serialPrintStatus() {
  static const char* fn[] = {"main","controller","bt","lora","media","battery"};
  char buf[128];
  snprintf(buf, sizeof(buf),
    "Screen:%s  Nav:%s  WiFi:%s  BT:%s  BLE-UART:%s",
    fn[(int)currentFunction],
    navState == NAV_NORMAL ? "normal" : navState == NAV_SETTINGS ? "settings" : "reset",
    WiFi.isConnected() ? "on" : "off",
    btConnected ? "connected" : (btInitialized ? "scanning" : "off"),
    (bleUartReady && btConnected) ? "connected" : "idle");
  serialWritelnAll(buf);
}

void serialPrintClock() {
  char buf[64];
  if (getLocalTime(&timeinfo, 0))
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d  (UTC+3)",
      timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  else
    snprintf(buf, sizeof(buf), "NTP not synced");
  serialWritelnAll(buf);
}

void serialPrintBattery() {
  const char* st =
    chargeStatus == NessoBattery::CHARGING    ? "CHARGING"   :
    chargeStatus == NessoBattery::FULL_CHARGE ? "FULL"       :
    chargeStatus == NessoBattery::PRE_CHARGE  ? "PRE-CHARGE" : "IDLE";
  char buf[80];
  snprintf(buf, sizeof(buf), "%.2fV  %.0f%%  %s  uptime:%s",
    batteryVoltage, chargeLevel, st, uptimeString);
  serialWritelnAll(buf);
}

void serialPrintController() {
  char buf[80];
  snprintf(buf, sizeof(buf), "Joystick X=%d Y=%d  Motors L=%d R=%d  WiFi:%s",
    (int)joyDisplayX, (int)joyDisplayY,
    transmitCmd.leftMotor, transmitCmd.rightMotor,
    WiFi.isConnected() ? "ok" : "off");
  serialWritelnAll(buf);
}

void serialGoto(const char* name) {
  static const struct { const char* n; int f; } map[] = {
    {"main",0},{"controller",1},{"bt",2},{"wifi",3},
    {"lora",4},{"ir",5},{"media",6},{"matrix",6},{"vader",6},{"obiwan",6},{"battery",7}
  };
  for (auto& e : map) {
    if (strcasecmp(name, e.n) == 0) {
      currentFunction = (MainFunctions)e.f;
      lastFunction = -1; navState = NAV_NORMAL;
      serialWritelnAll("OK");
      return;
    }
  }
  serialWritelnAll("Unknown screen. Use: main|controller|bt|wifi|lora|ir|media|battery");
}

void serialHandleBT(const char* arg) {
  if (!*arg) {
    serialWritelnAll("bt list|detail <N>|scan on|off|pair on|off|mode active|passive|filter -70|-80|-90|off");
    return;
  }
  if (cmdIs(arg,"list")) {
    if (!btLogCount) { serialWritelnAll("No devices found."); return; }
    char buf[80];
    for (int i = 0; i < btLogCount; i++) {
      snprintf(buf, sizeof(buf), "[%d] %-22s %s  %ddBm%s",
        i, btLog[i].name[0] ? btLog[i].name : "(unnamed)",
        btLog[i].addr, (int)btLog[i].rssi,
        btLog[i].connectable ? "  CONN" : "");
      serialWritelnAll(buf);
    }
  } else if (cmdIs(arg,"detail")) {
    int idx = atoi(cmdArg(arg,"detail"));
    if (idx < 0 || idx >= btLogCount) { serialWritelnAll("Invalid index."); return; }
    char buf[64]; char ago[8];
    snprintf(buf, sizeof(buf), "Name:        %s", btLog[idx].name[0] ? btLog[idx].name : "(unnamed)"); serialWritelnAll(buf);
    snprintf(buf, sizeof(buf), "MAC:         %s", btLog[idx].addr); serialWritelnAll(buf);
    snprintf(buf, sizeof(buf), "RSSI:        %d dBm", (int)btLog[idx].rssi); serialWritelnAll(buf);
    snprintf(buf, sizeof(buf), "Connectable: %s", btLog[idx].connectable ? "yes" : "no"); serialWritelnAll(buf);
    fmtAgo(btLog[idx].lastSeenMs, ago, sizeof(ago));
    snprintf(buf, sizeof(buf), "Last seen:   %s", ago); serialWritelnAll(buf);
    if (btDebugMode && btLog[idx].rawLen > 0) {
      char hex[40] = {};
      for (int b = 0; b < min((int)btLog[idx].rawLen, 8); b++)
        snprintf(hex + b*3, 4, "%02X ", btLog[idx].rawData[b]);
      snprintf(buf, sizeof(buf), "Mfr data:    %s", hex); serialWritelnAll(buf);
    }
  } else if (cmdIs(arg,"scan")) {
    const char* v = cmdArg(arg,"scan");
    if      (strcasecmp(v,"on") ==0) { btStartScan(); }
    else if (strcasecmp(v,"off")==0) { btStopScan();  }
    else { serialWritelnAll("Usage: bt scan on|off"); return; }
    serialPrintFunctionHelp(FUNCTION_BT);
  } else if (cmdIs(arg,"pair")) {
    const char* v = cmdArg(arg,"pair");
    if      (strcasecmp(v,"on") ==0) { btAdvEnabled=true;  applyBTSettings(); }
    else if (strcasecmp(v,"off")==0) { btAdvEnabled=false; applyBTSettings(); }
    else { serialWritelnAll("Usage: bt pair on|off"); return; }
    serialPrintFunctionHelp(FUNCTION_BT);
  } else if (cmdIs(arg,"mode")) {
    const char* v = cmdArg(arg,"mode");
    if      (strcasecmp(v,"active") ==0) { btScanModeIdx=0; applyBTSettings(); }
    else if (strcasecmp(v,"passive")==0) { btScanModeIdx=1; applyBTSettings(); }
    else { serialWritelnAll("Usage: bt mode active|passive"); return; }
    serialPrintFunctionHelp(FUNCTION_BT);
  } else if (cmdIs(arg,"filter")) {
    const char* v = cmdArg(arg,"filter");
    if      (strcmp(v,"-70")==0)     btRssiFilterIdx=0;
    else if (strcmp(v,"-80")==0)     btRssiFilterIdx=1;
    else if (strcmp(v,"-90")==0)     btRssiFilterIdx=2;
    else if (strcasecmp(v,"off")==0) btRssiFilterIdx=3;
    else { serialWritelnAll("Usage: bt filter -70|-80|-90|off"); return; }
    saveSettings();
    serialPrintFunctionHelp(FUNCTION_BT);
  } else if (cmdIs(arg,"startup")) {
    const char* v = cmdArg(arg,"startup");
    if      (strcasecmp(v,"on") ==0) btStartupEnabled=true;
    else if (strcasecmp(v,"off")==0) btStartupEnabled=false;
    else { serialWritelnAll("Usage: bt startup on|off"); return; }
    saveSettings();
    char buf[48]; snprintf(buf,sizeof(buf),"BT startup: %s (takes effect after reboot)", btStartupEnabled?"ON":"OFF");
    serialWritelnAll(buf);
  } else if (cmdIs(arg,"adv")) {
    const char* v = cmdArg(arg,"adv");
    if      (strcasecmp(v,"on") ==0) { btAdvEnabled=true;  applyBTSettings(); }
    else if (strcasecmp(v,"off")==0) { btAdvEnabled=false; applyBTSettings(); }
    else { serialWritelnAll("Usage: bt adv on|off"); return; }
    serialPrintFunctionHelp(FUNCTION_BT);
  } else {
    serialWritelnAll("Unknown bt subcommand. Type 'help'.");
  }
}

void serialHandleImu(const char* arg) {
  if (cmdIs(arg,"debug")) {
    const char* v = cmdArg(arg,"debug");
    if      (strcasecmp(v,"on") ==0) { imuDebugEnabled=true;  serialWritelnAll("[IMU] debug stream ON  (type 'imu debug off' to stop)"); }
    else if (strcasecmp(v,"off")==0) { imuDebugEnabled=false; serialWritelnAll("[IMU] debug stream OFF"); }
    else { serialWritelnAll("Usage: imu debug on|off"); }
  } else {
    // Single snapshot
    if (!IMU.accelerationAvailable()) { serialWritelnAll("[IMU] no sample available"); return; }
    float ax, ay, az;
    IMU.readAcceleration(ax, ay, az);
    char buf[120];
    snprintf(buf, sizeof(buf),
      "[IMU] ax=%.3f ay=%.3f az=%.3f xy=%.3f rot=%d  debug:%s",
      ax, ay, az, fabsf(ax)+fabsf(ay), currentRotation,
      imuDebugEnabled ? "ON" : "off");
    serialWritelnAll(buf);
  }
}

void serialHandleIR(const char* arg) {
  if (!*arg) {
    serialWritelnAll("ir list|select <N>|send <N> <btn>|reload|pin");
    return;
  }
  if (cmdIs(arg,"list")) {
    char buf[80];
    for (int i = 0; i < irFileCount; i++) {
      snprintf(buf, sizeof(buf), "%2d: [%c] %s", i, i == irSelectedIdx ? '*' : ' ', irFiles[i].name);
      serialWritelnAll(buf);
    }
    if (irFileCount == 0) serialWritelnAll("No .ir files found in /irdb/");
  } else if (cmdIs(arg,"reload")) {
    irScanFiles();
    irOpenDir(irBrowsePath[0] ? irBrowsePath : "/irdb");
    char buf[32]; snprintf(buf, sizeof(buf), "Found %d device(s).", irFileCount);
    serialWritelnAll(buf);
  } else if (cmdIs(arg,"send")) {
    const char* rest = cmdArg(arg,"send");
    int devIdx = atoi(rest);
    while (*rest && *rest != ' ') rest++;
    while (*rest == ' ') rest++;
    if (devIdx < 0 || devIdx >= irFileCount || !*rest) {
      serialWritelnAll("Usage: ir send <device N> <button label>  (use 'ir list' for indices)");
      return;
    }
    if (irSelectedIdx != devIdx) irLoadDevice(irFiles[devIdx].path);
    int btnIdx = -1;
    for (int i = 0; i < irBtnCount; i++) {
      if (!strcasecmp(irBtns[i].label, rest)) { btnIdx = i; break; }
    }
    if (btnIdx < 0) {
      serialWritelnAll("Button not found — label is case-sensitive after first char.");
      return;
    }
    irSendButton(btnIdx);
    char buf[64]; snprintf(buf, sizeof(buf), "Sent %s -> %s", irFiles[devIdx].name, irBtns[btnIdx].label);
    serialWritelnAll(buf);
  } else if (cmdIs(arg,"select")) {
    const char* rest = cmdArg(arg,"select");
    int idx = atoi(rest);
    if (idx < 0 || idx >= irFileCount) {
      serialWritelnAll("Usage: ir select <N>  (use 'ir list' for indices)");
      return;
    }
    irLoadDevice(irFiles[idx].path);
    strlcpy(irSavedPath, irFiles[idx].path, sizeof(irSavedPath));
    saveSettings();
    if (currentFunction == FUNCTION_IR) irLevel = IR_LEVEL_REMOTE;
    char buf[64]; snprintf(buf, sizeof(buf), "Selected: %s", irLoadedName);
    serialWritelnAll(buf);
  } else if (cmdIs(arg,"pin")) {
    char buf[48]; snprintf(buf, sizeof(buf), "IR blaster: GPIO %d (IR_TX_PIN).", IR_SEND_PIN);
    serialWritelnAll(buf);
  } else {
    serialWritelnAll("Unknown ir subcommand. Type 'help'.");
  }
}

// Called per-character when fsUploadActive — char-by-char so data lines
// of any length don't overflow the command buffer.
static void fsUploadFeed(char c) {
  if (c == '\r') return;
  if (c == '\n') {
    fsLineBuf[fsLineBufLen] = '\0';
    if (!fsLineOverflow && !strcmp(fsLineBuf, "---END---")) {
      fsUploadFile.close();
      fsUploadActive = false;
      char msg[80];
      snprintf(msg, sizeof(msg), "OK: %lu bytes -> %s", (unsigned long)fsUploadBytes, fsUploadPath);
      serialWritelnAll(msg);
      int plen = strlen(fsUploadPath);
      if (plen > 3 && !strcasecmp(fsUploadPath + plen - 3, ".ir")) {
        irScanFiles();
        snprintf(msg, sizeof(msg), "Rescanned /irdb: %d device(s)", irFileCount);
        serialWritelnAll(msg);
      }
    } else {
      if (!fsLineOverflow && fsLineBufLen > 0) {
        fsUploadFile.write((const uint8_t*)fsLineBuf, fsLineBufLen);
        fsUploadBytes += fsLineBufLen;
      }
      fsUploadFile.write('\n');
      fsUploadBytes++;
    }
    fsLineBufLen = 0;
    fsLineOverflow = false;
    return;
  }
  if (fsLineOverflow) {
    fsUploadFile.write((uint8_t)c);
    fsUploadBytes++;
  } else if (fsLineBufLen < (int)sizeof(fsLineBuf) - 1) {
    fsLineBuf[fsLineBufLen++] = c;
  } else {
    fsUploadFile.write((const uint8_t*)fsLineBuf, fsLineBufLen);
    fsUploadFile.write((uint8_t)c);
    fsUploadBytes += fsLineBufLen + 1;
    fsLineOverflow = true;
  }
}

void serialHandleFS(const char* arg) {
  if (!*arg) {
    serialWritelnAll("fs info | ls [path] | cat <path> | rm <path> | mkdir <path> | mv <src> <dst> | upload <path>");
    return;
  }

  if (cmdIs(arg, "info")) {
    char buf[80];
    snprintf(buf, sizeof(buf), "LittleFS: %lu KB total  %lu KB used  %lu KB free",
      (unsigned long)LittleFS.totalBytes()  / 1024,
      (unsigned long)LittleFS.usedBytes()   / 1024,
      (unsigned long)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
    serialWritelnAll(buf);

  } else if (cmdIs(arg, "ls")) {
    const char* path = cmdArg(arg, "ls");
    if (!*path) path = "/";
    File dir = LittleFS.open(path);
    if (!dir || !dir.isDirectory()) {
      serialWritelnAll("Not a directory.");
      if (dir) dir.close();
      return;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "--- %s ---", path);
    serialWritelnAll(buf);
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      char fullPath[64];
      snprintf(fullPath, sizeof(fullPath), "%s/%s",
        strcmp(path, "/") ? path : "", entry.name());
      bool isDir  = entry.isDirectory();
      size_t size = isDir ? 0 : entry.size();
      entry.close();
      if (isDir)
        snprintf(buf, sizeof(buf), "  [DIR]  %s", fullPath);
      else
        snprintf(buf, sizeof(buf), "  %5lu B  %s", (unsigned long)size, fullPath);
      serialWritelnAll(buf);
    }
    dir.close();

  } else if (cmdIs(arg, "cat")) {
    const char* path = cmdArg(arg, "cat");
    File f = LittleFS.open(path, "r");
    if (!f) { serialWritelnAll("File not found."); return; }
    uint8_t buf[128];
    while (f.available()) {
      while (Serial.availableForWrite() < (int)sizeof(buf)) yield();
      int n = f.readBytes((char*)buf, sizeof(buf));
      Serial.write(buf, n);
    }
    f.close();
    Serial.println();

  } else if (cmdIs(arg, "rm")) {
    const char* path = cmdArg(arg, "rm");
    if (LittleFS.remove(path)) {
      serialWritelnAll("Deleted.");
      int plen = strlen(path);
      if (plen > 3 && !strcasecmp(path + plen - 3, ".ir")) {
        irScanFiles();
        char buf[48]; snprintf(buf, sizeof(buf), "Rescanned: %d device(s)", irFileCount);
        serialWritelnAll(buf);
      }
    } else {
      serialWritelnAll("Failed — not found or is a directory.");
    }

  } else if (cmdIs(arg, "mkdir")) {
    const char* path = cmdArg(arg, "mkdir");
    if (LittleFS.mkdir(path)) serialWritelnAll("Created.");
    else                      serialWritelnAll("Failed — already exists or invalid path.");

  } else if (cmdIs(arg, "mv")) {
    const char* rest = cmdArg(arg, "mv");
    char src[64] = {}, dst[64] = {};
    int si = 0;
    while (*rest && *rest != ' ' && si < 63) src[si++] = *rest++;
    while (*rest == ' ') rest++;
    int di = 0;
    while (*rest && di < 63) dst[di++] = *rest++;
    if (!src[0] || !dst[0]) { serialWritelnAll("Usage: fs mv <src> <dst>"); return; }
    if (LittleFS.rename(src, dst)) {
      serialWritelnAll("Moved.");
      irScanFiles();
    } else {
      serialWritelnAll("Failed.");
    }

  } else if (cmdIs(arg, "upload")) {
    const char* path = cmdArg(arg, "upload");
    if (!*path) { serialWritelnAll("Usage: fs upload <path>"); return; }
    // Create parent directory if needed
    char parent[64];
    strlcpy(parent, path, sizeof(parent));
    char* last = strrchr(parent, '/');
    if (last && last != parent) { *last = '\0'; LittleFS.mkdir(parent); }
    File f = LittleFS.open(path, "w");
    if (!f) { serialWritelnAll("Cannot create file — check path."); return; }
    strlcpy(fsUploadPath, path, sizeof(fsUploadPath));
    fsUploadFile    = f;
    fsUploadBytes   = 0;
    fsLineBufLen    = 0;
    fsLineOverflow  = false;
    fsUploadActive  = true;
    serialWritelnAll("Ready — paste file content, then send  ---END---  on its own line.");

  } else {
    serialWritelnAll("Unknown fs subcommand. Type 'fs' for usage.");
  }
}

void serialHandleWiFi(const char* arg) {
  if (!*arg) {
    serialWritelnAll("wifi scan|list|status|connect <N>|connect ssid <S> pass <P>|disconnect|debug on|off");
    return;
  }
  if (cmdIs(arg,"scan")) {
    WiFi.scanDelete();
    wifiScanCount = 0; wifiScanOffset = 0;
    WiFi.scanNetworks(true);
    wifiScanning = true;
    serialWritelnAll("WiFi scan started.");
    serialPrintFunctionHelp(FUNCTION_WIFI);
  } else if (cmdIs(arg,"list")) {
    if (wifiScanCount == 0) {
      serialWritelnAll("No scan results. Run 'wifi scan' first.");
      return;
    }
    char buf[80];
    for (int i = 0; i < wifiScanCount; i++) {
      snprintf(buf, sizeof(buf), "%2d: %-32s %4ddBm %s ch%d  %s",
        i, wifiScanLog[i].ssid, wifiScanLog[i].rssi,
        wifiScanLog[i].encType == 0 ? "OPEN  " : "SECURE",
        wifiScanLog[i].channel, wifiScanLog[i].bssid);
      serialWritelnAll(buf);
    }
  } else if (cmdIs(arg,"status")) {
    char buf[80];
    if (WiFi.isConnected()) {
      snprintf(buf, sizeof(buf), "Connected: %s  IP:%s  RSSI:%ddBm",
        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      snprintf(buf, sizeof(buf), "Not connected. (saved SSID: %s)", ssid);
    }
    serialWritelnAll(buf);
  } else if (cmdIs(arg,"disconnect")) {
    WiFi.disconnect(false);
    serialWritelnAll("WiFi disconnected.");
    serialPrintFunctionHelp(FUNCTION_WIFI);
  } else if (cmdIs(arg,"debug")) {
    const char* v = cmdArg(arg,"debug");
    if      (strcasecmp(v,"on")==0)  wifiDebugMode = true;
    else if (strcasecmp(v,"off")==0) wifiDebugMode = false;
    else { serialWritelnAll("Usage: wifi debug on|off"); return; }
    saveSettings();
    serialPrintFunctionHelp(FUNCTION_WIFI);
  } else if (cmdIs(arg,"connect")) {
    const char* rest = cmdArg(arg,"connect");
    if (cmdIs(rest,"ssid")) {
      // wifi connect ssid <SSID> pass <PASSWORD>
      const char* afterSsid = cmdArg(rest,"ssid");
      const char* passPtr   = strstr(afterSsid," pass ");
      if (!passPtr) { serialWritelnAll("Usage: wifi connect ssid <SSID> pass <PASSWORD>"); return; }
      int ssidLen = passPtr - afterSsid;
      if (ssidLen <= 0 || ssidLen > 32) { serialWritelnAll("Invalid SSID length."); return; }
      char targetSsid[33]; strncpy(targetSsid, afterSsid, ssidLen); targetSsid[ssidLen] = '\0';
      const char* pass = passPtr + 6;  // skip " pass "
      // Persist and connect
      strncpy(wifiUserSsid, targetSsid, 32); wifiUserSsid[32] = '\0';
      strncpy(wifiUserPass, pass, 64);       wifiUserPass[64] = '\0';
      wifiAuthFailed = false;
      saveSettings();
      WiFi.disconnect(false);
      WiFi.begin(wifiUserSsid, wifiUserPass);
      char msg[80]; snprintf(msg, sizeof(msg), "Connecting to '%s' — credentials saved.", wifiUserSsid);
      serialWritelnAll(msg);
    } else {
      // wifi connect <N>  — open network by index
      int idx = atoi(rest);
      if (idx < 0 || idx >= wifiScanCount) {
        serialWritelnAll("Invalid index. Run 'wifi list' to see available networks.");
        return;
      }
      if (wifiScanLog[idx].encType != 0) {
        serialWritelnAll("Network is secured. Use: wifi connect ssid <SSID> pass <PASSWORD>");
        return;
      }
      // Persist open network (empty password) and connect
      strncpy(wifiUserSsid, wifiScanLog[idx].ssid, 32); wifiUserSsid[32] = '\0';
      wifiUserPass[0] = '\0';
      wifiAuthFailed  = false;
      saveSettings();
      WiFi.disconnect(false);
      WiFi.begin(wifiUserSsid);
      char msg[64]; snprintf(msg, sizeof(msg), "Connecting to '%s' — credentials saved.", wifiUserSsid);
      serialWritelnAll(msg);
    }
  } else {
    serialWritelnAll("Unknown wifi subcommand. Type 'help'.");
  }
}

void serialHandleLora(const char* arg) {
  if (!*arg) {
    serialWritelnAll("lora list|reply on|off|dedup on|off|preset 0-3");
    return;
  }
  if (cmdIs(arg,"list")) {
    if (!loraLogCount) { serialWritelnAll("No packets received."); return; }
    char buf[80]; char ago[8];
    for (int i = 0; i < loraLogCount; i++) {
      fmtAgo(loraLog[i].ms, ago, sizeof(ago));
      snprintf(buf, sizeof(buf), "[%d] RSSI:%d SNR:%.1f %dB  %s ago",
        i,(int)loraLog[i].rssi,loraLog[i].snr,loraLog[i].size,ago);
      serialWritelnAll(buf);
      if (loraLog[i].text[0]) serialWritelnAll(loraLog[i].text);
    }
  } else if (cmdIs(arg,"send")) {
    const char* text = cmdArg(arg,"send");
    if (!text || !*text) { serialWritelnAll("Usage: lora send <message text>"); return; }
    loraSendText(text);
  } else if (cmdIs(arg,"reply")) {
    const char* v = cmdArg(arg,"reply");
    if      (strcasecmp(v,"on") ==0) loraAutoReply=true;
    else if (strcasecmp(v,"off")==0) loraAutoReply=false;
    else { serialWritelnAll("Usage: lora reply on|off"); return; }
    saveSettings();
    serialPrintFunctionHelp(FUNCTION_LORA);
  } else if (cmdIs(arg,"dedup")) {
    const char* v = cmdArg(arg,"dedup");
    if      (strcasecmp(v,"on") ==0) loraDedup=true;
    else if (strcasecmp(v,"off")==0) loraDedup=false;
    else { serialWritelnAll("Usage: lora dedup on|off"); return; }
    saveSettings();
    serialPrintFunctionHelp(FUNCTION_LORA);
  } else if (cmdIs(arg,"preset")) {
    int idx = atoi(cmdArg(arg,"preset"));
    if (idx < 0 || idx >= LORA_PRESET_COUNT) {
      serialWritelnAll("Usage: lora preset 0-3  (0=LONG_FAST 1=LONG_SLOW 2=MED_FAST 3=SHORT_FAST)");
      return;
    }
    loraPresetIdx = idx; applyLoraSettings();
    serialPrintFunctionHelp(FUNCTION_LORA);
  } else {
    serialWritelnAll("Unknown lora subcommand. Type 'help'.");
  }
}

void serialHandleMusic(const char* arg) {
  if (strcasecmp(arg,"on")==0) {
    const BuzzNote* mel = nullptr; int len = 0;
    if (currentFunction==FUNCTION_MEDIA) {
      switch(mediaSubScreen) {
        case 1: mel=VADER_MELODY;  len=VADER_MELODY_LEN;  break;
        case 2: mel=OBIWAN_MELODY; len=OBIWAN_MELODY_LEN; break;
      }
    } else { serialWritelnAll("Navigate to media screen first."); return; }
    startBuzzer(mel, len);
    serialPrintFunctionHelp((int)currentFunction);
  } else if (strcasecmp(arg,"off")==0) {
    stopBuzzer();
    serialPrintFunctionHelp((int)currentFunction);
  } else {
    serialWritelnAll("Usage: music on|off");
  }
}

// ── Dispatcher ────────────────────────────────────────────────────

void serialHandleCommand(const char* raw) {
  resetActivity();
  while (*raw == ' ' || *raw == '\r') raw++;
  if (!*raw) return;

  if      (cmdIs(raw,"help")||cmdIs(raw,"?")) serialPrintHelp();
  else if (cmdIs(raw,"status"))     serialPrintStatus();
  else if (cmdIs(raw,"next"))       { onKey1Short(); serialPrintFunctionHelp((int)currentFunction); }
  else if (cmdIs(raw,"prev"))       { onKey2Short(); serialPrintFunctionHelp((int)currentFunction); }
  else if (cmdIs(raw,"goto"))       serialGoto(cmdArg(raw,"goto"));
  else if (cmdIs(raw,"clock"))      serialPrintClock();
  else if (cmdIs(raw,"battery"))    serialPrintBattery();
  else if (cmdIs(raw,"controller")) serialPrintController();
  else if (cmdIs(raw,"send")) {
    int L=0, R=0;
    sscanf(cmdArg(raw,"send"), "%d %d", &L, &R);
    L = constrain(L,-255,255); R = constrain(R,-255,255);
    transmitCmd.leftMotor = L; transmitCmd.rightMotor = R;
    transmitRemoteCommand(L, R);
    char buf[40]; snprintf(buf,sizeof(buf),"Sent: L=%d R=%d",L,R);
    serialWritelnAll(buf);
  }
  else if (cmdIs(raw,"fs"))    serialHandleFS(cmdArg(raw,"fs"));
  else if (cmdIs(raw,"bt"))    serialHandleBT(cmdArg(raw,"bt"));
  else if (cmdIs(raw,"wifi"))  serialHandleWiFi(cmdArg(raw,"wifi"));
  else if (cmdIs(raw,"ir"))    serialHandleIR(cmdArg(raw,"ir"));
  else if (cmdIs(raw,"lora"))  serialHandleLora(cmdArg(raw,"lora"));
  else if (cmdIs(raw,"music")) serialHandleMusic(cmdArg(raw,"music"));
  else if (cmdIs(raw,"imu"))   serialHandleImu(cmdArg(raw,"imu"));
  else if (cmdIs(raw,"webfm")) {
    if (webFMRunning) {
      char buf[64];
      snprintf(buf, sizeof(buf), "[WebFM] http://%s/", WiFi.localIP().toString().c_str());
      serialWritelnAll(buf);
    } else {
      serialWritelnAll("[WebFM] not running (connect WiFi first)");
    }
  }
  else {
    char err[64]; snprintf(err,sizeof(err),"Unknown: '%s'  (type help)",raw);
    serialWritelnAll(err);
  }
}

// ── Input collector — call from loop() ───────────────────────────

void serialCheckInput() {
  // USB Serial
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (fsUploadActive) { fsUploadFeed(c); continue; }
    if (c == '\n' || c == '\r') {
      if (sSerialLen > 0) {
        sSerialBuf[sSerialLen] = '\0';
        serialHandleCommand(sSerialBuf);
        sSerialLen = 0;
      }
    } else if (sSerialLen < (int)sizeof(sSerialBuf)-1) {
      sSerialBuf[sSerialLen++] = c;
    }
  }
  // BLE UART (BLERxCB appends to sBleBuf)
  while (sBleLen > 0) {
    int nl = -1;
    for (int i = 0; i < sBleLen; i++)
      if (sBleBuf[i]=='\n' || sBleBuf[i]=='\r') { nl=i; break; }
    if (nl < 0) break;
    sBleBuf[nl] = '\0';
    if (nl > 0) serialHandleCommand(sBleBuf);
    memmove(sBleBuf, sBleBuf+nl+1, sBleLen-nl-1);
    sBleLen -= nl+1;
  }
}

void renderFunction() {
  if (lastFunction == (int)FUNCTION_LORA && currentFunction != FUNCTION_LORA && loraInitialized) {
    lora.standby();
    digitalWrite(LORA_LNA_ENABLE, LOW);
  }
  if (lastFunction == (int)FUNCTION_BT && currentFunction != FUNCTION_BT && btInitialized) {
    btStopScan();
    btLogCount = 0; btTotalSeen = 0; btScrollOffset = 0;  // clear stale list so SCAN button shows on return
  }
  if (lastFunction == (int)FUNCTION_WIFI && currentFunction != FUNCTION_WIFI && wifiScanning) {
    WiFi.scanDelete();
    wifiScanning  = false;
    wifiScanCount = 0;
    wifiScanOffset = 0;
  }
  if (lastFunction == (int)FUNCTION_MEDIA && currentFunction != FUNCTION_MEDIA)
    stopBuzzer();

  static int serialLastScreen = -1;
  if ((int)currentFunction != serialLastScreen) {
    serialPrintFunctionHelp((int)currentFunction);
    serialLastScreen = (int)currentFunction;
  }

  switch (currentFunction) {
    case FUNCTION_BT:
      if (lastFunction != (int)FUNCTION_BT) initBT();
      renderBT();
      statusSprite.pushSprite(0, SPRITE_Y);
      break;

    case FUNCTION_WIFI:
      if (lastFunction != (int)FUNCTION_WIFI) initWiFi();
      renderWiFi();
      statusSprite.pushSprite(0, SPRITE_Y);
      break;

    case FUNCTION_IR:
      if (lastFunction != (int)FUNCTION_IR) initIR();
      renderIR();
      statusSprite.pushSprite(0, SPRITE_Y);
      break;

    case FUNCTION_MAIN:
      if (lastFunction != (int)FUNCTION_MAIN) initMain();
      renderMain();
      break;

    case FUNCTION_MEDIA:
      initMedia();   // internally guards on sub-screen change
      renderMedia();
      break;

    case FUNCTION_BATTERY:
      if (lastFunction != (int)FUNCTION_BATTERY) initBattery();
      renderBattery();
      statusSprite.pushSprite(0, SPRITE_Y);
      break;

    case FUNCTION_CONTROLLER:
      if (lastFunction != (int)FUNCTION_CONTROLLER) {
        initController();
        initGamePad();
      }
      renderController();
      statusSprite.pushSprite(0, SPRITE_Y);

      if (millis() - previousMillis >= 100) {
        previousMillis = millis();
        readGamePad();
      }
      if (millis() - previousMillisButtons >= 300) {
        previousMillisButtons = millis();
        readGamePadButtons();
      }
      break;

    case FUNCTION_LORA:
      if (lastFunction != (int)FUNCTION_LORA) initLora();
      loraCheckPacket();
      renderLora();
      statusSprite.pushSprite(0, SPRITE_Y);
      break;
  }
  lastFunction = (int)currentFunction;
}


// ================================================================
// FUNCTION_MAIN — Clock  (no seconds, Font7 7-segment style)
// ================================================================

void initMain() {
  lastMinute = -1;  // force immediate redraw whenever this screen is entered
  display.setTextDatum(MC_DATUM);
  renderHeader();
}

void renderMain() {
  getLocalTime(&timeinfo, 0);  // 0ms timeout = non-blocking; avoids 5s stall if NTP is slow
  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;
    drawTime();
    drawDateDay();
    drawBatteryIcon();
    drawWiFiStatus();
    drawBTIcon();
  }
}

void drawTime() {
  bool isLandscape = display.width() > display.height();
  int  cx          = display.width() / 2;
  int  avail       = display.height() - SPRITE_Y;  // vertical space below header

  display.setTextDatum(MC_DATUM);
  display.setFont(&fonts::Font7);  // 7-segment style, ~75px tall at textSize=1
  display.setTextSize(1);
  display.setTextColor(TIME_COLOR, BG_COLOR);

  if (isLandscape) {
    // Single row "HH:MM" — Font7 chars are ~48px wide; "HH:MM" ≈ 200px, fits in 240
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    int timeY = SPRITE_Y + avail / 2 - 8;  // slight upward bias to leave room for date
    display.fillRect(0, SPRITE_Y, display.width(), avail - 18, BG_COLOR);
    display.drawString(timeStr, cx, timeY);
  } else {
    // Portrait: HH and MM stacked close together, block centered in available area.
    // Font7 at size 1 renders digits ~75px tall; keep a tight gap between them.
    const int digitH = 75;
    const int gap    = 5;   // px between bottom of HH and top of MM
    const int dateH  = 18;  // reserved at bottom for date line
    int usable   = avail - dateH;
    int blockH   = digitH + gap + digitH;
    int blockTop = SPRITE_Y + (usable - blockH) / 2;
    int topY     = blockTop + digitH / 2;
    int botY     = blockTop + digitH + gap + digitH / 2;

    char hourStr[3], minStr[3];
    sprintf(hourStr, "%02d", timeinfo.tm_hour);
    sprintf(minStr,  "%02d", timeinfo.tm_min);

    display.fillRect(0, SPRITE_Y, display.width(), avail, BG_COLOR);
    display.drawString(hourStr, cx, topY);
    display.drawString(minStr,  cx, botY);
  }

  // Reset to default font
  display.setFont(&fonts::Font0);
}

void drawDateDay() {
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* fullDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                             "Thursday", "Friday", "Saturday"};

  display.setTextDatum(MC_DATUM);
  display.setFont(&fonts::Font0);
  display.setTextSize(1);
  display.setTextColor(DATE_COLOR, BG_COLOR);

  if (display.width() > display.height()) {
    // Landscape: full date at bottom
    char dateStr[40];
    sprintf(dateStr, "%s  %s %d, %d",
            fullDays[timeinfo.tm_wday],
            months[timeinfo.tm_mon],
            timeinfo.tm_mday,
            timeinfo.tm_year + 1900);
    int y = display.height() - 10;
    display.fillRect(0, y - 10, display.width(), 20, BG_COLOR);
    display.drawString(dateStr, display.width() / 2, y);
  } else {
    // Portrait: compact "Mon Apr 3" in the 20px reserved at bottom
    char dateStr[20];
    sprintf(dateStr, "%s %s %d",
            days[timeinfo.tm_wday],
            months[timeinfo.tm_mon],
            timeinfo.tm_mday);
    int y = display.height() - 10;
    display.fillRect(0, y - 10, display.width(), 20, BG_COLOR);
    display.drawString(dateStr, display.width() / 2, y);
  }
}


// ================================================================
// FUNCTION_BATTERY
// ================================================================

void initBattery() {
  progressPos       = 0;
  progressExpanding = true;
  display.fillScreen(COLOR_BLACK);
  renderHeader();
}

void renderBattery() {
  if (navState == NAV_SETTINGS) { renderBatterySettings(); return; }
  if (navState == NAV_RESET)    { renderBatteryReset();    return; }
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // Animated sweep bar (top 4px of sprite)
  statusSprite.setColor(COLOR_ORANGE);
  if (progressExpanding) {
    statusSprite.fillRect(0, 0, progressPos, 4);
  } else {
    statusSprite.fillRect(progressPos, 0, sw - progressPos, 4);
  }
  if (++progressPos >= sw) {
    progressPos       = 0;
    progressExpanding = !progressExpanding;
  }

  // Voltage color
  uint16_t voltColor = (batteryVoltage > 3.7f)   ? COLOR_GREEN  :
                       (batteryVoltage >= 3.3f)   ? COLOR_ORANGE :
                                                    COLOR_RED;
  // Bar fill color
  uint16_t barColor  = (voltagePercent > 50)  ? COLOR_GREEN  :
                       (voltagePercent > 20)  ? COLOR_ORANGE :
                                               COLOR_RED;

  // Charge status string
  const char* statusStr;
  uint16_t    statusColor;
  switch (chargeStatus) {
    case NessoBattery::CHARGING:
      statusStr = "CHARGING"; statusColor = COLOR_GREEN;  break;
    case NessoBattery::FULL_CHARGE:
      statusStr = "FULL";     statusColor = COLOR_GREEN;  break;
    case NessoBattery::NOT_CHARGING:
      statusStr = "IDLE";     statusColor = COLOR_GRAY;   break;
    default:
      statusStr = "PRE-CHG";  statusColor = COLOR_ORANGE; break;
  }

  if (isLandscape) {
    // ── Landscape layout (e.g. 240 × 113 sprite) ──────────────────
    // Row 1: label (left) + voltage (right)
    statusSprite.setTextSize(2);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.drawString("BATTERY", 8, 10);

    char vStr[10];
    sprintf(vStr, "%4.2fV", batteryVoltage);
    statusSprite.setTextColor(voltColor);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(vStr, sw - 8, 10);

    // Battery bar graphic
    int barX = 8, barY = 38, barW = sw - 32, barH = 22;
    int fillW = max(0, (int)((float)(barW - 4) * voltagePercent / 100.0f));
    statusSprite.drawRect(barX, barY, barW, barH, COLOR_GRAY);
    if (fillW > 0)
      statusSprite.fillRect(barX + 2, barY + 2, fillW, barH - 4, barColor);
    // Terminal cap
    statusSprite.fillRect(barX + barW, barY + 6, 10, barH - 12, COLOR_GRAY);

    // Row 3: percentage (left) + charge status (right)
    char pStr[6];
    sprintf(pStr, "%d%%", (int)voltagePercent);
    statusSprite.setTextSize(3);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.drawString(pStr, 8, 68);

    statusSprite.setTextSize(2);
    statusSprite.setTextColor(statusColor);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(statusStr, sw - 8, 75);

    // Row 4: uptime
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.drawString("UP", 8, 100);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(uptimeString, sw - 8, 100);

    // Debug: raw library chargeLevel (tiny, bottom-right)
    char clStr[12];
    sprintf(clStr, "lib:%d%%", (int)chargeLevel);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.setTextDatum(BR_DATUM);
    statusSprite.drawString(clStr, sw - 2, sh - 2);

  } else {
    // ── Portrait layout (e.g. 135 × 218 sprite) ───────────────────
    // Row 1: label centred
    statusSprite.setTextSize(2);
    statusSprite.setTextDatum(TC_DATUM);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.drawString("BATTERY", sw / 2, 10);

    // Row 2: large voltage centred
    char vStr[10];
    sprintf(vStr, "%4.2fV", batteryVoltage);
    statusSprite.setTextSize(3);
    statusSprite.setTextColor(voltColor);
    statusSprite.drawString(vStr, sw / 2, 36);

    // Battery bar
    int barX = 8, barY = 82, barW = sw - 28, barH = 22;
    int fillW = max(0, (int)((float)(barW - 4) * voltagePercent / 100.0f));
    statusSprite.drawRect(barX, barY, barW, barH, COLOR_GRAY);
    if (fillW > 0)
      statusSprite.fillRect(barX + 2, barY + 2, fillW, barH - 4, barColor);
    statusSprite.fillRect(barX + barW, barY + 6, 10, barH - 12, COLOR_GRAY);

    // Percentage + status
    char pStr[6];
    sprintf(pStr, "%d%%", (int)voltagePercent);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.drawString(pStr, 8, 114);

    statusSprite.setTextColor(statusColor);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(statusStr, sw - 8, 114);

    // Uptime
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.drawString("UP", 8, 148);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(uptimeString, sw - 8, 148);

    // Debug: raw library chargeLevel (tiny, bottom-right)
    char clStr[12];
    sprintf(clStr, "lib:%d%%", (int)chargeLevel);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.setTextDatum(BR_DATUM);
    statusSprite.drawString(clStr, sw - 2, sh - 2);
  }
}


// ================================================================
// FUNCTION_MATRIX — Matrix rain
// ================================================================

void startBuzzer(const BuzzNote* melody, int len) {
  buzzerMelody    = melody;
  buzzerMelodyLen = len;
  buzzerNoteIdx   = 0;
  buzzerPlaying   = true;
  uint16_t f = melody[0].freq;
  if (f) tone(BEEP_PIN, f); else noTone(BEEP_PIN);
  buzzerNoteEndMs = millis() + melody[0].ms;
}

void stopBuzzer() {
  buzzerPlaying = false;
  noTone(BEEP_PIN);
}

// One-shot UI click feedback — skipped if a melody is playing so music isn't interrupted.
// freq: Hz; durationMs: length in ms. Keep durations short (≤40 ms).
void uiClick(uint16_t freq, uint16_t durationMs) {
  if (!uiClickEnabled || buzzerPlaying) return;
  tone(BEEP_PIN, freq, durationMs);
}

void updateBuzzer(unsigned long msNow) {
  if (!buzzerPlaying || !buzzerMelody) return;
  if (msNow < buzzerNoteEndMs) return;
  buzzerNoteIdx = (buzzerNoteIdx + 1) % buzzerMelodyLen;
  uint16_t f = buzzerMelody[buzzerNoteIdx].freq;
  if (f) tone(BEEP_PIN, f); else noTone(BEEP_PIN);
  buzzerNoteEndMs = msNow + buzzerMelody[buzzerNoteIdx].ms;
}

void initMatrix() {
  display.fillScreen(COLOR_BLACK);
  randomSeed(analogRead(0));

  for (int i = 0; i < NUM_COLUMNS; i++) {
    dropPositions[i] = random(-display.height(), 0);
    dropSpeeds[i]    = random(1, 4);
    dropLengths[i]   = random(5, 15);
  }
}

void renderMatrix() {
  int dispW = display.width();
  int dispH = display.height();
  int colW  = dispW / NUM_COLUMNS;

  for (int i = 0; i < NUM_COLUMNS; i++) {
    display.fillRect(i * colW, 0, colW, dispH, display.color565(0, 2, 0));
  }

  for (int col = 0; col < NUM_COLUMNS; col++) {
    int x = col * colW;

    for (int i = 0; i < dropLengths[col]; i++) {
      int y = dropPositions[col] - (i * CHAR_HEIGHT);
      if (y >= 0 && y < dispH) {
        int brightness = map(i, 0, dropLengths[col], 255, 50);
        display.setCursor(x + 1, y);
        display.setTextColor(display.color565(0, brightness, 0));
        display.setTextSize(2);
        display.print((char)random(33, 126));
      }
    }

    if (dropPositions[col] >= 0 && dropPositions[col] < dispH) {
      display.setCursor(x + 1, dropPositions[col]);
      display.setTextColor(COLOR_WHITE);
      display.setTextSize(2);
      display.print((char)random(33, 126));
    }

    dropPositions[col] += dropSpeeds[col];
    if (dropPositions[col] - (dropLengths[col] * CHAR_HEIGHT) > dispH) {
      dropPositions[col] = random(-100, -20);
      dropSpeeds[col]    = random(1, 4);
      dropLengths[col]   = random(5, 15);
    }
  }

  // Music status hint (bottom-left, semi-transparent style via dark bg)
  display.setTextSize(1);
  display.setFont(&fonts::Font0);
  if (buzzerPlaying) {
    display.setTextColor(display.color565(0, 180, 0));
    display.drawString("[ MUSIC ON  ]", 4, dispH - 12);
  } else {
    display.setTextColor(display.color565(0, 60, 0));
    display.drawString("[ TAP: MUSIC]", 4, dispH - 12);
  }

  delay(30);
}


// ================================================================
// Braille dot-art renderer (shared by Vader + Obi-Wan)
// Each Braille char (U+2800–U+28FF) encodes an 8-dot 2×4 cell.
// ================================================================

// Advance one UTF-8 code point from *pp; returns it and advances the pointer.
static uint32_t nextUtf8cp(const uint8_t*& pp) {
  uint8_t b0 = *pp;
  if (!b0) return 0;
  if (b0 < 0x80)                         { pp += 1; return b0; }
  if ((b0 & 0xE0) == 0xC0 && pp[1])     { uint32_t c = ((b0&0x1F)<<6)|(pp[1]&0x3F); pp+=2; return c; }
  if ((b0 & 0xF0) == 0xE0 && pp[1] && pp[2]) {
    uint32_t c = ((b0&0x0F)<<12)|((pp[1]&0x3F)<<6)|(pp[2]&0x3F); pp+=3; return c;
  }
  pp++; return 0xFFFD;
}

// Render braille art array centred horizontally on the display.
// dotW/dotH: pixel size of each individual dot (1 or 2).
static void renderBrailleArt(const char* const* art, int lines,
                              uint16_t color, int startY,
                              int dotW, int dotH) {
  int dispW = display.width();
  int charW = dotW * 2;   // 2 dot-columns per braille char
  int charH = dotH * 4;   // 4 dot-rows    per braille char
  int artPxW = lines > 0 ? 0 : 0;

  // Measure max line width for centring
  int maxCols = 0;
  for (int r = 0; r < lines; r++) {
    const uint8_t* p = (const uint8_t*)art[r];
    int cols = 0;
    while (*p) { nextUtf8cp(p); cols++; }
    if (cols > maxCols) maxCols = cols;
  }
  int startX = max(0, (dispW - maxCols * charW) / 2);

  for (int row = 0; row < lines; row++) {
    const uint8_t* p = (const uint8_t*)art[row];
    int col = 0;
    while (*p) {
      uint32_t cp = nextUtf8cp(p);
      if (cp >= 0x2800 && cp <= 0x28FF) {
        uint8_t d  = (uint8_t)(cp - 0x2800);
        int bx = startX + col * charW;
        int by = startY + row * charH;
        if (d & 0x01) display.fillRect(bx,       by,           dotW, dotH, color);
        if (d & 0x02) display.fillRect(bx,       by + dotH,    dotW, dotH, color);
        if (d & 0x04) display.fillRect(bx,       by + dotH*2,  dotW, dotH, color);
        if (d & 0x08) display.fillRect(bx + dotW, by,           dotW, dotH, color);
        if (d & 0x10) display.fillRect(bx + dotW, by + dotH,    dotW, dotH, color);
        if (d & 0x20) display.fillRect(bx + dotW, by + dotH*2,  dotW, dotH, color);
        if (d & 0x40) display.fillRect(bx,       by + dotH*3,  dotW, dotH, color);
        if (d & 0x80) display.fillRect(bx + dotW, by + dotH*3,  dotW, dotH, color);
      }
      col++;
    }
  }
}

// ================================================================
// FUNCTION_VADER — Darth Vader + Imperial March
// ================================================================

bool vaderNeedsRedraw = true;

void initVader() {
  display.fillScreen(COLOR_BLACK);
  vaderNeedsRedraw = true;
}

void renderVader() {
  static bool lastBuzzer = false;
  if (!vaderNeedsRedraw && buzzerPlaying == lastBuzzer) { delay(30); return; }
  vaderNeedsRedraw = false;
  lastBuzzer = buzzerPlaying;

  int dispW = display.width();
  int dispH = display.height();
  display.fillScreen(COLOR_BLACK);

  // Title
  display.setFont(&fonts::Font0);
  display.setTextSize(2);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(display.color565(180, 0, 0));
  display.drawString("DARTH VADER", dispW / 2, 4);

  // Braille art: dotW=2, artH = 11 lines * 8px = 88px; starts at y=24
  renderBrailleArt(VADER_BRAILLE, VADER_BRAILLE_LINES,
                   display.color565(220, 60, 60), 24,
                   VADER_BRAILLE_DOT_W, VADER_BRAILLE_DOT_H);

  // Music hint at bottom
  display.setFont(&fonts::Font0);
  display.setTextSize(1);
  display.setTextDatum(TC_DATUM);
  if (buzzerPlaying) {
    display.setTextColor(display.color565(200, 30, 30));
    display.drawString("[ MUSIC ON  ]", dispW / 2, dispH - 12);
  } else {
    display.setTextColor(display.color565(60, 0, 0));
    display.drawString("[ TAP: MUSIC]", dispW / 2, dispH - 12);
  }
}


// ================================================================
// FUNCTION_OBIWAN — Obi-Wan Kenobi + Star Wars Theme
// ================================================================

bool obiwanNeedsRedraw = true;

void initObiwan() {
  display.fillScreen(COLOR_BLACK);
  obiwanNeedsRedraw = true;
}

void renderObiwan() {
  static bool lastBuzzer = false;
  if (!obiwanNeedsRedraw && buzzerPlaying == lastBuzzer) { delay(30); return; }
  obiwanNeedsRedraw = false;
  lastBuzzer = buzzerPlaying;

  int dispW = display.width();
  int dispH = display.height();
  display.fillScreen(COLOR_BLACK);

  // Title
  display.setFont(&fonts::Font0);
  display.setTextSize(2);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(display.color565(0, 120, 200));
  display.drawString("OBI-WAN KENOBI", dispW / 2, 4);

  // Braille art: dotW=1, artH = 28 lines * 4px = 112px; starts at y=22
  renderBrailleArt(OBIWAN_BRAILLE, OBIWAN_BRAILLE_LINES,
                   display.color565(0, 180, 255), 22,
                   OBIWAN_BRAILLE_DOT_W, OBIWAN_BRAILLE_DOT_H);

  // Music hint at bottom
  display.setFont(&fonts::Font0);
  display.setTextSize(1);
  display.setTextDatum(TC_DATUM);
  if (buzzerPlaying) {
    display.setTextColor(display.color565(0, 160, 230));
    display.drawString("[ MUSIC ON  ]", dispW / 2, dispH - 12);
  } else {
    display.setTextColor(display.color565(0, 40, 70));
    display.drawString("[ TAP: MUSIC]", dispW / 2, dispH - 12);
  }
}


// ================================================================
// FUNCTION_CONTROLLER — Gamepad
// ================================================================

void initController() {
  display.fillScreen(COLOR_BLACK);
  renderHeader();
}

// Draw a labelled button circle on the sprite. pressed = active LOW bit cleared.
static void drawGamepadBtn(int x, int y, const char* label, bool pressed) {
  uint16_t bg  = pressed ? COLOR_ORANGE                    : statusSprite.color565(30, 30, 30);
  uint16_t rim = pressed ? COLOR_WHITE                     : COLOR_GRAY;
  uint16_t fg  = pressed ? COLOR_BLACK                     : COLOR_GRAY;
  statusSprite.fillCircle(x, y, 9, bg);
  statusSprite.drawCircle(x, y, 9, rim);
  statusSprite.setTextDatum(MC_DATUM);
  statusSprite.setTextSize(1);
  statusSprite.setTextColor(fg);
  statusSprite.drawString(label, x, y);
}

void renderController() {
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // Joystick visualisation circle
  const int VIZ_R = 36;
  int jcx, jcy;
  if (isLandscape) {
    jcx = VIZ_R + 14;
    jcy = sh / 2;
  } else {
    jcx = sw / 2;
    jcy = VIZ_R + 18;
  }

  // Background disc + border + crosshairs
  statusSprite.fillCircle(jcx, jcy, VIZ_R, display.color565(18, 18, 18));
  statusSprite.drawCircle(jcx, jcy, VIZ_R,     COLOR_TEAL);
  statusSprite.drawCircle(jcx, jcy, VIZ_R - 1, display.color565(0, 30, 30));
  statusSprite.drawFastHLine(jcx - VIZ_R + 5, jcy,
                              (VIZ_R - 5) * 2, display.color565(35, 35, 35));
  statusSprite.drawFastVLine(jcx, jcy - VIZ_R + 5,
                              (VIZ_R - 5) * 2, display.color565(35, 35, 35));

  // Joystick dot — joyDisplayX = forward/backward, joyDisplayY = rotation
  // Map: forward = up on viz, rotate-right = right on viz
  int dotX = jcx + (int16_t)(joyDisplayY * (VIZ_R - 8) / 255);
  int dotY = jcy - (int16_t)(joyDisplayX * (VIZ_R - 8) / 255);

  // Constrain dot inside circle
  float dx = dotX - jcx, dy = dotY - jcy;
  float dist = sqrtf(dx * dx + dy * dy);
  float maxR = VIZ_R - 8;
  if (dist > maxR) {
    dotX = jcx + (int)(dx * maxR / dist);
    dotY = jcy + (int)(dy * maxR / dist);
  }
  statusSprite.fillCircle(dotX, dotY, 6, COLOR_ORANGE);

  // Action label
  const int dead = 15;
  const char* actionStr =
    (joyDisplayX >  dead) ? "FORWARD"    :
    (joyDisplayX < -dead) ? "REVERSE"    :
    (joyDisplayY >  dead) ? "ROTATE CW"  :
    (joyDisplayY < -dead) ? "ROTATE CCW" :
                            "STOP";

  uint16_t connColor = controllerConnected ? COLOR_GREEN : COLOR_RED;

  if (isLandscape) {
    // Info panel to the right of the joystick circle
    int ix = jcx + VIZ_R + 14;

    statusSprite.setTextDatum(TL_DATUM);

    statusSprite.setTextSize(1);
    statusSprite.setTextColor(connColor);
    statusSprite.drawString(controllerConnected ? "CONNECTED" : "NO GAMEPAD", ix, 8);

    // Divider
    statusSprite.drawFastHLine(ix, 22, sw - ix - 4, COLOR_GRAY);

    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString(actionStr, ix, 28);

    char motorStr[20];
    sprintf(motorStr, "L %4d", transmitCmd.leftMotor);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.drawString(motorStr, ix, 60);

    sprintf(motorStr, "R %4d", transmitCmd.rightMotor);
    statusSprite.drawString(motorStr, ix, 74);

    // Face buttons: SEL / STA on left side, Y/X/A/B diamond on right
    bool bSel = !(gamepadButtons & (1UL << BUTTON_SELECT));
    bool bSta = !(gamepadButtons & (1UL << BUTTON_START));
    bool bY   = !(gamepadButtons & (1UL << BUTTON_Y));
    bool bX   = !(gamepadButtons & (1UL << BUTTON_X));
    bool bA   = !(gamepadButtons & (1UL << BUTTON_A));
    bool bB   = !(gamepadButtons & (1UL << BUTTON_B));

    drawGamepadBtn(ix + 10, sh - 14, "SEL", bSel);
    drawGamepadBtn(ix + 35, sh - 14, "STA", bSta);

    // Diamond: Y=top, X=left, A=right, B=bottom
    int dx = sw - 42, dy = sh - 14;
    drawGamepadBtn(dx,      dy - 22, "Y", bY);
    drawGamepadBtn(dx - 22, dy,      "X", bX);
    drawGamepadBtn(dx + 22, dy,      "A", bA);
    drawGamepadBtn(dx,      dy,      "B", bB);

  } else {
    // Info panel below the joystick circle (portrait)
    int iy = jcy + VIZ_R + 14;

    statusSprite.drawFastHLine(8, iy, sw - 16, COLOR_GRAY);
    iy += 6;

    statusSprite.setTextDatum(TC_DATUM);

    statusSprite.setTextSize(1);
    statusSprite.setTextColor(connColor);
    statusSprite.drawString(controllerConnected ? "CONNECTED" : "NO GAMEPAD", sw / 2, iy);

    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString(actionStr, sw / 2, iy + 14);

    char motorStr[12];
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.setTextDatum(TL_DATUM);
    sprintf(motorStr, "L %4d", transmitCmd.leftMotor);
    statusSprite.drawString(motorStr, 10, iy + 38);
    statusSprite.setTextDatum(TR_DATUM);
    sprintf(motorStr, "R %4d", transmitCmd.rightMotor);
    statusSprite.drawString(motorStr, sw - 10, iy + 38);

    // Face buttons: SEL / STA row, then Y/X/A/B diamond
    bool bSel = !(gamepadButtons & (1UL << BUTTON_SELECT));
    bool bSta = !(gamepadButtons & (1UL << BUTTON_START));
    bool bY   = !(gamepadButtons & (1UL << BUTTON_Y));
    bool bX   = !(gamepadButtons & (1UL << BUTTON_X));
    bool bA   = !(gamepadButtons & (1UL << BUTTON_A));
    bool bB   = !(gamepadButtons & (1UL << BUTTON_B));

    int btnY0 = iy + 60;   // SEL/STA row
    drawGamepadBtn(sw / 2 - 18, btnY0, "SEL", bSel);
    drawGamepadBtn(sw / 2 + 18, btnY0, "STA", bSta);

    // Diamond: Y=top, X=left, A=right, B=bottom
    int dx = sw / 2, dy = btnY0 + 38;
    drawGamepadBtn(dx,      dy - 22, "Y", bY);
    drawGamepadBtn(dx - 22, dy,      "X", bX);
    drawGamepadBtn(dx + 22, dy,      "A", bA);
    drawGamepadBtn(dx,      dy,      "B", bB);
  }
}

void initGamePad() {
  controllerConnected = false;
  if (!joystickAvailable) return;  // not detected at boot, skip init

  // seesaw was already probed successfully at boot; re-init to configure pins
  ss.begin(0x50);
  debugln("seesaw started");
  debugln(ss.getI2CAddr());

  uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
  if (version != 5743) {
    debug("Wrong firmware loaded? ");
    debugln(version);
    return;
  }
  debugln("Found Product 5743");
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);
  controllerConnected = true;
}

void readGamePad() {
  float x = 0, y = 0;
  for (int s = 0; s < 4; s++) {
    x += 1023 - ss.analogRead(JOY1_X);
    y += 1023 - ss.analogRead(JOY1_Y);
    delay(10);
  }
  x /= 4.0;
  y /= 4.0;

  debug("x: "); debug(x); debug(", "); debug("y: "); debugln(y);

  if (!calibrationComplete) {
    zero_x              = x;
    zero_y              = y;
    calibrationComplete = true;
    debugln("Calibration complete!");
  }

  int16_t powerx = x - zero_x;
  int16_t powery = y - zero_y;

  // Store raw axis values for the joystick visualisation
  joyDisplayX = powerx;
  joyDisplayY = powery;

  if (powerx < 0) {
    int16_t p = map(powerx, 0, -515, 0, -255);
    transmitRemoteCommand(p, p);
  } else if (powerx > 0) {
    int16_t p = map(powerx, 0, 515, 0, 255);
    transmitRemoteCommand(p, p);
  } else if (powery < 0) {
    int16_t p = map(powery, 0, -515, 0, -255);
    transmitRemoteCommand(p, -p);
  } else if (powery > 0) {
    int16_t p = map(powery, 0, 515, 0, 255);
    transmitRemoteCommand(p, -p);
  } else {
    transmitRemoteCommand(0, 0);
  }
}

void readGamePadButtons() {
  uint32_t buttons = ss.digitalReadBulk(button_mask);
  gamepadButtons = buttons;  // snapshot for renderController()

  if (!(buttons & (1UL << BUTTON_A))) debugln("Button A pressed");
  if (!(buttons & (1UL << BUTTON_B))) debugln("Button B pressed");
  if (!(buttons & (1UL << BUTTON_Y))) debugln("Button Y pressed");
  if (!(buttons & (1UL << BUTTON_X))) debugln("Button X pressed");
  if (!(buttons & (1UL << BUTTON_SELECT))) {
    debugln("Button SELECT pressed");
    if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  }
  if (!(buttons & (1UL << BUTTON_START))) {
    debugln("Button START pressed");
    if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  }
}


// ================================================================
// Web File Manager
// ================================================================

#include "web_fm_html.h"

static WebServer webFM(80);

static void webFMJson(int code, const char* json) {
  webFM.send(code, "application/json", json);
}

static void webFMHandleRoot() {
  webFM.sendHeader("Cache-Control", "no-cache");
  webFM.send_P(200, "text/html; charset=utf-8", WEB_FM_HTML);
}

static void webFMHandleLs() {
  String path = webFM.arg("path");
  if (path.isEmpty()) path = "/";
  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory()) {
    dir.close();
    webFMJson(404, "{\"error\":\"Not a directory\"}");
    return;
  }
  String json = "{\"path\":\"";
  for (int i = 0; i < (int)path.length(); i++) {
    char c = path[i];
    if (c == '"' || c == '\\') json += '\\';
    json += c;
  }
  json += "\",\"entries\":[";
  bool first = true;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    char bare[64];
    strlcpy(bare, entry.name(), sizeof(bare));
    bool isD = entry.isDirectory();
    size_t fsz = isD ? 0 : entry.size();
    entry.close();
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"";
    for (char* p = bare; *p; p++) {
      if (*p == '"' || *p == '\\') json += '\\';
      json += *p;
    }
    json += "\",\"isDir\":";
    json += isD ? "true" : "false";
    if (!isD) { json += ",\"size\":"; json += fsz; }
    json += "}";
  }
  dir.close();
  json += "]}";
  webFMJson(200, json.c_str());
}

static void webFMHandleDl() {
  String path = webFM.arg("path");
  if (path.isEmpty()) { webFM.send(400, "text/plain", "Missing path"); return; }
  File f = LittleFS.open(path, "r");
  if (!f) { webFM.send(404, "text/plain", "Not found"); return; }
  String fname = path;
  int sl = fname.lastIndexOf('/');
  if (sl >= 0) fname = fname.substring(sl + 1);
  webFM.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
  webFM.streamFile(f, "application/octet-stream");
  f.close();
}

static void webFMHandleRm() {
  String path = webFM.arg("path");
  if (path.isEmpty()) { webFMJson(400, "{\"error\":\"Missing path\"}"); return; }
  File f = LittleFS.open(path);
  if (!f) { webFMJson(404, "{\"error\":\"Not found\"}"); return; }
  bool isD = f.isDirectory();
  f.close();
  bool ok = isD ? LittleFS.rmdir(path) : LittleFS.remove(path);
  if (ok) {
    if (path.endsWith(".ir") || path.endsWith(".IR") || isD) irScanFiles();
    webFMJson(200, "{\"ok\":true}");
  } else {
    webFMJson(500, "{\"error\":\"Delete failed (dir must be empty)\"}");
  }
}

static void webFMHandleMkdir() {
  String path = webFM.arg("path");
  if (path.isEmpty()) { webFMJson(400, "{\"error\":\"Missing path\"}"); return; }
  if (LittleFS.mkdir(path)) { webFMJson(200, "{\"ok\":true}"); }
  else { webFMJson(500, "{\"error\":\"mkdir failed\"}"); }
}

static void webFMHandleMv() {
  String from = webFM.arg("from"), to = webFM.arg("to");
  if (from.isEmpty() || to.isEmpty()) { webFMJson(400, "{\"error\":\"Missing from/to\"}"); return; }
  if (LittleFS.rename(from, to)) {
    if (from.endsWith(".ir") || to.endsWith(".ir") ||
        from.endsWith(".IR") || to.endsWith(".IR")) irScanFiles();
    webFMJson(200, "{\"ok\":true}");
  } else {
    webFMJson(500, "{\"error\":\"Rename failed\"}");
  }
}

static void webFMUploadHandler() {
  HTTPUpload& up = webFM.upload();
  if (up.status == UPLOAD_FILE_START) {
    webFMUploadPath = webFM.arg("path");
    webFMUploadOk   = false;
    webFMUploadFile = LittleFS.open(webFMUploadPath, "w");
    webFMUploadOk   = (bool)webFMUploadFile;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (webFMUploadFile) webFMUploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (webFMUploadFile) webFMUploadFile.close();
    if (webFMUploadOk &&
        (webFMUploadPath.endsWith(".ir") || webFMUploadPath.endsWith(".IR")))
      irScanFiles();
  }
}

void webFMStart() {
  if (webFMRunning) return;
  webFM.on("/",           HTTP_GET,  webFMHandleRoot);
  webFM.on("/api/ls",     HTTP_GET,  webFMHandleLs);
  webFM.on("/api/dl",     HTTP_GET,  webFMHandleDl);
  webFM.on("/api/rm",     HTTP_POST, webFMHandleRm);
  webFM.on("/api/mkdir",  HTTP_POST, webFMHandleMkdir);
  webFM.on("/api/mv",     HTTP_POST, webFMHandleMv);
  webFM.on("/api/upload", HTTP_POST,
    []() { webFMJson(webFMUploadOk ? 200 : 500,
                     webFMUploadOk ? "{\"ok\":true}" : "{\"error\":\"Open failed\"}"); },
    webFMUploadHandler);
  webFM.begin();
  webFMRunning = true;
  char buf[64];
  snprintf(buf, sizeof(buf), "[WebFM] http://%s/", WiFi.localIP().toString().c_str());
  serialWritelnAll(buf);
}

void webFMStop() {
  if (!webFMRunning) return;
  webFM.stop();
  webFMRunning = false;
}

void webFMHandle() {
  if (webFMRunning) webFM.handleClient();
}


// ================================================================
// WiFi & Networking
// ================================================================

void connectToWiFi() {
  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  // Prefer user-persisted credentials; fall back to compiled-in secrets
  const char* targetSsid = (wifiUserSsid[0] != '\0') ? wifiUserSsid : secretSsid;
  const char* targetPass = (wifiUserSsid[0] != '\0') ? wifiUserPass : secretPass;
  WiFi.begin(targetSsid, targetPass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    debug(".");
    attempts++;
  }
}

// WARNING: Called from a separate FreeRTOS task
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      debug("WiFi connected! IP address: ");
      debugln(WiFi.localIP());
      wifiAuthFailed = false;
      udp.begin(udpPort);
      webFMPendingStart = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      uint8_t reason = info.wifi_sta_disconnected.reason;
      if (reason == WIFI_REASON_AUTH_FAIL   ||
          reason == WIFI_REASON_AUTH_EXPIRE ||
          reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
        wifiAuthFailed = true;
        const char* activeSsid = (wifiUserSsid[0] != '\0') ? wifiUserSsid : secretSsid;
        char msg[80];
        snprintf(msg, sizeof(msg),
          "[WIFI] Wrong password for '%s'.", activeSsid);
        serialWritelnAll(msg);
        serialWritelnAll("  Update: wifi connect ssid <SSID> pass <PASSWORD>");
      } else {
        debugln("WiFi lost connection");
      }
      break;
    }
    default:
      break;
  }
}

void transmitRemoteCommand(int leftMotorPower, int rightMotorPower) {
  transmitCmd.leftMotor  = constrain(leftMotorPower,  -255, 255);
  transmitCmd.rightMotor = constrain(rightMotorPower, -255, 255);

  udp.beginPacket(targetIpAddress, udpPort);
  udp.write((uint8_t*)&transmitCmd, sizeof(transmitCmd));
  udp.endPacket();

  debug("L: "); debug(transmitCmd.leftMotor);
  debug(" | R: "); debugln(transmitCmd.rightMotor);
}


// ================================================================
// FUNCTION_LORA — LoRa packet scanner
// ================================================================

void initLora() {
  display.fillScreen(COLOR_BLACK);
  renderHeader();

  loraListening = false;   // user must tap LISTEN; no auto-resume on entry
  if (loraInitialized) {
    // Hardware already up — re-arm SPI (LovyanGFX reconfigures the shared bus between renders)
    // and re-enable LNA that was disabled when leaving this screen.
    SPI.begin(SCK, MISO, MOSI, LORA_CS);
    pinMode(LORA_LNA_ENABLE, OUTPUT);
    digitalWrite(LORA_LNA_ENABLE, HIGH);
    return;
  }
  if (loraInitFailed) return;

  // Power up LoRa module via expander
  pinMode(LORA_ENABLE, OUTPUT);
  digitalWrite(LORA_ENABLE, HIGH);
  delay(20);

  // SPI bus shared with display; LovyanGFX uses its own driver, safe to call begin()
  SPI.begin(SCK, MISO, MOSI, LORA_CS);

  int state = lora.begin(LORA_FREQ_LIST[loraFreqIdx],
                         LORA_PRESETS[loraPresetIdx].bw,
                         LORA_PRESETS[loraPresetIdx].sf,
                         LORA_CR, LORA_SYNC_WORD, 10);
  if (state != RADIOLIB_ERR_NONE) {
    debugln("LoRa init failed");
    loraInitFailed = true;
    return;
  }

  // Enable LNA, point antenna switch to RX
  pinMode(LORA_LNA_ENABLE, OUTPUT);
  digitalWrite(LORA_LNA_ENABLE, HIGH);
  pinMode(LORA_ANTENNA_SWITCH, OUTPUT);
  digitalWrite(LORA_ANTENNA_SWITCH, LOW);

  lora.setDio1Action(loraISR);
  loraInitialized = true;
  debugln("LoRa initialized — tap LISTEN to start receiving");
}

// Send a Meshtastic-formatted AES-128-CTR encrypted TEXT_MESSAGE broadcast.
// Works with the default "LongFast" channel (PSK "AQ==").
void loraSendMeshACK() {
  // Build Data protobuf: portnum=1 (TEXT_MESSAGE_APP), payload="NESSO ACK"
  const char* msg = "NESSO ACK";
  uint8_t payload[32];
  uint8_t plen = 0;
  payload[plen++] = 0x08; payload[plen++] = 0x01;             // field 1: portnum
  payload[plen++] = 0x12; payload[plen++] = (uint8_t)strlen(msg); // field 2: bytes
  memcpy(payload + plen, msg, strlen(msg));
  plen += (uint8_t)strlen(msg);

  uint32_t pktId  = (uint32_t)millis();

  // AES-128-CTR encrypt the payload in-place
  // Nonce: [pktId 4B LE][zeros 4B][myNodeId 4B LE][zeros 4B]
  uint8_t nonce[16] = {};
  memcpy(nonce + 0, &pktId,          4);
  memcpy(nonce + 8, &MESH_MY_NODE_ID, 4);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, MESH_DEFAULT_KEY, 128);
  uint8_t stream[16];
  size_t ncOff = 0;
  mbedtls_aes_crypt_ctr(&aes, plen, &ncOff, nonce, stream, payload, payload);
  mbedtls_aes_free(&aes);

  // Build full LoRa packet: 16-byte header + encrypted payload
  uint8_t pkt[256];
  MeshHeader hdr;
  hdr.to         = 0xFFFFFFFF;       // broadcast
  hdr.from       = MESH_MY_NODE_ID;
  hdr.id         = pktId;
  hdr.flags      = 0x6B;             // hop_limit=3 (bits 2:0), want_ack=1 (bit 3), hop_start=3 (bits 7:5)
  hdr.channel    = MESH_CHANNEL_HASH;
  hdr.next_hop   = 0x00;
  hdr.relay_node = 0x00;
  memcpy(pkt, &hdr, sizeof(hdr));
  memcpy(pkt + sizeof(hdr), payload, plen);

  lora.standby();
  digitalWrite(LORA_ANTENNA_SWITCH, HIGH);  // TX path
  delay(10);
  lora.transmit(pkt, sizeof(hdr) + plen);
  digitalWrite(LORA_ANTENNA_SWITCH, LOW);   // RX path
  delay(5);

  // Wait for ROUTING_APP ACK from recipient (15s timeout)
  loraPendingAckId  = pktId;
  loraAckDeadlineMs = millis() + 15000;
  loraLastAckOk     = false;
  debugln("Mesh ACK sent, waiting for ACK");
}

// Send a custom text message over LoRa using the Meshtastic packet format.
// The radio must be initialised. After transmitting, receive is resumed if
// loraListening was true.
void loraSendText(const char* msg) {
  if (!loraInitialized || loraInitFailed) {
    serialWritelnAll("[LoRa] not initialised — navigate to LoRa screen first");
    return;
  }

  size_t msgLen = strlen(msg);
  if (msgLen == 0) { serialWritelnAll("[LoRa] empty message"); return; }
  if (msgLen > 200) msgLen = 200;  // cap to keep packet inside 255-byte limit

  // Build Data protobuf: field 1 = portnum (TEXT_MESSAGE_APP=1), field 2 = payload bytes
  uint8_t payload[220];
  uint8_t plen = 0;
  payload[plen++] = 0x08; payload[plen++] = 0x01;           // portnum = 1
  payload[plen++] = 0x12; payload[plen++] = (uint8_t)msgLen; // bytes field
  memcpy(payload + plen, msg, msgLen);
  plen += (uint8_t)msgLen;

  uint32_t pktId = (uint32_t)millis();

  // AES-128-CTR encrypt payload in-place
  uint8_t nonce[16] = {};
  memcpy(nonce + 0, &pktId,           4);
  memcpy(nonce + 8, &MESH_MY_NODE_ID, 4);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, MESH_DEFAULT_KEY, 128);
  uint8_t stream[16]; size_t ncOff = 0;
  mbedtls_aes_crypt_ctr(&aes, plen, &ncOff, nonce, stream, payload, payload);
  mbedtls_aes_free(&aes);

  // Build 16-byte Meshtastic header + encrypted payload
  uint8_t pkt[256];
  MeshHeader hdr;
  hdr.to         = 0xFFFFFFFF;
  hdr.from       = MESH_MY_NODE_ID;
  hdr.id         = pktId;
  hdr.flags      = 0x6B;             // hop_limit=3, want_ack=1, hop_start=3
  hdr.channel    = MESH_CHANNEL_HASH;
  hdr.next_hop   = 0x00;
  hdr.relay_node = 0x00;
  memcpy(pkt, &hdr, sizeof(hdr));
  memcpy(pkt + sizeof(hdr), payload, plen);

  // Re-arm SPI in case display rendered since last LoRa operation
  SPI.begin(SCK, MISO, MOSI, LORA_CS);

  lora.standby();
  digitalWrite(LORA_ANTENNA_SWITCH, HIGH);  // TX path
  delay(10);
  int state = lora.transmit(pkt, sizeof(hdr) + plen);
  digitalWrite(LORA_ANTENNA_SWITCH, LOW);   // RX path
  delay(5);

  if (state == RADIOLIB_ERR_NONE) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[LoRa] sent (%u bytes): %.*s", sizeof(hdr) + plen, (int)msgLen, msg);
    serialWritelnAll(buf);
  } else {
    char buf[48];
    snprintf(buf, sizeof(buf), "[LoRa] transmit error %d", state);
    serialWritelnAll(buf);
  }

  // Resume receive if the user had listening active
  if (loraListening) lora.startReceive();
}

// Portnum → human label (Meshtastic PortNum enum)
static const char* meshnumLabel(uint8_t portnum) {
  switch (portnum) {
    case 1:  return nullptr;      // TEXT_MESSAGE_APP — payload IS the text
    case 3:  return "<position>";
    case 4:  return "<node info>";
    case 5:  return "<routing>";
    case 8:  return "<range test>";
    case 67: return "<telemetry>";
    case 70: return "<detection>";
    default: return "<binary>";
  }
}

// Decrypt a Meshtastic payload and parse the Data protobuf.
// Fills portnum, requestId, and textOut (message text or portnum label).
static bool meshParseData(const uint8_t* buf, int len,
                           uint32_t fromNode, uint32_t pktId,
                           uint8_t& portnum, uint32_t& requestId,
                           char* textOut = nullptr, uint8_t textMax = 0) {
  if (len <= 0 || len > 255) return false;

  uint8_t plain[256];
  memcpy(plain, buf, len);

  uint8_t nonce[16] = {};
  memcpy(nonce + 0, &pktId,    4);
  memcpy(nonce + 8, &fromNode, 4);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, MESH_DEFAULT_KEY, 128);
  uint8_t stream[16];
  size_t ncOff = 0;
  mbedtls_aes_crypt_ctr(&aes, len, &ncOff, nonce, stream, plain, plain);
  mbedtls_aes_free(&aes);

  portnum   = 0;
  requestId = 0;

  int i = 0;
  while (i < len) {
    uint8_t tagByte = plain[i++];
    uint8_t field   = tagByte >> 3;
    uint8_t wire    = tagByte & 0x07;

    if (wire == 0) {                          // varint
      uint32_t val = 0; int shift = 0;
      while (i < len) {
        uint8_t b = plain[i++];
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
      }
      if (field == 1) portnum   = (uint8_t)val;
      if (field == 3) requestId = val;
    } else if (wire == 2) {                   // length-delimited
      uint32_t sz = 0; int shift = 0;
      while (i < len) {
        uint8_t b = plain[i++];
        sz |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
      }
      if (field == 2 && textOut && textMax > 0) {
        // payload field — copy as text (only useful for TEXT_MESSAGE_APP)
        uint8_t copy = (uint8_t)min((uint32_t)(textMax - 1), sz);
        memcpy(textOut, plain + i, copy);
        textOut[copy] = '\0';
        // Sanitise: replace non-printable chars with '.'
        for (uint8_t c = 0; c < copy; c++)
          if (textOut[c] < 32 || textOut[c] > 126) textOut[c] = '.';
      }
      i += sz;
    } else { break; }
  }
  return true;
}

// Read a received packet, detect ACKs, and optionally auto-reply (once at a time).
void loraCheckPacket() {
  if (!loraListening) return;   // user hasn't started listening
  // Check ACK timeout
  if (loraPendingAckId != 0 && millis() > loraAckDeadlineMs) {
    debugln("ACK timeout");
    loraPendingAckId = 0;
    loraLastAckOk    = false;
  }

  if (!loraPacketFlag) return;
  loraPacketFlag = false;

  uint8_t buf[256];
  int len   = lora.getPacketLength();
  int state = lora.readData(buf, min(len, (int)sizeof(buf)));

  if (state != RADIOLIB_ERR_NONE || len < 16) {
    lora.startReceive();
    return;
  }

  // Parse unencrypted Meshtastic header
  uint32_t pktTo, pktFrom, pktId;
  memcpy(&pktTo,   buf + 0, 4);
  memcpy(&pktFrom, buf + 4, 4);
  memcpy(&pktId,   buf + 8, 4);

  // ── Dedup: skip retransmits with the same packet ID ─────────
  if (loraDedup) {
    for (int i = 0; i < loraLogCount; i++) {
      if (loraLog[i].pktId == pktId) {
        lora.startReceive();
        return;
      }
    }
  }

  // ── ACK detection: packet addressed directly to us ────────────
  if (pktTo == MESH_MY_NODE_ID && loraPendingAckId != 0) {
    uint8_t portnum = 0; uint32_t requestId = 0;
    if (meshParseData(buf + 16, len - 16, pktFrom, pktId, portnum, requestId)) {
      if (portnum == 5 && requestId == loraPendingAckId) {
        loraLastAckOk    = true;
        loraPendingAckId = 0;
        debugln("ACK received!");
      }
    }
    lora.startReceive();
    return;
  }

  // ── Ignore our own rebroadcasts ───────────────────────────────
  if (pktFrom == MESH_MY_NODE_ID) {
    lora.startReceive();
    return;
  }

  // ── Log the received packet ───────────────────────────────────
  if (loraLogCount < LORA_LOG_SIZE) loraLogCount++;
  for (int i = loraLogCount - 1; i > 0; i--) loraLog[i] = loraLog[i - 1];
  loraLog[0].rssi    = (int16_t)lora.getRSSI();
  loraLog[0].snr     = lora.getSNR();
  loraLog[0].size    = (uint8_t)constrain(len, 0, 255);
  loraLog[0].ms      = millis();
  loraLog[0].srcNode = pktFrom;
  loraLog[0].pktId   = pktId;
  loraLog[0].text[0] = '\0';
  loraTotalPackets++;
  loraScrollOffset   = 0;  // snap back to newest on new message

  // Decrypt and decode message text (or portnum label for non-text packets)
  if (len > 16) {
    uint8_t portnum = 0; uint32_t reqId = 0;
    char textBuf[52] = {};
    if (meshParseData(buf + 16, len - 16, pktFrom, pktId, portnum, reqId,
                      textBuf, sizeof(textBuf))) {
      if (portnum == 1 && textBuf[0] != '\0') {
        strncpy(loraLog[0].text, textBuf, sizeof(loraLog[0].text) - 1);
      } else {
        const char* label = meshnumLabel(portnum);
        strncpy(loraLog[0].text, label ? label : "<binary>", sizeof(loraLog[0].text) - 1);
      }
    }
  }

  // ── Auto-reply: only if idle (no pending ACK) ─────────────────
  if (loraAutoReply && loraPendingAckId == 0)
    loraSendMeshACK();

  lora.startReceive();
}

// Format elapsed time as "5s", "3m", "2h"
static void fmtAgo(uint32_t ms, char* buf, size_t sz) {
  uint32_t s = (millis() - ms) / 1000;
  if      (s < 60)   snprintf(buf, sz, "%lus",    s);
  else if (s < 3600) snprintf(buf, sz, "%lum",    s / 60);
  else               snprintf(buf, sz, "%luh",    s / 3600);
}

// Apply current loraPresetIdx / loraFreqIdx / loraAutoReply to the radio.
// Reconfigures without full re-init (preserves sync word, preamble, etc.)
void applyLoraSettings() {
  saveSettings();
  if (!loraInitialized) return;
  lora.standby();
  lora.setFrequency(LORA_FREQ_LIST[loraFreqIdx]);
  lora.setBandwidth(LORA_PRESETS[loraPresetIdx].bw);
  lora.setSpreadingFactor(LORA_PRESETS[loraPresetIdx].sf);
  if (loraListening) lora.startReceive();
  debugln("LoRa reconfigured");
}

// ── Settings overlay ─────────────────────────────────────────────

void renderLoraSettings() {
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  static const int ITEM_COUNT = 5;
  const char* labels[ITEM_COUNT] = {"PRESET", "FREQUENCY", "AUTO-REPLY", "DEDUP", "RESET"};
  char values[ITEM_COUNT][16];
  snprintf(values[0], 16, "%s",       LORA_PRESETS[loraPresetIdx].name);
  snprintf(values[1], 16, "%.3f MHz", LORA_FREQ_LIST[loraFreqIdx]);
  snprintf(values[2], 16, "%s",       loraAutoReply ? "ON" : "OFF");
  snprintf(values[3], 16, "%s",       loraDedup     ? "ON" : "OFF");
  snprintf(values[4], 16, "%s",       "\x10");  // right-arrow glyph

  // Title + divider
  int titleY = isLandscape ? 4 : 6;
  int divY   = isLandscape ? 22 : 26;
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_WHITE);
  statusSprite.drawString("LORA SETTINGS", sw / 2, titleY);
  statusSprite.drawFastHLine(8, divY, sw - 16, COLOR_TEAL);

  // Bottom-anchored APPLY / CANCEL buttons — side-by-side in both orientations
  int btnH = isLandscape ? 14 : 22;
  int btnY = sh - 6 - btnH;
  int sepY = btnY - 6;
  statusSprite.drawFastHLine(8, sepY, sw - 16, display.color565(40, 40, 40));
  int bw = (sw - 24) / 2;
  statusSprite.fillRect(8,       btnY, bw, btnH, display.color565(0, 60, 0));
  statusSprite.fillRect(16 + bw, btnY, bw, btnH, display.color565(60, 0, 0));
  statusSprite.drawRect(8,       btnY, bw, btnH, COLOR_GREEN);
  statusSprite.drawRect(16 + bw, btnY, bw, btnH, COLOR_RED);
  statusSprite.setTextSize(1);
  statusSprite.setTextDatum(MC_DATUM);
  statusSprite.setTextColor(COLOR_GREEN);
  statusSprite.drawString("APPLY",  8  + bw / 2,       btnY + btnH / 2);
  statusSprite.setTextColor(COLOR_RED);
  statusSprite.drawString("CANCEL", 16 + bw + bw / 2,  btnY + btnH / 2);

  // Scrollable settings rows (fills space between divider and separator)
  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);

  // Clamp and auto-scroll to keep cursor visible
  settingsScrollOffset = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);
  if (settingsCursor < settingsScrollOffset)
    settingsScrollOffset = settingsCursor;
  if (settingsCursor >= settingsScrollOffset + visRows)
    settingsScrollOffset = settingsCursor - visRows + 1;

  // Scroll indicator
  if (ITEM_COUNT > visRows) {
    int barH = max(6, rowsArea * visRows / ITEM_COUNT);
    int barY = startY + (rowsArea - barH) * settingsScrollOffset / max(1, ITEM_COUNT - visRows);
    statusSprite.fillRect(sw - 4, startY, 3, rowsArea, display.color565(20, 20, 20));
    statusSprite.fillRect(sw - 4, barY,   3, barH,     COLOR_TEAL);
  }

  for (int i = 0; i < visRows; i++) {
    int idx      = i + settingsScrollOffset;
    if (idx >= ITEM_COUNT) break;
    int y        = startY + i * rowH;
    bool selected = (idx == settingsCursor);
    bool isReset  = (idx == ITEM_COUNT - 1);
    int textY    = y + (rowH - 8) / 2;  // vertically centre Font0 (8px tall)

    if (isReset && selected)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(60, 15, 15));
    else if (selected)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(30, 30, 50));

    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(selected ? (isReset ? display.color565(255,80,80) : COLOR_TEAL)
                                       : display.color565(60, 60, 60));
    statusSprite.drawString(selected ? ">" : " ", 6, textY);

    statusSprite.setTextColor(selected ? COLOR_WHITE : (isReset ? display.color565(100,40,40) : COLOR_GRAY));
    statusSprite.drawString(labels[idx], 18, textY);

    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.setTextColor(selected ? (isReset ? display.color565(255,80,80) : COLOR_ORANGE) : COLOR_GRAY);
    statusSprite.drawString(values[idx], sw - 8, textY);

    if (i < visRows - 1 && idx < ITEM_COUNT - 1)
      statusSprite.drawFastHLine(8, y + rowH - 1, sw - 16, display.color565(25, 25, 25));
  }
}

void renderLora() {
  // Delegate to settings / reset overlays when active
  if (navState == NAV_SETTINGS) { renderLoraSettings(); return; }
  if (navState == NAV_RESET)    { renderLoraReset();    return; }

  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;
  bool blink = (millis() / 500) % 2;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // ── Status dot color ─────────────────────────────────────────
  uint16_t dotColor = loraInitFailed   ? COLOR_RED :
                      !loraListening   ? COLOR_GRAY :
                      (blink           ? COLOR_GREEN : display.color565(0, 80, 0));

  if (loraInitFailed) {
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_RED);
    statusSprite.drawString("LORA INIT", sw / 2, sh / 2 - 12);
    statusSprite.drawString("FAILED", sw / 2, sh / 2 + 12);
    return;
  }

  if (isLandscape) {
    // ── Landscape layout ─────────────────────────────────────────
    // Header row
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString("LORA SCAN", 8, 6);

    char freqStr[16];
    snprintf(freqStr, sizeof(freqStr), "%.3f MHz", LORA_FREQ_LIST[loraFreqIdx]);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(freqStr, sw - 18, 10);

    // Blinking scan dot
    statusSprite.fillCircle(sw - 8, 10, 5, dotColor);

    // ACK status (shown only when auto-reply is on)
    if (loraAutoReply) {
      statusSprite.setTextDatum(TL_DATUM);
      statusSprite.setTextSize(1);
      if (loraPendingAckId != 0) {
        statusSprite.setTextColor(COLOR_ORANGE);
        statusSprite.drawString("wait ACK", 8, sh - 10);
      } else if (loraLastAckOk) {
        statusSprite.setTextColor(COLOR_GREEN);
        statusSprite.drawString("ACK OK", 8, sh - 10);
      } else if (loraTotalPackets > 0) {
        statusSprite.setTextColor(COLOR_RED);
        statusSprite.drawString("no ACK", 8, sh - 10);
      }
    }

    statusSprite.drawFastHLine(8, 26, sw - 16, COLOR_GRAY);

    if (!loraListening && loraLogCount == 0) {
      // Idle — show LISTEN button
      int bw = 80, bh = 26, bx = (sw - bw) / 2, by = sh / 2 - bh / 2 + 8;
      statusSprite.fillRoundRect(bx, by, bw, bh, 5, display.color565(0, 60, 30));
      statusSprite.drawRoundRect(bx, by, bw, bh, 5, COLOR_GREEN);
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_WHITE);
      statusSprite.drawString("LISTEN", sw / 2, by + bh / 2 + 1);
    } else if (loraLogCount == 0) {
      // Listening but no packets yet
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString("Listening...", sw / 2, sh / 2 + 8);
    } else {
      char totalStr[16];
      snprintf(totalStr, sizeof(totalStr), "TOTAL %lu", loraTotalPackets);
      statusSprite.setTextSize(1);
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString(totalStr, sw - 8, 30);

      // Scroll indicator (right edge, if more entries than fit)
      int lsVisibleRows = (sh - 54) / 26;
      if (loraLogCount > lsVisibleRows) {
        int barH = max(8, (int)(sh * lsVisibleRows / loraLogCount));
        int barY = 42 + (sh - 42 - barH) * loraScrollOffset / max(1, loraLogCount - lsVisibleRows);
        statusSprite.fillRect(sw - 4, 42, 3, sh - 42, display.color565(20, 20, 20));
        statusSprite.fillRect(sw - 4, barY, 3, barH, COLOR_TEAL);
      }

      // Packet rows — 2 lines each: signal info + message text
      for (int i = 0; i < loraLogCount; i++) {
        int logIdx = i + loraScrollOffset;
        if (logIdx >= loraLogCount) break;
        int y = 42 + i * 26;
        if (y + 22 > sh - 12) break;
        char tmp[24], agoStr[8];
        uint16_t col = (logIdx == 0) ? COLOR_WHITE : COLOR_GRAY;

        // Line 1: node ID  RSSI  SNR  ago
        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextColor(display.color565(70, 100, 160));
        snprintf(tmp, sizeof(tmp), "!%08lX", loraLog[logIdx].srcNode);
        statusSprite.drawString(tmp, 8, y);

        statusSprite.setTextColor(col);
        snprintf(tmp, sizeof(tmp), "%ddBm %+.0fdB", loraLog[logIdx].rssi, loraLog[logIdx].snr);
        statusSprite.drawString(tmp, 82, y);

        fmtAgo(loraLog[logIdx].ms, agoStr, sizeof(agoStr));
        statusSprite.setTextDatum(TR_DATUM);
        statusSprite.setTextColor(display.color565(100, 100, 100));
        statusSprite.drawString(agoStr, sw - 8, y);

        // Line 2: message text (truncated to fit)
        statusSprite.setTextDatum(TL_DATUM);
        bool isText = (loraLog[logIdx].text[0] == '<' ? false : loraLog[logIdx].text[0] != '\0');
        statusSprite.setTextColor(isText ? display.color565(200, 200, 100) : display.color565(80, 80, 80));
        statusSprite.drawString(loraLog[logIdx].text[0] ? loraLog[logIdx].text : "...", 8, y + 12);

        if (logIdx < loraLogCount - 1)
          statusSprite.drawFastHLine(8, y + 24, sw - 16, display.color565(25, 25, 25));
      }
      // Settings hint
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(display.color565(60, 60, 60));
      statusSprite.drawString("K1 hold:settings", sw - 8, sh - 10);
    }

  } else {
    // ── Portrait layout ───────────────────────────────────────────
    statusSprite.setTextDatum(TC_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString("LORA SCAN", sw / 2, 6);

    char freqStr[20];
    snprintf(freqStr, sizeof(freqStr), "%.3f MHz", LORA_FREQ_LIST[loraFreqIdx]);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.drawString(freqStr, sw / 2, 26);

    // Scan indicator dot
    statusSprite.fillCircle(sw / 2 + 48, 29, 4, dotColor);

    // ACK status
    if (loraAutoReply) {
      statusSprite.setTextDatum(TC_DATUM);
      statusSprite.setTextSize(1);
      if (loraPendingAckId != 0) {
        statusSprite.setTextColor(COLOR_ORANGE);
        statusSprite.drawString("waiting ACK...", sw / 2, 40);
      } else if (loraLastAckOk) {
        statusSprite.setTextColor(COLOR_GREEN);
        statusSprite.drawString("ACK received", sw / 2, 40);
      } else if (loraTotalPackets > 0) {
        statusSprite.setTextColor(COLOR_RED);
        statusSprite.drawString("no ACK", sw / 2, 40);
      }
    }

    statusSprite.drawFastHLine(8, 40, sw - 16, COLOR_GRAY);

    if (!loraListening && loraLogCount == 0) {
      // Idle — show LISTEN button
      int bw = 90, bh = 30, bx = (sw - bw) / 2, by = sh / 2 - bh / 2 + 10;
      statusSprite.fillRoundRect(bx, by, bw, bh, 6, display.color565(0, 60, 30));
      statusSprite.drawRoundRect(bx, by, bw, bh, 6, COLOR_GREEN);
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(2);
      statusSprite.setTextColor(COLOR_WHITE);
      statusSprite.drawString("LISTEN", sw / 2, by + bh / 2 + 1);
    } else if (loraLogCount == 0) {
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString("Listening...", sw / 2, sh / 2 + 8);
    } else {
      // Total count
      char totalStr[20];
      snprintf(totalStr, sizeof(totalStr), "Total: %lu", loraTotalPackets);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.setTextDatum(TC_DATUM);
      statusSprite.drawString(totalStr, sw / 2, 46);

      statusSprite.drawFastHLine(8, 58, sw - 16, display.color565(30, 30, 30));

      // Scroll indicator (right edge, if more entries than fit)
      int lpVisibleRows = (sh - 64) / 44;
      if (loraLogCount > lpVisibleRows) {
        int barH = max(8, (int)(sh * lpVisibleRows / loraLogCount));
        int barY = 64 + (sh - 64 - barH) * loraScrollOffset / max(1, loraLogCount - lpVisibleRows);
        statusSprite.fillRect(sw - 4, 64, 3, sh - 64, display.color565(20, 20, 20));
        statusSprite.fillRect(sw - 4, barY, 3, barH, COLOR_TEAL);
      }

      // Packet entries — 3 lines each: node+signal / message / separator
      for (int i = 0; i < loraLogCount; i++) {
        int logIdx = i + loraScrollOffset;
        if (logIdx >= loraLogCount) break;
        int y = 64 + i * 44;
        if (y + 38 > sh - 12) break;
        char tmp[24], agoStr[8];
        uint16_t col = (logIdx == 0) ? COLOR_WHITE : COLOR_GRAY;

        // Line 1: node ID (left) + ago (right)
        statusSprite.setTextSize(1);
        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextColor(display.color565(70, 100, 160));
        snprintf(tmp, sizeof(tmp), "!%08lX", loraLog[logIdx].srcNode);
        statusSprite.drawString(tmp, 8, y);

        fmtAgo(loraLog[logIdx].ms, agoStr, sizeof(agoStr));
        statusSprite.setTextDatum(TR_DATUM);
        statusSprite.setTextColor(display.color565(100, 100, 100));
        statusSprite.drawString(agoStr, sw - 8, y);

        // Line 2: RSSI (left) + SNR (right)
        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextColor(col);
        snprintf(tmp, sizeof(tmp), "%d dBm", loraLog[logIdx].rssi);
        statusSprite.drawString(tmp, 8, y + 13);

        statusSprite.setTextDatum(TR_DATUM);
        snprintf(tmp, sizeof(tmp), "SNR %+.1f", loraLog[logIdx].snr);
        statusSprite.drawString(tmp, sw - 8, y + 13);

        // Line 3: message text
        statusSprite.setTextDatum(TL_DATUM);
        bool isText = (loraLog[logIdx].text[0] != '\0' && loraLog[logIdx].text[0] != '<');
        statusSprite.setTextColor(isText ? display.color565(220, 220, 80) : display.color565(80, 80, 80));
        // Truncate to ~20 chars so it fits portrait width
        char dispText[22];
        strncpy(dispText, loraLog[logIdx].text[0] ? loraLog[logIdx].text : "...", 21);
        dispText[21] = '\0';
        statusSprite.drawString(dispText, 8, y + 26);

        if (logIdx < loraLogCount - 1)
          statusSprite.drawFastHLine(8, y + 40, sw - 16, display.color565(25, 25, 25));
      }
      // Settings hint at the bottom
      statusSprite.setTextDatum(TC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(display.color565(60, 60, 60));
      statusSprite.drawString("K1 hold:settings", sw / 2, sh - 10);
    }
  }
}


// ================================================================
// FUNCTION_BT — Bluetooth LE scanner
// ================================================================

// Start a fixed-duration scan window (BT_SCAN_DURATION_S seconds).
// Called again from renderBT() when the window expires → continuous coverage
// without using start(0) which can block on ESP32-C6.
void btStartScan() {
  if (!btInitialized || btInitFailed || btScanning) return;
  // Mark scanning before start() so the render shows "Scanning..." immediately.
  btScanning    = true;
  btScanStartMs = millis();
  // Force a render now — pBLEScan->start() blocks while the BLE stack initialises
  // and the normal render loop cannot run during that time.
  if (currentFunction == FUNCTION_BT) {
    renderBT();
    statusSprite.pushSprite(0, SPRITE_Y);
  }
  pBLEScan->setActiveScan(btScanModeIdx == 0);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(BT_SCAN_DURATION_S, false);  // fixed window, non-blocking
  debugln("BT scan started");
}

void btStopScan() {
  if (!btScanning) return;
  pBLEScan->stop();
  pBLEScan->clearResults();
  btScanning = false;
  debugln("BT scan stopped");
}

void applyBTSettings() {
  saveSettings();
  if (!btInitialized || btInitFailed) return;
  btStopScan();
  if (btAdvEnabled) {
    // Include the device name in the advertising packet so phones show
    // "NESSO" without needing to connect first.
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
    advData.setFlags(0x06);   // LE General Discoverable | BR/EDR Not Supported
    advData.setCompleteServices(BLEUUID(BLE_UART_SERVICE_UUID));
    pAdv->setAdvertisementData(advData);
    BLEAdvertisementData scanResp;
    scanResp.setName("NESSO");
    pAdv->setScanResponseData(scanResp);
    pAdv->setMinPreferred(0x06);
    pAdv->setMaxPreferred(0x12);
    pAdv->start();
    debugln("BT advertising started");
  } else {
    BLEDevice::stopAdvertising();
    debugln("BT advertising stopped");
  }
  if (btScanRequested) btStartScan();
}

// Called every loop() iteration — handles BLE connect/disconnect events
// on the main task regardless of which screen is currently active.
void btProcessPendingEvents() {
  if (btConnectPending) {
    btConnectPending = false;
    // Best-guess: most recently seen connectable device in the scan log
    btConnectedName[0] = '\0';
    btConnectedAddr[0] = '\0';
    uint32_t newest = 0;
    for (int i = 0; i < btLogCount; i++) {
      if (btLog[i].connectable && btLog[i].lastSeenMs >= newest) {
        newest = btLog[i].lastSeenMs;
        strncpy(btConnectedAddr, btLog[i].addr, 17); btConnectedAddr[17] = '\0';
        strncpy(btConnectedName, btLog[i].name, 23); btConnectedName[23] = '\0';
      }
    }
    btStopScan();
    btLogCount = 0; btTotalSeen = 0; btScrollOffset = 0; btSelectedAddr[0] = '\0';
    // Delay welcome so the phone has time to subscribe to BLE notifications
    btWelcomePending   = true;
    btWelcomePendingMs = millis();
  }

  // Send welcome ~2 s after connect, by which time the client will have subscribed
  if (btWelcomePending && btConnected &&
      (millis() - btWelcomePendingMs >= 2000)) {
    btWelcomePending = false;
    serialWritelnAll("=== BLE UART connected ===");
    serialPrintHelp();
    serialPrintFunctionHelp((int)currentFunction);
  }

  if (btDisconnectPending) {
    btDisconnectPending = false;
    btWelcomePending    = false;
    serialWritelnAll("=== BLE UART disconnected ===");
    if (btScanRequested) btStartScan();   // only resume if user had scan running
    if (btAdvEnabled) {
      BLEDevice::getAdvertising()->start();
      debugln("BT advertising restarted after disconnect");
    }
  }
}

// Pure BLE stack init — no display output; safe to call from loop() or initBT().
void btInitStack() {
  if (btInitialized || btInitFailed) return;

  BLEDevice::init("NESSO");
  delay(50);   // let FreeRTOS BLE task settle before we touch the scan object

  // Server must exist so incoming connections have a GATT handler.
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(&btServerCB);

  // Nordic UART Service — exposes BLE serial to any NUS-compatible client.
  BLEService* pUartSvc = pServer->createService(BLE_UART_SERVICE_UUID);
  pBLETxChar = pUartSvc->createCharacteristic(
    BLE_UART_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  // BLE2902 (CCCD) is added automatically by NimBLE when NOTIFY is enabled.
  BLECharacteristic* pRxChar = pUartSvc->createCharacteristic(
    BLE_UART_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxChar->setCallbacks(&bleRxCB);
  pUartSvc->start();
  bleUartReady = true;
  debugln("BLE UART service started");

  // No bonding — avoids stale-key GATT errors on reconnect after reboot.
  BLESecurity* pSec = new BLESecurity();
  pSec->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
  pSec->setCapability(ESP_IO_CAP_NONE);

  pBLEScan = BLEDevice::getScan();
  if (!pBLEScan) {
    btInitFailed = true;
    debugln("BT init failed: no scan object");
    return;
  }
  pBLEScan->setAdvertisedDeviceCallbacks(&btScanCB, true);
  btInitialized = true;
  debugln("BT initialized");
  applyBTSettings();  // start advertising + scan
}

void initBT() {
  display.fillScreen(COLOR_BLACK);
  renderHeader();
  btScrollOffset    = 0;
  btSelectedAddr[0] = '\0';
  btScanRequested   = false;   // user must tap SCAN; no auto-start on entry

  if (btInitFailed) return;

  if (!btInitialized) {
    statusSprite.fillSprite(COLOR_BLACK);
    statusSprite.setFont(&fonts::Font0);
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    int sw = statusSprite.width(), sh = statusSprite.height();
    statusSprite.drawString("Initializing BT...", sw / 2, sh / 2);
    statusSprite.pushSprite(0, SPRITE_Y);
    btInitStack();
    // btInitStack → applyBTSettings starts advertising but NOT scan
    btStopScan();   // cancel the scan applyBTSettings kicked off
    return;
  }
  // Don't start scan — wait for user to tap SCAN
}

void renderBTSettings() {
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  static const int ITEM_COUNT = 6;
  const char* labels[ITEM_COUNT] = {"SCAN MODE", "RSSI FILTER", "DEBUG LOG", "ADVERTISING", "BT ON BOOT", "RESET"};
  char values[ITEM_COUNT][16];
  snprintf(values[0], 16, "%s",  btScanModeIdx == 0 ? "ACTIVE" : "PASSIVE");
  snprintf(values[1], 16, "%s",  BT_RSSI_LABELS[btRssiFilterIdx]);
  snprintf(values[2], 16, "%s",  btDebugMode ? "ON" : "OFF");
  snprintf(values[3], 16, "%s",  btAdvEnabled ? "ON" : "OFF");
  snprintf(values[4], 16, "%s",  btStartupEnabled ? "ON" : "OFF");
  snprintf(values[5], 16, "%s",  "\x10");  // right-arrow glyph

  int titleY = isLandscape ? 4 : 6;
  int divY   = isLandscape ? 22 : 26;
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_WHITE);
  statusSprite.drawString("BT SETTINGS", sw / 2, titleY);
  statusSprite.drawFastHLine(8, divY, sw - 16, display.color565(0, 80, 160));

  int btnH = isLandscape ? 14 : 22;
  int btnY = sh - 6 - btnH;
  int sepY = btnY - 6;
  statusSprite.drawFastHLine(8, sepY, sw - 16, display.color565(40, 40, 40));
  int bw = (sw - 24) / 2;
  statusSprite.fillRect(8,       btnY, bw, btnH, display.color565(0, 60, 0));
  statusSprite.fillRect(16 + bw, btnY, bw, btnH, display.color565(60, 0, 0));
  statusSprite.drawRect(8,       btnY, bw, btnH, COLOR_GREEN);
  statusSprite.drawRect(16 + bw, btnY, bw, btnH, COLOR_RED);
  statusSprite.setTextSize(1);
  statusSprite.setTextDatum(MC_DATUM);
  statusSprite.setTextColor(COLOR_GREEN);
  statusSprite.drawString("APPLY",  8  + bw / 2,       btnY + btnH / 2);
  statusSprite.setTextColor(COLOR_RED);
  statusSprite.drawString("CANCEL", 16 + bw + bw / 2,  btnY + btnH / 2);

  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);

  settingsScrollOffset = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);
  if (settingsCursor < settingsScrollOffset)
    settingsScrollOffset = settingsCursor;
  if (settingsCursor >= settingsScrollOffset + visRows)
    settingsScrollOffset = settingsCursor - visRows + 1;

  if (ITEM_COUNT > visRows) {
    int barH = max(6, rowsArea * visRows / ITEM_COUNT);
    int barY = startY + (rowsArea - barH) * settingsScrollOffset / max(1, ITEM_COUNT - visRows);
    statusSprite.fillRect(sw - 4, startY, 3, rowsArea, display.color565(20, 20, 20));
    statusSprite.fillRect(sw - 4, barY,   3, barH,     display.color565(0, 80, 160));
  }

  for (int i = 0; i < visRows; i++) {
    int idx      = i + settingsScrollOffset;
    if (idx >= ITEM_COUNT) break;
    int y        = startY + i * rowH;
    bool selected = (idx == settingsCursor);
    bool isReset  = (idx == ITEM_COUNT - 1);
    int textY    = y + (rowH - 8) / 2;

    if (isReset && selected)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(60, 15, 15));
    else if (selected)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(20, 30, 50));

    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(selected ? (isReset ? display.color565(255,80,80) : display.color565(0,140,255))
                                       : display.color565(60, 60, 60));
    statusSprite.drawString(selected ? ">" : " ", 6, textY);

    statusSprite.setTextColor(selected ? COLOR_WHITE : (isReset ? display.color565(100,40,40) : COLOR_GRAY));
    statusSprite.drawString(labels[idx], 18, textY);

    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.setTextColor(selected ? (isReset ? display.color565(255,80,80) : COLOR_ORANGE) : COLOR_GRAY);
    statusSprite.drawString(values[idx], sw - 8, textY);

    if (i < visRows - 1 && idx < ITEM_COUNT - 1)
      statusSprite.drawFastHLine(8, y + rowH - 1, sw - 16, display.color565(25, 25, 25));
  }
}

void renderBTDetail() {
  // Find selected device by MAC address
  int idx = -1;
  for (int i = 0; i < btLogCount; i++) {
    if (strcmp(btLog[i].addr, btSelectedAddr) == 0) { idx = i; break; }
  }
  if (idx < 0) { btSelectedAddr[0] = '\0'; return; }  // evicted — back to list

  const BTEntry& e = btLog[idx];
  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // Title: name if available, otherwise MAC
  const char* title = e.name[0] ? e.name : e.addr;
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(isLandscape ? 1 : 2);
  statusSprite.setTextColor(COLOR_WHITE);
  int titleY = isLandscape ? 4 : 6;
  statusSprite.drawString(title, sw / 2, titleY);

  int divY = isLandscape ? 18 : 26;
  statusSprite.drawFastHLine(8, divY, sw - 16, display.color565(0, 60, 100));

  int y = divY + 6;
  int lineH = isLandscape ? 14 : 16;
  statusSprite.setTextDatum(TL_DATUM);
  statusSprite.setTextSize(1);

  // RSSI
  char rssiStr[20];
  snprintf(rssiStr, sizeof(rssiStr), "RSSI: %d dBm", (int)e.rssi);
  uint16_t rssiCol = e.rssi > -60 ? COLOR_GREEN : e.rssi > -80 ? COLOR_ORANGE : COLOR_RED;
  statusSprite.setTextColor(rssiCol);
  statusSprite.drawString(rssiStr, 8, y); y += lineH;

  // Full MAC address
  statusSprite.setTextColor(display.color565(80, 120, 200));
  statusSprite.drawString(e.addr, 8, y); y += lineH;

  // Connectable flag
  statusSprite.setTextColor(e.connectable ? COLOR_GREEN : COLOR_GRAY);
  statusSprite.drawString(e.connectable ? "Connectable: YES" : "Connectable: NO", 8, y); y += lineH;

  // Last seen
  char agoStr[8]; fmtAgo(e.lastSeenMs, agoStr, sizeof(agoStr));
  char seenStr[24];
  snprintf(seenStr, sizeof(seenStr), "Last seen: %s", agoStr);
  statusSprite.setTextColor(COLOR_GRAY);
  statusSprite.drawString(seenStr, 8, y); y += lineH;

  // Manufacturer data (debug mode)
  if (btDebugMode && e.rawLen > 0) {
    char hexBuf[40] = {};
    int hlen = min((int)e.rawLen, isLandscape ? 8 : 6);
    for (int b = 0; b < hlen; b++)
      snprintf(hexBuf + b * 3, 4, "%02X ", e.rawData[b]);
    statusSprite.setTextColor(display.color565(60, 60, 60));
    statusSprite.drawString(hexBuf, 8, y);
  }

  // Return hint at bottom
  statusSprite.setTextDatum(BC_DATUM);
  statusSprite.setTextColor(display.color565(60, 60, 60));
  statusSprite.drawString("TAP TO RETURN", sw / 2, sh - 4);
}

void renderBTConnected() {
  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLandscape = sw > sh;
  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_GREEN);
  statusSprite.drawString("CONNECTED", sw / 2, isLandscape ? 6 : 8);

  int divY = isLandscape ? 26 : 32;
  statusSprite.drawFastHLine(8, divY, sw - 16, display.color565(0, 100, 0));

  statusSprite.setTextSize(1);
  int y = divY + 10;

  if (btConnectedName[0]) {
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString(btConnectedName, sw / 2, y);
    y += isLandscape ? 14 : 16;
  }
  statusSprite.setTextColor(display.color565(80, 120, 200));
  statusSprite.drawString(btConnectedAddr[0] ? btConnectedAddr : "(unknown)", sw / 2, y);
  y += isLandscape ? 16 : 20;

  statusSprite.setTextColor(display.color565(0, 160, 80));
  statusSprite.drawString("BLE UART active", sw / 2, y);

  statusSprite.setTextDatum(BC_DATUM);
  statusSprite.setTextColor(display.color565(50, 50, 50));
  statusSprite.drawString("Disconnect from remote to resume scan", sw / 2, sh - 4);
}

void renderBT() {
  if (navState == NAV_SETTINGS) { renderBTSettings(); return; }
  if (navState == NAV_RESET)    { renderBTReset();    return; }

  // Init failed — show error
  if (btInitFailed) {
    int sw = statusSprite.width(), sh = statusSprite.height();
    statusSprite.fillSprite(COLOR_BLACK);
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_RED);
    statusSprite.drawString("BT INIT", sw / 2, sh / 2 - 12);
    statusSprite.drawString("FAILED",  sw / 2, sh / 2 + 12);
    delay(100);
    return;
  }

  // Detail view for a selected device
  if (btSelectedAddr[0] != '\0') {
    renderBTDetail();
    return;
  }

  // (BLE connection events are handled in btProcessPendingEvents(), called from loop())

  // Show connected state while a remote device is connected
  if (btConnected) {
    renderBTConnected();
    return;
  }

  // Restart scan window when the previous one has expired.
  // Do NOT set btScanning=false — that would gray out all entries during the brief restart gap.
  // Do NOT call pBLEScan->stop() — the scan already expired naturally and stop() blocks
  // for 50-200ms waiting for the BLE task, which freezes touch and rendering.
  if (btInitialized && btScanning &&
      (millis() - btScanStartMs > (unsigned long)(BT_SCAN_DURATION_S * 1000 + 500))) {
    pBLEScan->clearResults();
    pBLEScan->setActiveScan(btScanModeIdx == 0);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(BT_SCAN_DURATION_S, false);
    btScanStartMs = millis();
    // btScanning stays true throughout — no gray flash
  }

  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;
  bool blink = (millis() / 500) % 2;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // Scan dot: blue blink when scanning, gray when stopped
  uint16_t dotColor = !btScanning ? COLOR_GRAY :
                      (blink ? display.color565(0, 120, 255) : display.color565(0, 40, 100));

  if (isLandscape) {
    // ── Landscape ────────────────────────────────────────────────
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString("BT SCAN", 8, 6);

    const char* modeStr = btScanModeIdx == 0 ? "ACTIVE" : "PASSIVE";
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(display.color565(0, 100, 200));
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.drawString(modeStr, sw - 18, 10);
    statusSprite.fillCircle(sw - 8, 10, 5, dotColor);

    if (btAdvEnabled) {
      statusSprite.setTextDatum(TL_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_ORANGE);
      statusSprite.drawString("ADV", 8, sh - 10);
    }

    statusSprite.drawFastHLine(8, 26, sw - 16, display.color565(0, 60, 100));

    if (btLogCount == 0 && !btScanning) {
      // Idle — show SCAN button
      int bw = 80, bh = 26, bx = (sw - bw) / 2, by = sh / 2 - bh / 2 + 8;
      statusSprite.fillRoundRect(bx, by, bw, bh, 5, display.color565(0, 60, 120));
      statusSprite.drawRoundRect(bx, by, bw, bh, 5, display.color565(0, 120, 255));
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_WHITE);
      statusSprite.drawString("SCAN", sw / 2, by + bh / 2 + 1);
    } else if (btLogCount == 0) {
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString("Scanning...", sw / 2, sh / 2 + 8);
    } else {
      char totalStr[20];
      snprintf(totalStr, sizeof(totalStr), "SEEN %d", btTotalSeen);
      statusSprite.setTextSize(1);
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString(totalStr, sw - 8, 30);

      int rowH = 20;
      int listTop = 42;
      int visRows = (sh - listTop - 12) / rowH;

      if (btLogCount > visRows) {
        int barH = max(8, (sh - listTop) * visRows / btLogCount);
        int barY = listTop + (sh - listTop - barH) * btScrollOffset / max(1, btLogCount - visRows);
        statusSprite.fillRect(sw - 4, listTop, 3, sh - listTop, display.color565(20, 20, 20));
        statusSprite.fillRect(sw - 4, barY, 3, barH, display.color565(0, 80, 160));
      }

      for (int i = 0; i < visRows; i++) {
        int idx = i + btScrollOffset;
        if (idx >= btLogCount) break;
        int y = listTop + i * rowH;
        if (y + rowH - 4 > sh - 12) break;

        bool isNew = (millis() - btLog[idx].lastSeenMs < (uint32_t)(BT_SCAN_DURATION_S * 2000));
        uint16_t nameColor = isNew ? COLOR_WHITE : COLOR_GRAY;

        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextSize(1);
        statusSprite.setTextColor(nameColor);
        char dispName[22];
        strncpy(dispName, btLog[idx].name[0] ? btLog[idx].name : btLog[idx].addr, 21);
        dispName[21] = '\0';
        statusSprite.drawString(dispName, 8, y + 4);

        char rssiStr[12];
        snprintf(rssiStr, sizeof(rssiStr), "%d", (int)btLog[idx].rssi);
        statusSprite.setTextDatum(TR_DATUM);
        statusSprite.setTextColor(
          btLog[idx].rssi > -60 ? COLOR_GREEN :
          btLog[idx].rssi > -80 ? COLOR_ORANGE : COLOR_RED);
        statusSprite.drawString(rssiStr, sw - 18, y + 4);

        if (idx < btLogCount - 1)
          statusSprite.drawFastHLine(8, y + rowH - 1, sw - 16, display.color565(20, 20, 20));
      }
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(display.color565(60, 60, 60));
      statusSprite.drawString("K1 hold:settings", sw - 8, sh - 10);
    }

  } else {
    // ── Portrait ─────────────────────────────────────────────────
    statusSprite.setTextDatum(TC_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_WHITE);
    statusSprite.drawString("BT SCAN", sw / 2, 6);

    const char* modeStr = btScanModeIdx == 0 ? "ACTIVE" : "PASSIVE";
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(display.color565(0, 100, 200));
    statusSprite.drawString(modeStr, sw / 2, 26);
    statusSprite.fillCircle(sw / 2 + 40, 29, 4, dotColor);

    if (btAdvEnabled) {
      statusSprite.setTextColor(COLOR_ORANGE);
      statusSprite.drawString("ADV", sw / 2 - 40, 29);
    }

    statusSprite.drawFastHLine(8, 40, sw - 16, display.color565(0, 60, 100));

    if (btLogCount == 0 && !btScanning) {
      // Idle — show SCAN button
      int bw = 90, bh = 30, bx = (sw - bw) / 2, by = sh / 2 - bh / 2 + 10;
      statusSprite.fillRoundRect(bx, by, bw, bh, 6, display.color565(0, 60, 120));
      statusSprite.drawRoundRect(bx, by, bw, bh, 6, display.color565(0, 120, 255));
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(2);
      statusSprite.setTextColor(COLOR_WHITE);
      statusSprite.drawString("SCAN", sw / 2, by + bh / 2 + 1);
    } else if (btLogCount == 0) {
      statusSprite.setTextDatum(MC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.drawString("Scanning...", sw / 2, sh / 2 + 8);
    } else {
      char totalStr[20];
      snprintf(totalStr, sizeof(totalStr), "Seen: %d", btTotalSeen);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(COLOR_GRAY);
      statusSprite.setTextDatum(TC_DATUM);
      statusSprite.drawString(totalStr, sw / 2, 46);
      statusSprite.drawFastHLine(8, 58, sw - 16, display.color565(25, 25, 25));

      int rowH    = 22;
      int listTop = 64;
      int visRows = (sh - listTop - 12) / rowH;

      if (btLogCount > visRows) {
        int barH = max(8, (sh - listTop) * visRows / btLogCount);
        int barY = listTop + (sh - listTop - barH) * btScrollOffset / max(1, btLogCount - visRows);
        statusSprite.fillRect(sw - 4, listTop, 3, sh - listTop, display.color565(20, 20, 20));
        statusSprite.fillRect(sw - 4, barY, 3, barH, display.color565(0, 80, 160));
      }

      for (int i = 0; i < visRows; i++) {
        int idx = i + btScrollOffset;
        if (idx >= btLogCount) break;
        int y = listTop + i * rowH;
        if (y + rowH - 4 > sh - 12) break;

        bool isNew = (millis() - btLog[idx].lastSeenMs < (uint32_t)(BT_SCAN_DURATION_S * 2000));
        uint16_t nameColor = isNew ? COLOR_WHITE : COLOR_GRAY;

        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextSize(1);
        statusSprite.setTextColor(nameColor);
        char dispName[20];
        strncpy(dispName, btLog[idx].name[0] ? btLog[idx].name : btLog[idx].addr, 19);
        dispName[19] = '\0';
        statusSprite.drawString(dispName, 8, y + 6);

        char rssiStr[12];
        snprintf(rssiStr, sizeof(rssiStr), "%d dBm", (int)btLog[idx].rssi);
        statusSprite.setTextDatum(TR_DATUM);
        statusSprite.setTextColor(
          btLog[idx].rssi > -60 ? COLOR_GREEN :
          btLog[idx].rssi > -80 ? COLOR_ORANGE : COLOR_RED);
        statusSprite.drawString(rssiStr, sw - 8, y + 6);

        if (idx < btLogCount - 1)
          statusSprite.drawFastHLine(8, y + rowH - 1, sw - 16, display.color565(20, 20, 20));
      }
      statusSprite.setTextDatum(TC_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(display.color565(60, 60, 60));
      statusSprite.drawString("K1 hold:settings", sw / 2, sh - 10);
    }
  }
}


// ================================================================
// Power management
// ================================================================

void resetActivity() {
  bool wasOff = displayOff || displayDimmed;
  lastActivityMs = millis();
  if (wasOff) {
    displayDimmed = false;
    displayOff    = false;
    digitalWrite(LCD_BACKLIGHT, HIGH);  // restore backlight
    lastFunction = -1;   // force full redraw on next loop
    if (btScanRequested && btInitialized && !btScanning && !btConnected) btStartScan();
  }
}

void checkPowerManagement(unsigned long msNow) {
  bool onUsb = (digitalRead(VIN_DETECT) == HIGH);
  // Use millis() directly — msNow is captured at loop() start but resetActivity() sets
  // lastActivityMs = millis() slightly later in the same iteration, causing unsigned
  // underflow (msNow - lastActivityMs wraps to ~UINT32_MAX) and immediate dim.
  unsigned long idle = millis() - lastActivityMs;

  uint32_t dimMs   = DIM_TIMEOUTS_MS[dimTimeoutIdx];
  uint32_t sleepMs = SLEEP_TIMEOUTS_MS[sleepTimeoutIdx];

  if (!displayOff && idle >= sleepMs) {
    digitalWrite(LCD_BACKLIGHT, LOW);   // backlight off (LCD_BACKLIGHT is I2C expander pin)
    displayOff    = true;
    displayDimmed = true;
    if (!onUsb && btInitialized) btStopScan();
  } else if (!displayDimmed && idle >= dimMs) {
    digitalWrite(LCD_BACKLIGHT, LOW);   // dim = backlight off (no PWM on I2C expander)
    displayDimmed = true;
    if (!onUsb && btInitialized) btStopScan();
  }
}

// ================================================================
// FUNCTION_MEDIA — matrix rain + artwork, sub-screen via swipe
// ================================================================

void initMedia() {
  if (mediaSubScreen == lastMediaSubScreen) return;
  stopBuzzer();
  lastMediaSubScreen = mediaSubScreen;
  switch (mediaSubScreen) {
    case 0: initMatrix(); break;
    case 1: initVader();  break;
    case 2: initObiwan(); break;
  }
}

void renderMedia() {
  switch (mediaSubScreen) {
    case 0: renderMatrix(); break;
    case 1: renderVader();  break;
    case 2: renderObiwan(); break;
  }
  // Sub-screen indicator: small dots at bottom-right corner
  int sw = display.width();
  int sh = display.height();
  for (int i = 0; i < 3; i++) {
    uint16_t c = (i == mediaSubScreen) ? COLOR_TEAL : display.color565(40,40,40);
    display.fillCircle(sw - 8, sh - 10 + (i - 1) * 7, 2, c);
  }
}

// ================================================================
// FUNCTION_BATTERY — device settings panel
// ================================================================

void applyDeviceSettings() {
  saveSettings();
  // Reset dim/sleep state so new timeouts take effect immediately
  lastActivityMs = millis();
  displayDimmed  = false;
  displayOff     = false;
  digitalWrite(LCD_BACKLIGHT, HIGH);
}

void renderBatterySettings() {
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  static const int ITEM_COUNT = 5;
  const char* labels[ITEM_COUNT] = {"DIM TIMEOUT", "SLEEP TIMEOUT", "LOW BAT SLEEP", "UI CLICKS", "RESET"};
  char values[ITEM_COUNT][16];
  snprintf(values[0], 16, "%s", DIM_TIMEOUT_LABELS[dimTimeoutIdx]);
  snprintf(values[1], 16, "%s", SLEEP_TIMEOUT_LABELS[sleepTimeoutIdx]);
  snprintf(values[2], 16, "%s", LOW_BAT_LABELS[lowBatIdx]);
  snprintf(values[3], 16, "%s", uiClickEnabled ? "ON" : "OFF");
  snprintf(values[4], 16, "%s", "\x10");  // right-arrow glyph

  int titleY = isLandscape ? 4 : 6;
  int divY   = isLandscape ? 22 : 26;
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_WHITE);
  statusSprite.drawString("DEVICE", sw / 2, titleY);
  statusSprite.drawFastHLine(8, divY, sw - 16, display.color565(0, 80, 160));

  int btnH = isLandscape ? 14 : 22;
  int btnY = sh - 6 - btnH;
  int sepY = btnY - 6;
  statusSprite.drawFastHLine(8, sepY, sw - 16, display.color565(40, 40, 40));
  int bw = (sw - 24) / 2;
  statusSprite.fillRect(8,       btnY, bw, btnH, display.color565(0, 60, 0));
  statusSprite.fillRect(16 + bw, btnY, bw, btnH, display.color565(60, 0, 0));
  statusSprite.drawRect(8,       btnY, bw, btnH, COLOR_GREEN);
  statusSprite.drawRect(16 + bw, btnY, bw, btnH, COLOR_RED);
  statusSprite.setTextSize(1);
  statusSprite.setTextDatum(MC_DATUM);
  statusSprite.setTextColor(COLOR_GREEN);
  statusSprite.drawString("APPLY",  8  + bw / 2,      btnY + btnH / 2);
  statusSprite.setTextColor(COLOR_RED);
  statusSprite.drawString("CANCEL", 16 + bw + bw / 2, btnY + btnH / 2);

  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);

  settingsScrollOffset = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);
  if (settingsCursor < settingsScrollOffset) settingsScrollOffset = settingsCursor;
  if (settingsCursor >= settingsScrollOffset + visRows)
    settingsScrollOffset = settingsCursor - visRows + 1;

  for (int i = 0; i < visRows; i++) {
    int idx     = i + settingsScrollOffset;
    if (idx >= ITEM_COUNT) break;
    int y       = startY + i * rowH;
    bool sel    = (idx == settingsCursor);
    bool isReset = (idx == ITEM_COUNT - 1);
    int textY   = y + (rowH - 8) / 2;

    if (isReset && sel)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(60, 15, 15));
    else if (sel)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(20, 30, 50));

    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(sel ? (isReset ? display.color565(255,80,80) : display.color565(0,140,255))
                                  : display.color565(60, 60, 60));
    statusSprite.drawString(sel ? ">" : " ", 6, textY);
    statusSprite.setTextColor(sel ? COLOR_WHITE : (isReset ? display.color565(100,40,40) : COLOR_GRAY));
    statusSprite.drawString(labels[idx], 18, textY);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.setTextColor(sel ? (isReset ? display.color565(255,80,80) : COLOR_ORANGE) : COLOR_GRAY);
    statusSprite.drawString(values[idx], sw - 8, textY);
  }

  statusSprite.pushSprite(0, SPRITE_Y);
}

void handleBatterySettingsTap(int16_t sx, int16_t sy) {
  int  sw          = statusSprite.width();
  int  sh          = statusSprite.height();
  bool isLandscape = sw > sh;
  int  sprite_y    = sy - SPRITE_Y;

  static const int ITEM_COUNT = 5;  // 4 items + RESET
  int divY     = isLandscape ? 22 : 26;
  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int btnH     = isLandscape ? 14 : 22;
  int btnY     = sh - 6 - btnH;
  int sepY     = btnY - 6;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);
  int offset   = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);

  for (int i = 0; i < visRows; i++) {
    int idx  = i + offset;
    int rowY = startY + i * rowH;
    if (sprite_y >= rowY && sprite_y < rowY + rowH) {
      if (settingsCursor == idx) onKey1Short();
      else                       settingsCursor = idx;
      return;
    }
  }
  if (sprite_y >= btnY && sprite_y < btnY + btnH) {
    if (sx < sw / 2) onKey1Long();
    else             onKey2Long();
  }
}

// ================================================================
// FUNCTION_WIFI — WiFi network scanner
// ================================================================

void applyWiFiSettings() {
  saveSettings();
}

void initWiFi() {
  display.fillScreen(BG_COLOR);
  renderHeader();
  wifiScanOffset = 0;
  if (wifiAutoScan && !wifiScanning) {
    wifiScanCount = 0;
    WiFi.scanNetworks(true);
    wifiScanning = true;
  }
}

// Settings: 2 items — DEBUG LOG, AUTO-SCAN
void renderWiFiSettings() {
  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  static const int ITEM_COUNT = 3;
  const char* labels[ITEM_COUNT] = {"DEBUG LOG", "AUTO-SCAN", "RESET"};
  char values[ITEM_COUNT][16];
  snprintf(values[0], 16, "%s", wifiDebugMode ? "ON" : "OFF");
  snprintf(values[1], 16, "%s", wifiAutoScan  ? "ON" : "OFF");
  snprintf(values[2], 16, "%s", "\x10");  // right-arrow glyph

  int titleY = isLandscape ? 4 : 6;
  int divY   = isLandscape ? 22 : 26;
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_WHITE);
  statusSprite.drawString("WIFI SETTINGS", sw / 2, titleY);
  statusSprite.drawFastHLine(8, divY, sw - 16, display.color565(0, 100, 80));

  int btnH = isLandscape ? 14 : 22;
  int btnY = sh - 6 - btnH;
  int sepY = btnY - 6;
  statusSprite.drawFastHLine(8, sepY, sw - 16, display.color565(40, 40, 40));
  int bw = (sw - 24) / 2;
  statusSprite.fillRect(8,       btnY, bw, btnH, display.color565(0, 60, 0));
  statusSprite.fillRect(16 + bw, btnY, bw, btnH, display.color565(60, 0, 0));
  statusSprite.drawRect(8,       btnY, bw, btnH, COLOR_GREEN);
  statusSprite.drawRect(16 + bw, btnY, bw, btnH, COLOR_RED);
  statusSprite.setTextSize(1);
  statusSprite.setTextDatum(MC_DATUM);
  statusSprite.setTextColor(COLOR_GREEN);
  statusSprite.drawString("APPLY",  8  + bw / 2,       btnY + btnH / 2);
  statusSprite.setTextColor(COLOR_RED);
  statusSprite.drawString("CANCEL", 16 + bw + bw / 2,  btnY + btnH / 2);

  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);

  settingsScrollOffset = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);
  if (settingsCursor < settingsScrollOffset)
    settingsScrollOffset = settingsCursor;
  if (settingsCursor >= settingsScrollOffset + visRows)
    settingsScrollOffset = settingsCursor - visRows + 1;

  for (int i = 0; i < visRows; i++) {
    int idx      = i + settingsScrollOffset;
    if (idx >= ITEM_COUNT) break;
    int y        = startY + i * rowH;
    bool sel     = (idx == settingsCursor);
    bool isReset = (idx == ITEM_COUNT - 1);
    int textY    = y + (rowH - 8) / 2;

    if (isReset && sel)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(60, 15, 15));
    else if (sel)
      statusSprite.fillRect(4, y, sw - 8, rowH - 2, display.color565(10, 40, 30));

    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(sel ? (isReset ? display.color565(255,80,80) : display.color565(0,200,140))
                                  : display.color565(60, 60, 60));
    statusSprite.drawString(sel ? ">" : " ", 6, textY);
    statusSprite.setTextColor(sel ? COLOR_WHITE : (isReset ? display.color565(100,40,40) : COLOR_GRAY));
    statusSprite.drawString(labels[idx], 18, textY);
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.setTextColor(sel ? (isReset ? display.color565(255,80,80) : COLOR_ORANGE) : COLOR_GRAY);
    statusSprite.drawString(values[idx], sw - 8, textY);

    if (i < visRows - 1 && idx < ITEM_COUNT - 1)
      statusSprite.drawFastHLine(8, y + rowH - 1, sw - 16, display.color565(25, 25, 25));
  }
}

void handleWiFiSettingsTap(int16_t sx, int16_t sy) {
  int  sw          = statusSprite.width();
  int  sh          = statusSprite.height();
  bool isLandscape = sw > sh;
  int  sprite_y    = sy - SPRITE_Y;

  static const int ITEM_COUNT = 3;  // 2 items + RESET
  int divY     = isLandscape ? 22 : 26;
  int startY   = divY + 4;
  int rowH     = isLandscape ? 20 : 32;
  int btnH     = isLandscape ? 14 : 22;
  int btnY     = sh - 6 - btnH;
  int sepY     = btnY - 6;
  int rowsArea = sepY - startY;
  int visRows  = constrain(rowsArea / rowH, 1, ITEM_COUNT);
  int offset   = constrain(settingsScrollOffset, 0, ITEM_COUNT - visRows);

  for (int i = 0; i < visRows; i++) {
    int idx  = i + offset;
    int rowY = startY + i * rowH;
    if (sprite_y >= rowY && sprite_y < rowY + rowH) {
      if (settingsCursor == idx) onKey1Short();
      else                       settingsCursor = idx;
      return;
    }
  }
  if (sprite_y >= btnY && sprite_y < btnY + btnH) {
    if (sx < sw / 2) onKey1Long();
    else             onKey2Long();
  }
}

void renderWiFi() {
  if (navState == NAV_SETTINGS) { renderWiFiSettings(); return; }
  if (navState == NAV_RESET)    { renderWiFiReset();    return; }

  // Poll async scan result
  if (wifiScanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      wifiScanCount = min(n, WIFI_SCAN_SIZE);
      for (int i = 0; i < wifiScanCount; i++) {
        strncpy(wifiScanLog[i].ssid, WiFi.SSID(i).c_str(), 32);
        wifiScanLog[i].ssid[32]  = '\0';
        wifiScanLog[i].rssi      = WiFi.RSSI(i);
        wifiScanLog[i].encType   = (uint8_t)WiFi.encryptionType(i);
        wifiScanLog[i].channel   = (uint8_t)WiFi.channel(i);
        strncpy(wifiScanLog[i].bssid, WiFi.BSSIDstr(i).c_str(), 17);
        wifiScanLog[i].bssid[17] = '\0';
      }
      WiFi.scanDelete();
      wifiScanning = false;
    } else if (n == WIFI_SCAN_FAILED) {
      wifiScanning  = false;
      wifiScanCount = 0;
    }
  }

  int sw = statusSprite.width();
  int sh = statusSprite.height();
  bool isLandscape = sw > sh;

  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // ── Title & connection status bar ─────────────────────────────
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(COLOR_TEAL);
  statusSprite.drawString("WIFI SCAN", sw / 2, isLandscape ? 4 : 6);

  char connStr[48];
  uint16_t connColor;
  if (WiFi.isConnected()) {
    snprintf(connStr, sizeof(connStr), "%s", WiFi.localIP().toString().c_str());
    connColor = COLOR_GREEN;
  } else if (wifiAuthFailed) {
    const char* failedSsid = (wifiUserSsid[0] != '\0') ? wifiUserSsid : secretSsid;
    snprintf(connStr, sizeof(connStr), "wrong password: %.20s", failedSsid);
    connColor = COLOR_RED;
  } else {
    snprintf(connStr, sizeof(connStr), "not connected");
    connColor = COLOR_GRAY;
  }
  statusSprite.setTextSize(1);
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextColor(connColor);
  statusSprite.drawString(connStr, sw / 2, isLandscape ? 18 : 22);

  // Auth-failure hint row
  if (wifiAuthFailed && !WiFi.isConnected()) {
    statusSprite.setTextSize(1);
    statusSprite.setTextDatum(TC_DATUM);
    statusSprite.setTextColor(display.color565(120, 40, 40));
    statusSprite.drawString("serial: wifi connect ssid ... pass ...", sw / 2, isLandscape ? 26 : 32);
  }

  int divY = isLandscape ? 28 : 36;
  statusSprite.drawFastHLine(4, divY, sw - 8, display.color565(0, 60, 50));

  // ── Scan button / scanning dot / results ─────────────────────
  int listTop  = divY + 6;
  bool blink   = (millis() / 500) % 2;

  if (!wifiScanning && wifiScanCount == 0) {
    // SCAN button
    int btnW = isLandscape ? 80 : 90;
    int btnH = isLandscape ? 22 : 26;
    int btnX = (sw - btnW) / 2;
    int btnY = listTop + (sh - listTop - btnH) / 2;
    statusSprite.fillRect(btnX, btnY, btnW, btnH, display.color565(0, 60, 50));
    statusSprite.drawRect(btnX, btnY, btnW, btnH, COLOR_TEAL);
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(2);
    statusSprite.setTextColor(COLOR_TEAL);
    statusSprite.drawString("SCAN", sw / 2, btnY + btnH / 2);
    statusSprite.setTextSize(1);
    statusSprite.setTextDatum(TC_DATUM);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.drawString("tap to start", sw / 2, btnY + btnH + 4);
  } else {
    // Scanning dot + count header
    int dotX = isLandscape ? 8 : sw / 2 - 4;
    int dotY = isLandscape ? listTop + 2 : listTop;
    if (wifiScanning) {
      statusSprite.fillCircle(dotX + 4, dotY + 4, 4, blink ? COLOR_TEAL : COLOR_BLACK);
      statusSprite.drawCircle(dotX + 4, dotY + 4, 4, COLOR_TEAL);
    } else {
      statusSprite.fillCircle(dotX + 4, dotY + 4, 4, COLOR_TEAL);
    }
    char cntStr[24];
    if (wifiScanning) snprintf(cntStr, sizeof(cntStr), "scanning...");
    else              snprintf(cntStr, sizeof(cntStr), "%d networks", wifiScanCount);
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(wifiScanning ? COLOR_TEAL : COLOR_WHITE);
    statusSprite.drawString(cntStr, dotX + 12, dotY);
    // Hint
    statusSprite.setTextDatum(TR_DATUM);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.drawString("K1:settings", sw - 2, dotY);

    // ── Network list ─────────────────────────────────────────────
    int rowH    = isLandscape ? 20 : (wifiDebugMode ? 30 : 22);
    int rowsTop = isLandscape ? listTop + 18 : listTop + 18;
    int visRows = max(0, (sh - rowsTop - 4) / rowH);

    wifiScanOffset = constrain(wifiScanOffset, 0, max(0, wifiScanCount - 1));

    // Scroll indicator
    if (wifiScanCount > visRows) {
      int barArea = sh - rowsTop;
      int barH    = max(6, barArea * visRows / wifiScanCount);
      int barY    = rowsTop + (barArea - barH) * wifiScanOffset / max(1, wifiScanCount - visRows);
      statusSprite.fillRect(sw - 3, rowsTop, 2, barArea, display.color565(20, 20, 20));
      statusSprite.fillRect(sw - 3, barY,    2, barH,    COLOR_TEAL);
    }

    for (int i = 0; i < visRows; i++) {
      int idx = i + wifiScanOffset;
      if (idx >= wifiScanCount) break;
      const WiFiScanEntry& e = wifiScanLog[idx];
      int rowY = rowsTop + i * rowH;

      // Alternating row background
      if (i % 2 == 0)
        statusSprite.fillRect(0, rowY, sw - 4, rowH - 1, display.color565(8, 12, 10));

      // Signal strength → bar color
      uint16_t sigColor = (e.rssi >= -60) ? COLOR_GREEN :
                          (e.rssi >= -75) ? COLOR_ORANGE : COLOR_RED;

      // Lock icon (3×6 px) for secured networks
      bool secured = (e.encType != 0);
      int lockX = 4;
      if (secured) {
        statusSprite.fillRect(lockX + 1, rowY + (rowH/2) - 1, 5, 4, sigColor);
        statusSprite.drawFastHLine(lockX + 2, rowY + (rowH/2) - 2, 3, sigColor);
        statusSprite.drawFastHLine(lockX + 2, rowY + (rowH/2) - 3, 3, sigColor);
      }

      // SSID (truncated)
      char truncSsid[22];
      int maxCh = isLandscape ? 20 : 16;
      strncpy(truncSsid, e.ssid[0] ? e.ssid : "(hidden)", maxCh);
      truncSsid[maxCh] = '\0';
      statusSprite.setTextDatum(TL_DATUM);
      statusSprite.setTextSize(1);
      statusSprite.setTextColor(secured ? COLOR_GRAY : COLOR_WHITE);
      statusSprite.drawString(truncSsid, 14, rowY + 2);

      // RSSI
      char rssiStr[8]; snprintf(rssiStr, sizeof(rssiStr), "%ddBm", e.rssi);
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextColor(sigColor);
      statusSprite.drawString(rssiStr, sw - 5, rowY + 2);

      // Debug row: channel + BSSID
      if (wifiDebugMode && rowH >= 28) {
        char dbg[32];
        snprintf(dbg, sizeof(dbg), "ch%d  %s", e.channel, e.bssid);
        statusSprite.setTextDatum(TL_DATUM);
        statusSprite.setTextColor(display.color565(50, 80, 70));
        statusSprite.drawString(dbg, 14, rowY + 13);
      }

      // Row separator
      if (i < visRows - 1)
        statusSprite.drawFastHLine(4, rowY + rowH - 1, sw - 8, display.color565(20, 28, 24));
    }
  }

  // Settings hint at bottom when not scanning
  if (!wifiScanning && wifiScanCount == 0) return;
  statusSprite.setTextDatum(BL_DATUM);
  statusSprite.setTextSize(1);
  statusSprite.setTextColor(display.color565(30, 50, 40));
  statusSprite.drawString("K1 hold:settings", 4, sh - 1);
}

// ================================================================
// Reset pages — shared helper + per-function render & tap handlers
// ================================================================

// Shared reset page renderer. btnLabels[] has btnCount entries.
// Tapped buttons show "DONE" for 1 s via resetFeedbackMs / resetFeedbackBtn.
static void renderResetPageCommon(const char* title, uint16_t accentCol,
                                   const char** btnLabels, int btnCount) {
  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLand = sw > sh;
  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  // Title bar
  int titleH = isLand ? 22 : 28;
  statusSprite.fillRect(0, 0, sw, titleH, display.color565(50, 10, 10));
  statusSprite.drawFastHLine(0, titleH, sw, accentCol);
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(accentCol);
  statusSprite.drawString(title, sw / 2, isLand ? 3 : 5);

  // Button area
  int pad   = 4;
  int listY = titleH + pad + 2;
  int areaH = sh - listY - pad;
  int btnH  = max(16, areaH / btnCount);

  for (int k = 0; k < btnCount; k++) {
    int y    = listY + k * btnH;
    bool done = (resetFeedbackBtn == k && millis() - resetFeedbackMs < 1000);
    uint16_t bgCol  = done ? display.color565(0, 55, 0)   : display.color565(55, 12, 12);
    uint16_t brdCol = done ? COLOR_GREEN                   : accentCol;
    uint16_t txtCol = done ? COLOR_GREEN                   : COLOR_WHITE;
    statusSprite.fillRect(pad, y + 2, sw - pad * 2, btnH - 4, bgCol);
    statusSprite.drawRect(pad, y + 2, sw - pad * 2, btnH - 4, brdCol);
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(txtCol);
    statusSprite.drawString(done ? "DONE" : btnLabels[k], sw / 2, y + btnH / 2);
  }

  // Hint
  statusSprite.setTextDatum(BL_DATUM);
  statusSprite.setTextSize(1);
  statusSprite.setTextColor(display.color565(60, 20, 20));
  statusSprite.drawString("tap title = back", 4, sh - 1);
}

// Shared tap handler for reset pages. Calls actionFn(buttonIndex) on hit.
// titleH must match renderResetPageCommon.
static void handleResetPageTap(int16_t sx, int16_t sy, int btnCount,
                                void (*actionFn)(int)) {
  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLand = sw > sh;
  int titleH = isLand ? 22 : 28;
  int sprite_y = sy - SPRITE_Y;

  if (sprite_y < titleH) { navState = NAV_SETTINGS; resetFeedbackMs = 0; resetFeedbackBtn = -1; return; }

  int pad   = 4;
  int listY = titleH + pad + 2;
  int areaH = sh - listY - pad;
  int btnH  = max(16, areaH / btnCount);

  for (int k = 0; k < btnCount; k++) {
    int y = listY + k * btnH;
    if (sprite_y >= y + 2 && sprite_y < y + btnH - 2) {
      resetFeedbackMs  = millis();
      resetFeedbackBtn = k;
      actionFn(k);
      return;
    }
  }
}

// ── LoRa reset ────────────────────────────────────────────────────
static void doLoraReset(int btn) {
  // btn 0: reset all LoRa settings to defaults
  loraPresetIdx = 0; loraFreqIdx = 0; loraAutoReply = false; loraDedup = true;
  saveSettings(); applyLoraSettings();
}

void renderLoraReset() {
  static const char* btns[] = {"RESET ALL SETTINGS"};
  renderResetPageCommon("LORA RESET", COLOR_TEAL, btns, 1);
}

void handleLoraResetTap(int16_t sx, int16_t sy) {
  handleResetPageTap(sx, sy, 1, doLoraReset);
}

// ── BT reset ──────────────────────────────────────────────────────
static void doBTReset(int btn) {
  if (btn == 0) {
    // Clear scan log
    btLogCount = 0; btScrollOffset = 0; btTotalSeen = 0; btSelectedAddr[0] = '\0';
  } else {
    // Reset all BT settings to defaults
    btScanModeIdx = 0; btRssiFilterIdx = 3; btDebugMode = false;
    btAdvEnabled = true; btStartupEnabled = true;
    saveSettings(); applyBTSettings();
  }
}

void renderBTReset() {
  static const char* btns[] = {"CLEAR SCAN LOG", "RESET ALL SETTINGS"};
  renderResetPageCommon("BT RESET", display.color565(0, 140, 255), btns, 2);
}

void handleBTResetTap(int16_t sx, int16_t sy) {
  handleResetPageTap(sx, sy, 2, doBTReset);
}

// ── WiFi reset ────────────────────────────────────────────────────
static void doWiFiReset(int btn) {
  if (btn == 0) {
    // Clear stored credentials
    wifiUserSsid[0] = '\0'; wifiUserPass[0] = '\0';
    saveSettings();
  } else {
    // Reset all WiFi settings to defaults
    wifiDebugMode = false; wifiAutoScan = false;
    saveSettings();
  }
}

void renderWiFiReset() {
  static const char* btns[] = {"CLEAR CREDENTIALS", "RESET ALL SETTINGS"};
  renderResetPageCommon("WIFI RESET", display.color565(0, 200, 140), btns, 2);
}

void handleWiFiResetTap(int16_t sx, int16_t sy) {
  handleResetPageTap(sx, sy, 2, doWiFiReset);
}

// ── Battery / device reset ────────────────────────────────────────
static void doBatteryReset(int btn) {
  // btn 0: reset all device settings to defaults
  dimTimeoutIdx = 0; sleepTimeoutIdx = 0; lowBatIdx = 0; uiClickEnabled = true;
  saveSettings(); applyDeviceSettings();
}

void renderBatteryReset() {
  static const char* btns[] = {"RESET ALL TIMEOUTS"};
  renderResetPageCommon("DEVICE RESET", display.color565(0, 140, 255), btns, 1);
}

void handleBatteryResetTap(int16_t sx, int16_t sy) {
  handleResetPageTap(sx, sy, 1, doBatteryReset);
}

// ================================================================
// FUNCTION_IR — IR Remote Control (LittleFS .ir file based)
// ================================================================

static IRFileProto irProtoFromStr(const char* s) {
  if (!strcasecmp(s,"NEC"))                          return IRP_NEC;
  if (!strcasecmp(s,"SAMSUNG")||!strcasecmp(s,"SAMSUNG32")) return IRP_SAMSUNG;
  if (!strcasecmp(s,"SIRC12")||!strcasecmp(s,"SIRC-12"))    return IRP_SIRC12;
  if (!strcasecmp(s,"SIRC15")||!strcasecmp(s,"SIRC-15"))    return IRP_SIRC15;
  if (!strcasecmp(s,"SIRC20")||!strcasecmp(s,"SIRC-20"))    return IRP_SIRC20;
  if (!strcasecmp(s,"RC5"))                          return IRP_RC5;
  if (!strcasecmp(s,"RC6"))                          return IRP_RC6;
  if (!strcasecmp(s,"LG"))                           return IRP_LG;
  if (!strcasecmp(s,"JVC"))                          return IRP_JVC;
  return IRP_UNKNOWN;
}

static uint32_t irParseHexLE(const char* s) {
  // Parses "30 00 00 00" as little-endian hex bytes → 0x00000030
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) {
    while (*s == ' ') s++;
    if (!*s) break;
    char* end;
    uint8_t b = (uint8_t)strtoul(s, &end, 16);
    v |= ((uint32_t)b << (i * 8));
    s = end;
  }
  return v;
}

static void irScanDir(const char* dirPath) {
  File dir = LittleFS.open(dirPath);
  if (!dir || !dir.isDirectory()) { dir.close(); return; }
  while (irFileCount < IR_MAX_FILES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    // entry.name() returns bare name; build full path manually
    char pathBuf[64];
    snprintf(pathBuf, sizeof(pathBuf), "%s/%s", dirPath, entry.name());
    bool isDir = entry.isDirectory();
    entry.close();
    if (isDir) {
      irScanDir(pathBuf);
    } else {
      const char* slash    = strrchr(pathBuf, '/');
      const char* fileName = slash ? slash + 1 : pathBuf;
      int len = strlen(fileName);
      if (len > 3 && !strcasecmp(fileName + len - 3, ".ir")) {
        strlcpy(irFiles[irFileCount].path, pathBuf, sizeof(irFiles[0].path));
        strlcpy(irFiles[irFileCount].name, fileName, sizeof(irFiles[0].name));
        int nlen = strlen(irFiles[irFileCount].name);
        if (nlen > 3) irFiles[irFileCount].name[nlen - 3] = '\0'; // strip .ir
        irFileCount++;
      }
    }
  }
  dir.close();
}

void irScanFiles() {
  irFileCount = 0;
  memset(irFiles, 0, sizeof(irFiles));
  irScanDir("/irdb");
}

void irOpenDir(const char* path) {
  char p[64];
  strlcpy(p, path, sizeof(p));   // copy before memset wipes irDir[] (path may point into it)
  irDirCount = 0;
  memset(irDir, 0, sizeof(irDir));
  strlcpy(irBrowsePath, p, sizeof(irBrowsePath));
  irListOff = 0;
  File dir = LittleFS.open(p);
  if (!dir || !dir.isDirectory()) { dir.close(); return; }
  while (irDirCount < IR_DIR_MAX) {
    File entry = dir.openNextFile();
    if (!entry) break;
    char bare[40];
    strlcpy(bare, entry.name(), sizeof(bare));
    bool isD = entry.isDirectory();
    entry.close();
    IRDirEntry& e = irDir[irDirCount];
    e.isDir = isD;
    snprintf(e.path, sizeof(e.path), "%s/%s", p, bare);
    strlcpy(e.name, bare, sizeof(e.name));
    if (!isD) {
      int n = strlen(e.name);
      if (n > 3 && !strcasecmp(e.name + n - 3, ".ir")) e.name[n - 3] = '\0';
    }
    irDirCount++;
  }
  dir.close();
}

void irLoadDevice(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) { irBtnCount = 0; irLoadedPath[0] = '\0'; irLoadedName[0] = '\0'; return; }
  irBtnCount = 0;
  strlcpy(irLoadedPath, path, sizeof(irLoadedPath));
  const char* sl = strrchr(path, '/');
  strlcpy(irLoadedName, sl ? sl + 1 : path, sizeof(irLoadedName));
  int nl = strlen(irLoadedName);
  if (nl > 3 && !strcasecmp(irLoadedName + nl - 3, ".ir")) irLoadedName[nl - 3] = '\0';
  irSelectedIdx = -1;
  for (int i = 0; i < irFileCount; i++)
    if (!strcmp(irFiles[i].path, path)) { irSelectedIdx = i; break; }
  IRButton* btn = nullptr;

  while (f.available()) {
    // Read key (up to colon)
    char key[24]; int ki = 0; bool gotKey = false; char c;
    while (f.available()) {
      c = f.read();
      if (c == '\n') break;
      if (c == '\r') continue;
      if (c == '#') { while (f.available()) { c = f.read(); if (c=='\n') break; } break; }
      if (c == ':') { key[ki] = '\0'; gotKey = true; break; }
      if (ki < (int)sizeof(key)-1) key[ki++] = c;
    }
    if (!gotKey) continue;

    bool isDataLine = (!strcmp(key,"data") && btn && btn->proto == IRP_RAW && btn->rawLen == 0);

    if (!isDataLine) {
      // Read value into a small buffer
      char val[100]; int vi = 0;
      while (f.available()) {
        c = f.read(); if (c == '\n') break; if (c == '\r') continue;
        if (vi < (int)sizeof(val)-1) val[vi++] = c;
      }
      val[vi] = '\0';
      char* v = val; while (*v == ' ') v++;
      int vlen = strlen(v);
      while (vlen > 0 && (v[vlen-1]==' '||v[vlen-1]=='\r'||v[vlen-1]=='\n')) vlen--;
      v[vlen] = '\0';

      if (!strcmp(key,"name") && irBtnCount < IR_MAX_BUTTONS) {
        btn = &irBtns[irBtnCount++];
        memset(btn, 0, sizeof(*btn));
        strlcpy(btn->label, v, sizeof(btn->label));
      } else if (!strcmp(key,"type") && btn) {
        if (!strcmp(v,"raw")) btn->proto = IRP_RAW;
      } else if (!strcmp(key,"protocol") && btn) {
        btn->proto = irProtoFromStr(v);
      } else if (!strcmp(key,"address") && btn) {
        btn->address = irParseHexLE(v);
      } else if (!strcmp(key,"command") && btn) {
        btn->command = irParseHexLE(v);
      } else if (!strcmp(key,"frequency") && btn) {
        btn->rawFreq = (uint16_t)atoi(v);
      }
    } else {
      // Stream-parse raw timing data; stop at first inter-frame gap (>15000 µs)
      btn->rawLen = 0;
      uint32_t acc = 0; bool inNum = false;
      while (f.available()) {
        c = f.read();
        if (c == '\n' || c == '\r') {
          if (inNum) {
            if (acc <= 15000 && btn->rawLen < IR_MAX_RAW_LEN)
              btn->rawData[btn->rawLen++] = (uint16_t)acc;
          }
          if (c == '\n') break;
          continue;
        }
        if (c >= '0' && c <= '9') {
          acc = inNum ? acc * 10 + (uint32_t)(c - '0') : (uint32_t)(c - '0');
          inNum = true;
        } else if (inNum) {
          inNum = false;
          if (acc > 15000) {
            // Drain rest of this data line
            while (f.available()) { c = f.read(); if (c == '\n') break; }
            break;
          }
          if (btn->rawLen < IR_MAX_RAW_LEN)
            btn->rawData[btn->rawLen++] = (uint16_t)acc;
          acc = 0;
        }
      }
    }
  }
  f.close();
}

void irSendButton(int btnIdx) {
  if (!irLoadedPath[0] || btnIdx < 0 || btnIdx >= irBtnCount) return;
  IRButton& b = irBtns[btnIdx];
  irTxMs = millis();
  switch (b.proto) {
    case IRP_NEC:     IrSender.sendNEC(b.address, b.command & 0xFF, 0);                    break;
    case IRP_SAMSUNG: IrSender.sendSamsung(b.address & 0xFF, b.command & 0xFF, 0);         break;
    case IRP_SIRC12:  IrSender.sendSony(b.address, b.command, 2, 12);                      break;
    case IRP_SIRC15:  IrSender.sendSony(b.address, b.command, 2, 15);                      break;
    case IRP_SIRC20:  IrSender.sendSony(b.address, b.command, 2, 20);                      break;
    case IRP_RC5:     IrSender.sendRC5(b.address & 0x1F, b.command & 0x3F, 0);             break;
    case IRP_RC6:     IrSender.sendRC6(b.address & 0xFF, b.command & 0xFF, 0);             break;
    case IRP_LG:      IrSender.sendLG(b.address & 0xFF, b.command & 0xFF, 0);              break;
    case IRP_JVC:     IrSender.sendJVC((uint8_t)(b.address&0xFF),(uint8_t)(b.command&0xFF),0); break;
    case IRP_RAW:
      if (b.rawLen >= 8) {
        uint8_t freqKHz = b.rawFreq > 0 ? (uint8_t)(b.rawFreq / 1000) : 38;
        for (int r = 0; r < 3; r++) {
          IrSender.sendRaw(b.rawData, b.rawLen, freqKHz);
          if (r < 2) delay(45);
        }
      }
      break;
    default: break;
  }
}

void initIR() {
  IrSender.begin(IR_SEND_PIN);
  display.fillScreen(BG_COLOR);
  renderHeader();
  irBtnPageOff = 0;
  irOpenDir("/irdb");
  if (irSavedPath[0] != '\0') {
    irLoadDevice(irSavedPath);
    irLevel = IR_LEVEL_REMOTE;
  } else {
    irLevel = IR_LEVEL_LIST;
  }
}

// ── Shared: draw a list title bar with optional back arrow ────────
static void irDrawTitle(const char* title, bool showBack, int sw, int titleH, bool isLand) {
  uint16_t orange = display.color565(200,80,0);
  statusSprite.fillRect(0, 0, sw, titleH, display.color565(20,10,0));
  statusSprite.drawFastHLine(0, titleH, sw, display.color565(80,30,0));
  if (showBack) {
    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(display.color565(120,50,0));
    statusSprite.drawString("< BACK", 4, (titleH-8)/2);
  }
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(2);
  statusSprite.setTextColor(orange);
  statusSprite.drawString(title, sw/2, isLand ? 3 : 4);
}


// ── Draw one button cell ──────────────────────────────────────────
static void irDrawBtn(int bx, int by, int bw, int bh, const char* label, uint16_t col) {
  bool flashing = irFlashMs > 0 && (millis() - irFlashMs) < 200 &&
                  irFlashX >= bx && irFlashX < bx+bw &&
                  irFlashY >= by && irFlashY < by+bh;
  uint16_t bg   = flashing ? display.color565(60,30,5) : display.color565(12,6,0);
  uint16_t edge = flashing ? display.color565(255,220,100) : col;
  statusSprite.fillRoundRect(bx, by, bw, bh, 3, bg);
  statusSprite.drawRoundRect(bx, by, bw, bh, 3, edge);
  if (label && *label) {
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(flashing ? display.color565(255,220,100) : col);
    statusSprite.drawString(label, bx+bw/2, by+bh/2);
  }
}

// ── Level 0: Directory browser ────────────────────────────────────
static void renderIRDir() {
  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLand = sw > sh;
  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  int titleH = isLand ? 20 : 24;
  bool atRoot = (strcmp(irBrowsePath, "/irdb") == 0);
  const char* sl = strrchr(irBrowsePath, '/');
  const char* dirLabel = (sl && !atRoot) ? sl + 1 : "IR REMOTE";
  irDrawTitle(dirLabel, !atRoot, sw, titleH, isLand);

  if (irDirCount == 0) {
    statusSprite.setTextDatum(MC_DATUM);
    statusSprite.setTextSize(1);
    statusSprite.setTextColor(COLOR_GRAY);
    statusSprite.drawString(atRoot ? "No .ir files in /irdb/" : "Empty", sw/2, sh/2);
    return;
  }

  int rowH     = isLand ? 18 : 20;
  int listTop  = titleH + 4;
  int listArea = sh - listTop - 4;
  int visRows  = max(1, listArea / rowH);
  irListOff    = constrain(irListOff, 0, max(0, irDirCount - visRows));

  if (irDirCount > visRows) {
    int barH = max(6, listArea * visRows / irDirCount);
    int barY = listTop + (listArea - barH) * irListOff / max(1, irDirCount - visRows);
    statusSprite.fillRect(sw-3, listTop, 2, listArea, display.color565(20,20,20));
    statusSprite.fillRect(sw-3, barY,    2, barH,     display.color565(180,70,0));
  }

  uint16_t colDir  = display.color565(220,150,20);
  uint16_t colFile = display.color565(180,100,20);
  uint16_t colSel  = COLOR_WHITE;
  uint16_t colArrow = display.color565(60,30,5);

  for (int i = 0; i < visRows; i++) {
    int n    = i + irListOff;
    if (n >= irDirCount) break;
    int rowY  = listTop + i * rowH;
    int textY = rowY + (rowH-8)/2;
    bool isSel = !irDir[n].isDir && !strcmp(irDir[n].path, irLoadedPath);

    if (isSel) statusSprite.fillRect(2, rowY, sw-4, rowH-1, display.color565(25,12,0));

    statusSprite.setTextDatum(TL_DATUM);
    statusSprite.setTextSize(1);

    if (irDir[n].isDir) {
      statusSprite.setTextColor(colDir);
      char label[44]; snprintf(label, sizeof(label), "> %s", irDir[n].name);
      statusSprite.drawString(label, 8, textY);
    } else {
      statusSprite.setTextColor(isSel ? colSel : colFile);
      statusSprite.drawString(irDir[n].name, 8, textY);
      statusSprite.setTextDatum(TR_DATUM);
      statusSprite.setTextColor(isSel ? display.color565(200,80,0) : colArrow);
      statusSprite.drawString(isSel ? "OPEN>" : ">", sw-6, textY);
    }

    if (i < visRows-1 && n < irDirCount-1)
      statusSprite.drawFastHLine(4, rowY+rowH-1, sw-8, display.color565(20,10,0));
  }
}

// ── Level 1: Remote button grid ──────────────────────────────────
static void renderIRRemote() {
  if (!irLoadedPath[0] || irBtnCount == 0) { irLevel = IR_LEVEL_LIST; return; }

  int sw = statusSprite.width(), sh = statusSprite.height();
  bool isLand = sw > sh;
  statusSprite.fillSprite(COLOR_BLACK);
  statusSprite.setFont(&fonts::Font0);

  int titleH = isLand ? 22 : 26;
  irDrawTitle(irLoadedName, true, sw, titleH, isLand);

  // TX indicator: blinking red dot for 500 ms
  uint32_t txAge = irTxMs ? (uint32_t)(millis() - irTxMs) : 0xFFFFFFFFu;
  if (txAge < 500 && (txAge / 100) % 2 == 0)
    statusSprite.fillCircle(sw - 8, titleH/2, 4, display.color565(220,0,0));

  int pad   = 3;
  int areaY = titleH + pad;
  int cols  = 2;
  int btnH  = isLand ? 22 : 24;
  int btnW  = (sw - pad * (cols + 1)) / cols;
  int areaH = sh - areaY - pad;
  int visRows   = max(1, areaH / (btnH + pad));
  int totalRows = (irBtnCount + cols - 1) / cols;
  irBtnPageOff  = constrain(irBtnPageOff, 0, max(0, totalRows - visRows));

  if (totalRows > visRows) {
    int barH = max(6, areaH * visRows / totalRows);
    int barY = areaY + (areaH - barH) * irBtnPageOff / max(1, totalRows - visRows);
    statusSprite.fillRect(sw-3, areaY, 2, areaH, display.color565(20,20,20));
    statusSprite.fillRect(sw-3, barY,  2, barH,  display.color565(180,70,0));
  }

  uint16_t btnCol = display.color565(150,70,10);
  for (int row = irBtnPageOff; row < irBtnPageOff + visRows && row < totalRows; row++) {
    int rowY = areaY + (row - irBtnPageOff) * (btnH + pad);
    for (int col = 0; col < cols; col++) {
      int idx = row * cols + col;
      if (idx >= irBtnCount) break;
      int bx = pad + col * (btnW + pad);
      irDrawBtn(bx, rowY, btnW, btnH, irBtns[idx].label, btnCol);
    }
  }

  statusSprite.setTextDatum(BL_DATUM);
  statusSprite.setTextSize(1);
  statusSprite.setTextColor(display.color565(30,15,0));
  statusSprite.drawString("tap title=list", 4, sh-1);
}

void renderIR() {
  if (irLevel == IR_LEVEL_LIST) renderIRDir();
  else                          renderIRRemote();
}

// ================================================================
// Battery management
// ================================================================

// Piecewise linear LiPo discharge curve: voltage → percentage
float voltageToPercent(float v) {
  static const float vt[] = {3.00f, 3.30f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  static const float pt[] = {  0.0f,  5.0f, 10.0f, 20.0f, 35.0f, 50.0f, 65.0f, 80.0f, 90.0f,100.0f};
  const int N = 10;
  if (v <= vt[0])     return 0.0f;
  if (v >= vt[N - 1]) return 100.0f;
  for (int i = 0; i < N - 1; i++) {
    if (v < vt[i + 1]) {
      float t = (v - vt[i]) / (vt[i + 1] - vt[i]);
      return pt[i] + t * (pt[i + 1] - pt[i]);
    }
  }
  return 100.0f;
}

void batteryCheck() {
  // Read AW32001 status first — it is the authoritative source for charge state.
  // VIN_DETECT monitors USB-C VBUS (the Nesso N1 has no separate VIN pin).
  // AW32001 charge status is a belt-and-suspenders fallback for VBUS detection.
  chargeStatus = battery.getChargeStatus();

  bool externalPower = (digitalRead(VIN_DETECT) == HIGH) ||
                       (chargeStatus != NessoBattery::NOT_CHARGING);

  if (!onExternalPower && externalPower) {
    debugln("External power connected!");
    onExternalPower = true;
  } else if (onExternalPower && !externalPower) {
    debugln("External power disconnected!");
    onExternalPower = false;
  }

  // The AW32001 manages charge termination and recharge autonomously.
  // Do NOT call setChargeEnable() here — overriding the IC causes the BQ27220
  // fuel gauge to see unexpected charge interruptions and snap to wrong SoC values.
  // batteryCharging is a UI-only flag derived from the IC's reported status.
  batteryCharging = (chargeStatus == NessoBattery::CHARGING ||
                     chargeStatus == NessoBattery::PRE_CHARGE);

  debug("Charge status: ");
  switch (chargeStatus) {
    case NessoBattery::CHARGING:     debugln("CHARGING");     break;
    case NessoBattery::FULL_CHARGE:  debugln("FULL_CHARGE");  break;
    case NessoBattery::NOT_CHARGING: debugln("NOT_CHARGING"); break;
    case NessoBattery::PRE_CHARGE:   debugln("PRE_CHARGE");   break;
  }

  // Low battery protection: turn off display and halt when critically low
  if (!onExternalPower && lowBatIdx < 2) {
    uint8_t thresh = LOW_BAT_THRESHOLDS[lowBatIdx];
    if (voltagePercent > 0.5f && voltagePercent < (float)thresh) {
      serialWritelnAll("[POWER] Critical battery — display off. Connect USB to resume.");
      digitalWrite(LCD_BACKLIGHT, LOW);
      displayOff = true;
      displayDimmed = true;
      if (btInitialized) btStopScan();
    }
  }
}
