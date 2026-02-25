/*
 * UVAD Multi-Device Controller
 * ============================
 * Same firmware for every ESP32-S3 unit.
 *   - First unit powered on  → Coordinator (wins election as sole candidate;
 *     ties broken by lowest MAC)
 *   - Subsequent units       → Client      (ESP-NOW motor node)
 *
 * Phone / tablet connects to the "UVAD-AP" WiFi network and opens
 * 192.168.4.1 in a browser. All discovered motors appear as cards
 * and can be controlled from that single page.
 *
 * Failsafe: if the coordinator disappears, clients enter an election
 * (lowest MAC wins) and the winner promotes itself to coordinator.
 *
 * Dependencies (same as single-device UVAD):
 *   ESPAsyncWebServer  – https://github.com/ESP32Async/ESPAsyncWebServer
 *   AsyncTCP           – https://github.com/ESP32Async/AsyncTCP
 *   TMC2209            – https://github.com/janelia-arduino/TMC2209
 *   Preferences, Wire  – built-in
 *
 * Build: Arduino IDE → Board = ESP32S3 Dev Module, USB CDC on Boot = Enabled
 *
 ******* Software Version 2.0 — Multi-Device ********
 */

// =====================================================================
//  INCLUDES
// =====================================================================
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_idf_version.h>
#include <ESPAsyncWebServer.h>   // ESP32Async version
#include <AsyncTCP.h>            // ESP32Async version
#include <TMC2209.h>             // janelia-arduino
#include <Preferences.h>
#include <Wire.h>

#include "espnow_protocol.h"
#include "index_html.h"

// =====================================================================
//  PIN DEFINITIONS  (same as UVAD / PD-Stepper hardware)
// =====================================================================
// TMC2209 Stepper Driver
#define TMC_EN  21
#define STEP    5
#define DIR     6
#define MS1     1
#define MS2     2
#define SPREAD  7
#define TMC_TX  17
#define TMC_RX  18
#define DIAG    16
#define INDEX   11

// PD Trigger (CH224K)
#define PG      15   // Power-good (active LOW)
#define CFG1    38
#define CFG2    48
#define CFG3    47

// Misc
#define VBUS    4
#define NTC     7    // NOTE: shares GPIO 7 with SPREAD; neither is used in this sketch
#define LED1    10
#define LED2    12
#define SW1     35
#define SW2     36
#define SW3     37
#define AUX1    14
#define AUX2    13

// =====================================================================
//  CONFIGURATION
// =====================================================================
#define FIRMWARE_VERSION "2.0-multi"

const long   SERIAL_BAUD_RATE      = 115200;
const uint8_t RUN_CURRENT_PERCENT  = 100;

// Assembly / motion units
const int   MOTOR_STEPS_PER_REV    = 200;
const long  COUNTS_PER_FULL_STEP   = 256;
float       GEAR_RATIO             = 30.0;
const float SMALL_ANGLE_DEG        = 22.5;
const float LARGE_ANGLE_DEG        = 45.0;

long countsPerOutputRev() {
  return (long)((long)MOTOR_STEPS_PER_REV * (long)COUNTS_PER_FULL_STEP * GEAR_RATIO);
}
long countsFromDegrees(float deg) {
  return (long)((deg / 360.0) * (double)countsPerOutputRev());
}

// =====================================================================
//  ROLE MANAGEMENT
// =====================================================================
enum Role : uint8_t { ROLE_UNDECIDED, ROLE_COORDINATOR, ROLE_CLIENT, ROLE_ELECTING };
volatile Role currentRole = ROLE_UNDECIDED;

// =====================================================================
//  OBJECTS
// =====================================================================
Preferences     preferences;
AsyncWebServer  server(80);
TMC2209         stepper_driver;
HardwareSerial &serial_stream = Serial2;

// =====================================================================
//  DEVICE REGISTRY  (coordinator maintains; client only has "self")
// =====================================================================
struct DeviceEntry {
  bool     used;
  uint8_t  mac[6];
  char     name[DEVICE_NAME_LEN];
  bool     online;
  bool     isSelf;
  unsigned long lastSeen;
  // Cached sensor values
  int16_t  voltage_mv;
  uint8_t  pgStatus;
  uint8_t  driverStatus;
  uint8_t  stallStatus;
  int32_t  angle_deg;
};
DeviceEntry devices[MAX_DEVICES];

char    myDeviceName[DEVICE_NAME_LEN];
uint8_t myMac[6];

// =====================================================================
//  MOTOR GLOBALS  (same as single-device UVAD)
// =====================================================================
int  set_speed      = 0;
bool PGState        = false;
bool enabledState   = false;
bool stepState      = false;

signed long setPoint         = 0;
signed long CurrentPosition  = 0;
unsigned long lastStep       = 0;

int  buttonSpeed = 0;

// Encoder
#define AS5600_ADDRESS 0x36
signed long   total_encoder_counts = 0;
unsigned long lastEncRead          = 0;
signed long   encoder_offset       = 0;
bool          encoder_offset_set   = false;

int mainFreq = 10; // 100 Hz tick for slower tasks

// Button debounce
bool incButtonState   = HIGH;
bool decButtonState   = HIGH;
bool resetButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Voltage
const float DIV_RATIO = 0.1189427313; // 20 k + 2.7 k divider

// NVS motor settings (strings, matching existing keys)
String enabled1       = "enabled";
String setVoltage     = "5";
String microsteps     = "1";
String current        = "30";
String stallThreshold = "10";
String standstillMode = "FREEWHEELING";

// =====================================================================
//  VOLATILE FLAGS — web-server / ESP-NOW callback → loop()
// =====================================================================
volatile bool  speedUpdatePending = false;
volatile int   pendingSpeed       = 0;
volatile bool  posUpdatePending   = false;
volatile int   pendingPosMode     = 0;
volatile bool  stopRequested      = false;
volatile bool  zeroAngleRequested = false;

// ESP-NOW incoming command buffer (client side)
volatile bool    espnowCmdPending = false;
volatile uint8_t espnowCmdType    = 0;
volatile int16_t espnowCmdValue   = 0;

// ESP-NOW incoming rename buffer (client side)
volatile bool espnowRenamePending = false;
char          espnowNewName[DEVICE_NAME_LEN];

// Heartbeat tracking (client side)
volatile unsigned long lastHeartbeatRecv = 0;
volatile bool          heartbeatEverRecv = false;

// Election state
uint8_t       bestCandidateMac[6]  = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned long electionStartTime    = 0;
unsigned long lastElectionBcast    = 0;
unsigned long clientStartTime      = 0;
volatile bool coordinatorFound     = false;  // Recv callback → electingLoop()
volatile bool demoteRequested      = false;  // Recv callback → coordinatorLoop()

// =====================================================================
//  TIMING
// =====================================================================
unsigned long lastBeaconSend   = 0;
unsigned long lastHeartbeatSend= 0;
unsigned long lastStatusSend   = 0;
unsigned long lastSelfUpdate   = 0;
unsigned long lastDeviceCheck  = 0;
bool          webRoutesRegistered = false;

// =====================================================================
//  HELPER:  MAC ↔ String
// =====================================================================
String macToHex(const uint8_t* mac) {
  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void hexToMac(const String &s, uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)strtoul(s.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  }
}

bool macEqual(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

// =====================================================================
//  DEVICE REGISTRY helpers
// =====================================================================
// Find device by MAC — returns index, or -1
int findDeviceByMac(const uint8_t *mac) {
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].used && macEqual(devices[i].mac, mac)) return i;
  }
  return -1;
}

// Find device by hex-string id from the web UI
int findDeviceById(const String &id) {
  uint8_t mac[6];
  hexToMac(id, mac);
  return findDeviceByMac(mac);
}

// Register self at slot 0
void registerSelf() {
  memset(&devices[0], 0, sizeof(DeviceEntry));
  devices[0].used   = true;
  devices[0].isSelf = true;
  memcpy(devices[0].mac, myMac, 6);
  strncpy(devices[0].name, myDeviceName, DEVICE_NAME_LEN - 1);
  devices[0].online   = true;
  devices[0].lastSeen = millis();
}

// Register or update a remote device — returns index
int registerDevice(const uint8_t *mac, const char *name) {
  int idx = findDeviceByMac(mac);
  if (idx >= 0) {
    // Update existing
    strncpy(devices[idx].name, name, DEVICE_NAME_LEN - 1);
    devices[idx].online   = true;
    devices[idx].lastSeen = millis();
    return idx;
  }
  // Find empty slot (skip 0 = self)
  for (int i = 1; i < MAX_DEVICES; i++) {
    if (!devices[i].used) {
      memset(&devices[i], 0, sizeof(DeviceEntry));
      devices[i].used = true;
      memcpy(devices[i].mac, mac, 6);
      strncpy(devices[i].name, name, DEVICE_NAME_LEN - 1);
      devices[i].online   = true;
      devices[i].lastSeen = millis();
      return i;
    }
  }
  return -1; // Registry full
}

void markOfflineDevices() {
  unsigned long now = millis();
  for (int i = 1; i < MAX_DEVICES; i++) {
    if (devices[i].used && devices[i].online) {
      if (now - devices[i].lastSeen > DEVICE_OFFLINE_MS) {
        devices[i].online = false;
      }
    }
  }
}

int countOnlineDevices() {
  int c = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].used && devices[i].online) c++;
  }
  return c;
}

// =====================================================================
//  DEVICE NAME  (NVS persistence)
// =====================================================================
void loadDeviceName() {
  preferences.begin("uvad", true);
  String n = preferences.getString("devName", "");
  preferences.end();

  if (n.length() == 0) {
    // Generate default from MAC tail
    snprintf(myDeviceName, DEVICE_NAME_LEN, "UVAD-%02X%02X", myMac[4], myMac[5]);
    // Save the generated name
    preferences.begin("uvad", false);
    preferences.putString("devName", String(myDeviceName));
    preferences.end();
  } else {
    strncpy(myDeviceName, n.c_str(), DEVICE_NAME_LEN - 1);
    myDeviceName[DEVICE_NAME_LEN - 1] = '\0';
  }
}

void saveDeviceName(const char *name) {
  strncpy(myDeviceName, name, DEVICE_NAME_LEN - 1);
  myDeviceName[DEVICE_NAME_LEN - 1] = '\0';
  preferences.begin("uvad", false);
  preferences.putString("devName", String(myDeviceName));
  preferences.end();
}

// =====================================================================
//  MOTOR SETTINGS  (NVS — "settings" namespace, same keys as single-device)
// =====================================================================
void readSettings() {
  preferences.begin("settings", false);
  enabled1 = preferences.getString("enable", "");
  if (enabled1 == "") {
    preferences.end();
    enabled1       = "enabled";
    setVoltage     = "5";
    microsteps     = "1";
    current        = "30";
    stallThreshold = "10";
    standstillMode = "FREEWHEELING";
    writeSettings();
  } else {
    setVoltage     = preferences.getString("voltage",        "5");
    microsteps     = preferences.getString("microsteps",     "1");
    current        = preferences.getString("current",        "30");
    stallThreshold = preferences.getString("stallThreshold", "10");
    standstillMode = preferences.getString("standstillMode", "FREEWHEELING");
    preferences.end();
  }
}

void writeSettings() {
  preferences.begin("settings", false);
  preferences.putString("enable",        enabled1);
  preferences.putString("voltage",       setVoltage);
  preferences.putString("microsteps",    microsteps);
  preferences.putString("current",       current);
  preferences.putString("stallThreshold",stallThreshold);
  preferences.putString("standstillMode",standstillMode);
  preferences.end();
  Serial.println("Settings saved to flash");
  configureSettings();
}

void configureSettings() {
  // Voltage via CFG1/2/3
  if      (setVoltage == "5")  { digitalWrite(CFG1, HIGH); }
  else if (setVoltage == "9")  { digitalWrite(CFG1, LOW); digitalWrite(CFG2, LOW);  digitalWrite(CFG3, LOW);  }
  else if (setVoltage == "12") { digitalWrite(CFG1, LOW); digitalWrite(CFG2, LOW);  digitalWrite(CFG3, HIGH); }
  else if (setVoltage == "15") { digitalWrite(CFG1, LOW); digitalWrite(CFG2, HIGH); digitalWrite(CFG3, HIGH); }
  else if (setVoltage == "20") { digitalWrite(CFG1, LOW); digitalWrite(CFG2, HIGH); digitalWrite(CFG3, LOW);  }

  stepper_driver.setRunCurrent(current.toInt());
  stepper_driver.setMicrostepsPerStep(microsteps.toInt());
  stepper_driver.setStallGuardThreshold(stallThreshold.toInt());

  if      (standstillMode == "NORMAL")         stepper_driver.setStandstillMode(stepper_driver.NORMAL);
  else if (standstillMode == "FREEWHEELING")   stepper_driver.setStandstillMode(stepper_driver.FREEWHEELING);
  else if (standstillMode == "BRAKING")        stepper_driver.setStandstillMode(stepper_driver.BRAKING);
  else if (standstillMode == "STRONG_BRAKING") stepper_driver.setStandstillMode(stepper_driver.STRONG_BRAKING);
}

// =====================================================================
//  ENCODER
// =====================================================================
void readEncoder() {
  int raw_counts = 0;
  static int prev_raw_counts = 0;
  static signed long revolutions = 0;

  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(0x0C);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDRESS, 2);
  if (Wire.available() >= 2) {
    raw_counts = Wire.read() << 8 | Wire.read();
  }

  if (prev_raw_counts > 3000 && raw_counts < 1000)      revolutions++;
  else if (prev_raw_counts < 1000 && raw_counts > 3000)  revolutions--;

  prev_raw_counts = raw_counts;
  total_encoder_counts = raw_counts + (4096 * revolutions);
}

// =====================================================================
//  SENSOR READING — raw numeric helpers
// =====================================================================
int16_t readVoltageRaw() {
  uint32_t mvSum = 0;
  for (int i = 0; i < 10; i++) mvSum += analogReadMilliVolts(VBUS);
  float avgMv = (float)mvSum / 10.0f;
  float vbus  = (avgMv / 1000.0f) / DIV_RATIO;
  return (int16_t)(vbus * 1000.0f); // millivolts (e.g. 12340 = 12.34 V; int16_t covers up to 32.7 V)
}

uint8_t readPgCode() {
  return (digitalRead(PG) == LOW) ? PG_GOOD : PG_BAD;
}

uint8_t readDriverCode() {
  if (stepper_driver.hardwareDisabled()) return DRV_HW_DISABLED;
  TMC2209::Status st = stepper_driver.getStatus();
  if (st.over_temperature_shutdown) return DRV_TEMP_SHUTDOWN;
  if (st.over_temperature_warning)  return DRV_TEMP_WARN;
  return DRV_OK;
}

uint8_t readStallCode() {
  return (digitalRead(DIAG) == HIGH) ? STALL_DETECTED : STALL_NONE;
}

int32_t readAngleDeg() {
  readEncoder();
  if (!encoder_offset_set) {
    encoder_offset     = total_encoder_counts;
    encoder_offset_set = true;
  }
  signed long delta = total_encoder_counts - encoder_offset;
  return (int32_t)lround((double)delta * 360.0 / (4096.0 * GEAR_RATIO));
}

// =====================================================================
//  COMMAND EXECUTION  (called by both coordinator-local and client-remote)
// =====================================================================
void executeCommand(uint8_t cmd, int16_t value) {
  switch (cmd) {
    case CMD_VELOCITY:
      set_speed = value;
      stepper_driver.moveAtVelocity(set_speed * microsteps.toInt());
      break;

    case CMD_POSITION:
      stepper_driver.moveAtVelocity(0);
      if      (value == 1) setPoint -= countsFromDegrees(LARGE_ANGLE_DEG);
      else if (value == 2) setPoint -= countsFromDegrees(SMALL_ANGLE_DEG);
      else if (value == 3) setPoint += countsFromDegrees(SMALL_ANGLE_DEG);
      else if (value == 4) setPoint += countsFromDegrees(LARGE_ANGLE_DEG);
      break;

    case CMD_STOP:
      stepper_driver.moveAtVelocity(0);
      set_speed   = 0;
      buttonSpeed = 0;
      setPoint    = CurrentPosition;
      break;

    case CMD_ZERO_ANGLE:
      stepper_driver.moveAtVelocity(0);
      set_speed = 0;
      readEncoder();
      encoder_offset     = total_encoder_counts;
      encoder_offset_set = true;
      setPoint        = 0;
      CurrentPosition = 0;
      break;
  }
}

// =====================================================================
//  ESP-NOW  — peer helpers
// =====================================================================
void addBroadcastPeer() {
  if (esp_now_is_peer_exist(BROADCAST_MAC)) return;
  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, BROADCAST_MAC, 6);
  pi.channel = UVAD_AP_CHANNEL;
  pi.encrypt = false;
  // Coordinator sends via AP interface; clients and electing devices use STA interface
  pi.ifidx = (currentRole == ROLE_COORDINATOR) ? WIFI_IF_AP : WIFI_IF_STA;
  esp_now_add_peer(&pi);
}

void addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, mac, 6);
  pi.channel = UVAD_AP_CHANNEL;
  pi.encrypt = false;
  pi.ifidx = (currentRole == ROLE_COORDINATOR) ? WIFI_IF_AP : WIFI_IF_STA;
  esp_now_add_peer(&pi);
}

// =====================================================================
//  ESP-NOW  — receive callback  (runs in WiFi task context!)
//  Signature differs between ESP-IDF 4.x (Core 2.x) and 5.x (Core 3.x)
// =====================================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len < 1) return;
  uint8_t type = data[0];

  // ---- Election: process election and announce messages ----
  if (currentRole == ROLE_ELECTING) {
    if (type == MSG_ELECTION && len >= (int)sizeof(MsgElection)) {
      MsgElection msg;
      memcpy(&msg, data, sizeof(MsgElection));
      Serial.printf("[ELECT] Received candidate %s (current best %s)\n",
                     macToHex(msg.mac).c_str(), macToHex(bestCandidateMac).c_str());
      if (memcmp(msg.mac, bestCandidateMac, 6) < 0) {
        memcpy(bestCandidateMac, msg.mac, 6);
      }
    }
    else if (type == MSG_ANNOUNCE || type == MSG_HEARTBEAT) {
      // An existing coordinator is active — abort election and join as client
      Serial.printf("[ELECT] Coordinator active (msg type 0x%02X from %s) — aborting election\n",
                     type, macToHex(mac).c_str());
      coordinatorFound = true;
    }
    return; // During election, ignore all other message types
  }

  if (currentRole == ROLE_COORDINATOR) {
    // ---- Coordinator receives beacons and status from clients ----
    if (type == MSG_BEACON && len >= (int)sizeof(MsgBeacon)) {
      MsgBeacon msg;
      memcpy(&msg, data, sizeof(MsgBeacon));
      msg.name[DEVICE_NAME_LEN - 1] = '\0';

      addPeer(mac); // Ensure we can unicast back
      int idx = registerDevice(mac, msg.name);
      if (idx >= 0) {
        Serial.printf("Beacon from %s (%s)\n", msg.name, macToHex(mac).c_str());
      }
    }
    else if (type == MSG_STATUS && len >= (int)sizeof(MsgStatus)) {
      MsgStatus msg;
      memcpy(&msg, data, sizeof(MsgStatus));
      msg.name[DEVICE_NAME_LEN - 1] = '\0';

      int idx = findDeviceByMac(mac);
      if (idx < 0) {
        // Auto-register if we see status before beacon
        addPeer(mac);
        idx = registerDevice(mac, msg.name);
      }
      if (idx >= 0) {
        devices[idx].voltage_mv    = msg.voltage_mv;
        devices[idx].pgStatus      = msg.pg;
        devices[idx].driverStatus  = msg.driver;
        devices[idx].stallStatus   = msg.stall;
        devices[idx].angle_deg     = msg.angle_deg;
        devices[idx].online        = true;
        devices[idx].lastSeen      = millis();
        // Update name if client changed it
        strncpy(devices[idx].name, msg.name, DEVICE_NAME_LEN - 1);
      }
    }

    // ---- Coordinator conflict detection ----
    if (type == MSG_HEARTBEAT && !macEqual(mac, myMac)) {
      // Another coordinator is broadcasting heartbeats — higher MAC yields
      if (memcmp(myMac, mac, 6) > 0) {
        demoteRequested = true;
      }
    }
    // ---- Respond to election messages so electing devices know we exist ----
    if (type == MSG_ELECTION) {
      sendCoordinatorAnnounce();
    }
  }
  else if (currentRole == ROLE_CLIENT) {
    // ---- Client receives commands, renames, and heartbeats ----
    if (type == MSG_HEARTBEAT && len >= (int)sizeof(MsgHeartbeat)) {
      lastHeartbeatRecv = millis();
      if (!heartbeatEverRecv) {
        Serial.printf("[CLIENT] First heartbeat received from %s\n", macToHex(mac).c_str());
      }
      heartbeatEverRecv = true;
    }
    else if (type == MSG_COMMAND && len >= (int)sizeof(MsgCommand)) {
      MsgCommand msg;
      memcpy(&msg, data, sizeof(MsgCommand));
      espnowCmdType    = msg.cmd;
      espnowCmdValue   = msg.value;
      espnowCmdPending = true;   // loop() will execute
    }
    else if (type == MSG_RENAME && len >= (int)sizeof(MsgRename)) {
      MsgRename msg;
      memcpy(&msg, data, sizeof(MsgRename));
      msg.name[DEVICE_NAME_LEN - 1] = '\0';
      strncpy(espnowNewName, msg.name, DEVICE_NAME_LEN);
      espnowRenamePending = true; // loop() will save
    }
  }
}

// Signature differs between ESP-IDF 4.x (Core 2.x) and 5.x (Core 3.x)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onEspNowSend(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void onEspNowSend(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  // Optional: could track success/fail stats here
}

// =====================================================================
//  ESP-NOW  — send helpers
// =====================================================================
void sendHeartbeat() {
  MsgHeartbeat msg;
  msg.type        = MSG_HEARTBEAT;
  msg.deviceCount = (uint8_t)countOnlineDevices();
  esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
}

void sendBeacon() {
  MsgBeacon msg;
  msg.type = MSG_BEACON;
  strncpy(msg.name, myDeviceName, DEVICE_NAME_LEN - 1);
  msg.name[DEVICE_NAME_LEN - 1] = '\0';
  esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
}

void sendElection() {
  MsgElection msg;
  msg.type = MSG_ELECTION;
  memcpy(msg.mac, myMac, 6);
  esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
}

void sendCoordinatorAnnounce() {
  MsgAnnounce msg;
  msg.type = MSG_ANNOUNCE;
  memcpy(msg.mac, myMac, 6);
  esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
}

void sendStatusMsg() {
  MsgStatus msg;
  msg.type = MSG_STATUS;
  strncpy(msg.name, myDeviceName, DEVICE_NAME_LEN - 1);
  msg.name[DEVICE_NAME_LEN - 1] = '\0';
  msg.voltage_mv = readVoltageRaw();
  msg.pg         = readPgCode();
  msg.driver     = readDriverCode();
  msg.stall      = readStallCode();
  msg.angle_deg  = readAngleDeg();
  esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
}

void sendCommandToDevice(const uint8_t *mac, uint8_t cmd, int16_t value) {
  MsgCommand msg;
  msg.type  = MSG_COMMAND;
  msg.cmd   = cmd;
  msg.value = value;
  esp_err_t err = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
  if (err != ESP_OK) {
    Serial.printf("sendCommandToDevice failed (%d) to %s\n", (int)err, macToHex(mac).c_str());
  }
}

void sendRenameToDevice(const uint8_t *mac, const char *name) {
  MsgRename msg;
  msg.type = MSG_RENAME;
  strncpy(msg.name, name, DEVICE_NAME_LEN - 1);
  msg.name[DEVICE_NAME_LEN - 1] = '\0';
  esp_err_t err = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
  if (err != ESP_OK) {
    Serial.printf("sendRenameToDevice failed (%d) to %s\n", (int)err, macToHex(mac).c_str());
  }
}

// =====================================================================
//  JSON builder — used by GET /devices
// =====================================================================
String buildDevicesJson() {
  String j = "{\"cfg\":{\"sa\":";
  j += String(SMALL_ANGLE_DEG, 1);
  j += ",\"la\":";
  j += String(LARGE_ANGLE_DEG, 1);
  j += "},\"devs\":[";

  bool first = true;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].used) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"id\":\"";
    j += macToHex(devices[i].mac);
    j += "\",\"n\":\"";
    // Escape any quotes in name
    String safeName = devices[i].name;
    safeName.replace("\"", "'");
    j += safeName;
    j += "\",\"on\":";
    j += devices[i].online ? "1" : "0";
    j += ",\"mv\":";
    j += String(devices[i].voltage_mv);
    j += ",\"pg\":";
    j += String(devices[i].pgStatus);
    j += ",\"dr\":";
    j += String(devices[i].driverStatus);
    j += ",\"st\":";
    j += String(devices[i].stallStatus);
    j += ",\"ang\":";
    j += String(devices[i].angle_deg);
    j += ",\"hub\":";
    j += devices[i].isSelf ? "1" : "0";
    j += "}";
  }
  j += "]}";
  return j;
}

// =====================================================================
//  WEB SERVER ROUTES  (registered once, server.begin() called when
//  the device becomes coordinator)
// =====================================================================
void registerWebRoutes() {
  if (webRoutesRegistered) return;
  webRoutesRegistered = true;

  // --- Serve the multi-device UI (no template processor) ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // --- Device list (JSON, polled by JS) ---
  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildDevicesJson());
  });

  // --- Motor command from web UI ---
  server.on("/command", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id", true) || !request->hasParam("cmd", true)) {
      request->send(400);
      return;
    }
    String id  = request->getParam("id",  true)->value();
    String cmd = request->getParam("cmd", true)->value();
    int val = request->hasParam("val", true) ? request->getParam("val", true)->value().toInt() : 0;

    int idx = findDeviceById(id);
    if (idx < 0) { request->send(404); return; }

    uint8_t cmdType = 0;
    if      (cmd == "velocity") cmdType = CMD_VELOCITY;
    else if (cmd == "position") cmdType = CMD_POSITION;
    else if (cmd == "stop")     cmdType = CMD_STOP;
    else if (cmd == "zero")     cmdType = CMD_ZERO_ANGLE;
    else { request->send(400); return; }

    if (devices[idx].isSelf) {
      // Execute locally via volatile flags (thread safety)
      if (cmdType == CMD_VELOCITY)   { pendingSpeed = (int)val; speedUpdatePending = true; }
      else if (cmdType == CMD_POSITION) { pendingPosMode = val; posUpdatePending = true; }
      else if (cmdType == CMD_STOP)     { stopRequested = true; }
      else if (cmdType == CMD_ZERO_ANGLE) { zeroAngleRequested = true; }
    } else {
      // Relay to remote device via ESP-NOW (unicast)
      sendCommandToDevice(devices[idx].mac, cmdType, (int16_t)val);
    }
    request->send(200);
  });

  // --- Rename device ---
  server.on("/rename", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id", true) || !request->hasParam("name", true)) {
      request->send(400);
      return;
    }
    String id   = request->getParam("id",   true)->value();
    String name = request->getParam("name", true)->value();
    name.trim();
    if (name.length() == 0 || name.length() >= DEVICE_NAME_LEN) {
      request->send(400);
      return;
    }

    int idx = findDeviceById(id);
    if (idx < 0) { request->send(404); return; }

    strncpy(devices[idx].name, name.c_str(), DEVICE_NAME_LEN - 1);
    devices[idx].name[DEVICE_NAME_LEN - 1] = '\0';

    if (devices[idx].isSelf) {
      saveDeviceName(name.c_str());
    } else {
      sendRenameToDevice(devices[idx].mac, name.c_str());
    }
    request->send(200);
  });

  // --- Stop ALL devices ---
  server.on("/stop_all", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Stop self
    stopRequested = true;
    // Stop every remote client
    for (int i = 1; i < MAX_DEVICES; i++) {
      if (devices[i].used && devices[i].online) {
        sendCommandToDevice(devices[i].mac, CMD_STOP, 0);
      }
    }
    request->send(200);
  });

  // --- Firmware version ---
  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", FIRMWARE_VERSION);
  });
}

// =====================================================================
//  ROLE SELECTION  (election protocol via ESP-NOW)
// =====================================================================
void startElection() {
  Serial.println("=== Starting coordinator election ===");
  currentRole = ROLE_ELECTING;
  coordinatorFound = false;
  memcpy(bestCandidateMac, myMac, 6);
  electionStartTime = millis();
  lastElectionBcast = 0;

  esp_now_deinit();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(UVAD_AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  setupEspNow();
}

void determineRole() {
  // Small random jitter (0–1 s) to stagger simultaneous boots
  delay(esp_random() % 1000);
  startElection();
}

// =====================================================================
//  SETUP HELPERS  (role-specific networking)
// =====================================================================
void setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_register_send_cb(onEspNowSend);
  addBroadcastPeer();
  Serial.println("ESP-NOW initialised");
}

void setupCoordinator() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(UVAD_AP_SSID, UVAD_AP_PASSWORD, UVAD_AP_CHANNEL);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupEspNow();
  registerSelf();

  registerWebRoutes();
  server.begin();
  Serial.println("Web server started");
}

void setupClient() {
  currentRole = ROLE_CLIENT;
  esp_now_deinit();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(UVAD_AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  setupEspNow();
  lastHeartbeatRecv = millis();
  heartbeatEverRecv = false;
  clientStartTime   = millis();
  Serial.println("Client ready — waiting for coordinator heartbeat");
}

void becomeCoordinator() {
  Serial.println("=== Promoting to COORDINATOR ===");
  esp_now_deinit();
  memset(devices, 0, sizeof(devices));

  currentRole = ROLE_COORDINATOR;  // Set BEFORE setupEspNow so broadcast peer uses AP interface

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(UVAD_AP_SSID, UVAD_AP_PASSWORD, UVAD_AP_CHANNEL);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupEspNow();
  registerSelf();

  registerWebRoutes();
  server.begin();
  digitalWrite(LED1, HIGH); // Coordinator = solid ON
  Serial.println("Promoted — web server started");
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  // --- PD Trigger ---
  pinMode(PG,   INPUT);
  pinMode(CFG1, OUTPUT);
  pinMode(CFG2, OUTPUT);
  pinMode(CFG3, OUTPUT);
  digitalWrite(CFG1, HIGH); // Default 5 V
  digitalWrite(CFG2, LOW);
  digitalWrite(CFG3, LOW);

  // --- General I/O ---
  // Physical buttons disabled (PCB inputs ignored)
//  pinMode(SW1,  INPUT);
//  pinMode(SW2,  INPUT);
//  pinMode(SW3,  INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR,  OUTPUT);

  // --- TMC2209 ---
  pinMode(MS1,    OUTPUT);
  pinMode(MS2,    OUTPUT);
  pinMode(TMC_EN, OUTPUT);
  pinMode(DIAG,   INPUT);
  digitalWrite(TMC_EN, LOW);
  digitalWrite(MS2,    LOW);

  // --- Encoder ---
  Wire.begin(SDA, SCL);

  // --- ADC ---
  analogSetPinAttenuation(VBUS, ADC_11db);

  // --- Motor settings from NVS ---
  readSettings();

  stepper_driver.setup(serial_stream, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_0, TMC_RX, TMC_TX);
  stepper_driver.setRunCurrent(RUN_CURRENT_PERCENT);
  stepper_driver.enableAutomaticCurrentScaling();
  stepper_driver.enableStealthChop();
  stepper_driver.setCoolStepDurationThreshold(5000);
  stepper_driver.disable();
  configureSettings();

  delay(200);
  Serial.begin(115200);
  Serial.println("UVAD Multi-Device " FIRMWARE_VERSION);

  // --- Learn own MAC (read from eFuse — reliable regardless of WiFi state) ---
  esp_efuse_mac_get_default(myMac);
  WiFi.mode(WIFI_STA);
  Serial.printf("MAC: %s\n", macToHex(myMac).c_str());

  // --- Device name ---
  loadDeviceName();
  Serial.printf("Name: %s\n", myDeviceName);

  // --- Role selection via election protocol ---
  // Register web routes now so they're ready for any role transition
  registerWebRoutes();
  determineRole();

  // --- Boot LED flash ---
  digitalWrite(LED1, HIGH);
  delay(200);
  digitalWrite(LED1, LOW);
}

// =====================================================================
//  COMMON LOOP — motor control, encoder, volatile-flag dispatch  (both roles)
// =====================================================================
void commonLoop() {

  // --- Handle stop request (volatile flag) ---
  if (stopRequested) {
    stepper_driver.moveAtVelocity(0);
    set_speed   = 0;
    buttonSpeed = 0;
    pendingSpeed = 0;
    speedUpdatePending = false;
    pendingPosMode = 0;
    posUpdatePending = false;
    setPoint = CurrentPosition;
    stopRequested = false;
  }

  // --- Handle zero-angle request ---
  if (zeroAngleRequested) {
    readEncoder();
    encoder_offset     = total_encoder_counts;
    encoder_offset_set = true;
    setPoint        = 0;
    CurrentPosition = 0;
    pendingPosMode  = 0;
    posUpdatePending = false;
    zeroAngleRequested = false;
  }

  // --- Handle velocity update (from web or ESP-NOW) ---
  if (speedUpdatePending) {
    set_speed = pendingSpeed;
    stepper_driver.moveAtVelocity(set_speed * microsteps.toInt());
    speedUpdatePending = false;
  }

  // --- Handle position update ---
  if (posUpdatePending) {
    stepper_driver.moveAtVelocity(0);
    if      (pendingPosMode == 1) setPoint -= countsFromDegrees(LARGE_ANGLE_DEG);
    else if (pendingPosMode == 2) setPoint -= countsFromDegrees(SMALL_ANGLE_DEG);
    else if (pendingPosMode == 3) setPoint += countsFromDegrees(SMALL_ANGLE_DEG);
    else if (pendingPosMode == 4) setPoint += countsFromDegrees(LARGE_ANGLE_DEG);
    posUpdatePending = false;
  }

  // --- Handle ESP-NOW command (client receives from coordinator) ---
  if (espnowCmdPending) {
    executeCommand(espnowCmdType, espnowCmdValue);
    espnowCmdPending = false;
  }

  // --- Handle ESP-NOW rename ---
  if (espnowRenamePending) {
    saveDeviceName(espnowNewName);
    Serial.printf("Renamed to: %s\n", myDeviceName);
    espnowRenamePending = false;
  }

  // --- Periodic: PG check, enable/disable stepper, stall LED ---
  if (millis() - lastEncRead >= (unsigned long)mainFreq) {
    lastEncRead = millis();
    digitalWrite(LED2, digitalRead(DIAG)); // Stall LED

    PGState = digitalRead(PG);
    if (PGState == LOW && enabled1 == "enabled" && !enabledState) {
      stepper_driver.enable();
      enabledState = true;
    } else if ((PGState == HIGH || enabled1 == "disabled") && enabledState) {
      stepper_driver.disable();
      enabledState = false;
    }
  }

  // --- Open-loop position stepping ---
  int delaySpeed = 4500;
  int microSteps = microsteps.toInt();
  int delayAdj   = delaySpeed / microSteps;

  if (setPoint > CurrentPosition) {
    if (micros() - lastStep > (unsigned long)delayAdj) {
      digitalWrite(DIR, LOW);
      digitalWrite(STEP, stepState);
      stepState = !stepState;
      if (stepState) CurrentPosition += (COUNTS_PER_FULL_STEP / microSteps);
      lastStep = micros();
    }
  } else if (setPoint < CurrentPosition) {
    if (micros() - lastStep > (unsigned long)delayAdj) {
      digitalWrite(DIR, HIGH);
      digitalWrite(STEP, stepState);
      stepState = !stepState;
      if (stepState) CurrentPosition -= (COUNTS_PER_FULL_STEP / microSteps);
      lastStep = micros();
    }
  }

  // --- Physical button handling disabled (PCB inputs ignored) ---
//  if ((millis() - lastDebounceTime) > debounceDelay) {
//    lastDebounceTime = millis();
//    bool curInc   = digitalRead(SW3);
//    bool curDec   = digitalRead(SW1);
//    bool curReset = digitalRead(SW2);
//
//    if (curInc != incButtonState) {
//      incButtonState = curInc;
//      if (incButtonState == LOW) {
//        buttonSpeed = min(buttonSpeed + 30, 330);
//        stepper_driver.moveAtVelocity(buttonSpeed * microsteps.toInt());
//      }
//    }
//    if (curDec != decButtonState) {
//      decButtonState = curDec;
//      if (decButtonState == LOW) {
//        buttonSpeed = max(buttonSpeed - 30, -330);
//        stepper_driver.moveAtVelocity(buttonSpeed * microsteps.toInt());
//      }
//    }
//    if (curReset != resetButtonState) {
//      resetButtonState = curReset;
//      if (resetButtonState == LOW) {
//        buttonSpeed = 0;
//        stepper_driver.moveAtVelocity(0);
//      }
//    }
//  }
}

// =====================================================================
//  ELECTING LOOP  — runs while ROLE_ELECTING, handles election protocol
// =====================================================================
void electingLoop() {
  unsigned long now = millis();

  // Another coordinator responded — join as client immediately
  if (coordinatorFound) {
    Serial.println("Coordinator found during election — becoming client");
    setupClient();
    return;
  }

  // Broadcast our candidacy periodically
  if (now - lastElectionBcast > ELECTION_BROADCAST_MS) {
    lastElectionBcast = now;
    sendElection();
  }

  // Election duration elapsed — decide winner
  if (now - electionStartTime > ELECTION_DURATION_MS) {
    if (memcmp(bestCandidateMac, myMac, 6) == 0) {
      Serial.println("Election won — becoming coordinator");
      becomeCoordinator();
      // Announce so any late-arriving electors know immediately
      sendCoordinatorAnnounce();
    } else {
      Serial.printf("Election lost to %s — becoming client\n",
                     macToHex(bestCandidateMac).c_str());
      setupClient();
    }
    return;
  }

  // Fast LED blink during election
  digitalWrite(LED1, (millis() / 150) % 2);
}

// =====================================================================
//  COORDINATOR LOOP
// =====================================================================
void coordinatorLoop() {
  unsigned long now = millis();

  // --- Check for demotion (coordinator conflict resolution) ---
  if (demoteRequested) {
    demoteRequested = false;
    Serial.println("=== Demoting to CLIENT (conflict resolution) ===");
    esp_now_deinit();
    memset(devices, 0, sizeof(devices));
    setupClient();
    return;
  }

  // --- Send heartbeat ---
  if (now - lastHeartbeatSend > HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatSend = now;
    sendHeartbeat();
    Serial.printf("[COORD] Heartbeat sent | %d devices online\n", countOnlineDevices());
  }

  // --- Update own status in device registry ---
  if (now - lastSelfUpdate > SELF_STATUS_INTERVAL_MS) {
    lastSelfUpdate = now;
    devices[0].voltage_mv   = readVoltageRaw();
    devices[0].pgStatus     = readPgCode();
    devices[0].driverStatus = readDriverCode();
    devices[0].stallStatus  = readStallCode();
    devices[0].angle_deg    = readAngleDeg();
    devices[0].online       = true;
    devices[0].lastSeen     = now;
    // Keep name in sync
    strncpy(devices[0].name, myDeviceName, DEVICE_NAME_LEN - 1);
  }

  // --- Mark offline devices ---
  if (now - lastDeviceCheck > 2000) {
    lastDeviceCheck = now;
    markOfflineDevices();
  }
}

// =====================================================================
//  CLIENT LOOP
// =====================================================================
void clientLoop() {
  // Snapshot volatiles BEFORE millis() to prevent unsigned underflow.
  // If the callback updates lastHeartbeatRecv between our snapshot and millis(),
  // millis() will still be >= our snapshot, keeping the subtraction safe.
  unsigned long lastHbSnap = lastHeartbeatRecv;
  bool          hbEverSnap = heartbeatEverRecv;
  unsigned long now = millis();

  // --- Send beacon periodically ---
  if (now - lastBeaconSend > BEACON_INTERVAL_MS) {
    lastBeaconSend = now;
    sendBeacon();
    Serial.printf("[CLIENT] Beacon sent | hbEver=%d hbAge=%lums clientAge=%lums\n",
                   (int)hbEverSnap,
                   hbEverSnap ? (now - lastHbSnap) : 0UL,
                   now - clientStartTime);
  }

  // --- Send status periodically ---
  if (now - lastStatusSend > STATUS_INTERVAL_MS) {
    lastStatusSend = now;
    sendStatusMsg();
  }

  // --- Coordinator timeout → start new election ---
  if (hbEverSnap && (now - lastHbSnap > COORDINATOR_TIMEOUT_MS)) {
    Serial.println("Coordinator timeout — starting election");
    startElection();
    return;
  }

  // --- Bootstrap fix: never received a heartbeat within timeout ---
  if (!hbEverSnap && (now - clientStartTime > CLIENT_NO_HB_TIMEOUT_MS)) {
    Serial.println("No heartbeat ever received — starting election");
    startElection();
    return;
  }

  // LED1 flicker when in client mode (distinguishes from coordinator)
  digitalWrite(LED1, (millis() / 500) % 2);
}

// =====================================================================
//  MAIN LOOP
// =====================================================================
void loop() {
  commonLoop();

  if (currentRole == ROLE_COORDINATOR) {
    coordinatorLoop();
  } else if (currentRole == ROLE_ELECTING) {
    electingLoop();
  } else {
    clientLoop();
  }
}
