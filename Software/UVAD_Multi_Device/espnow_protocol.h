/*
 * espnow_protocol.h
 * ESP-NOW message definitions for UVAD Multi-Device system.
 * Shared between Coordinator and Client roles.
 */

#ifndef ESPNOW_PROTOCOL_H
#define ESPNOW_PROTOCOL_H

#include <stdint.h>

// =============== Network Configuration ===============
// AP name and password — all coordinators use the same SSID so phones auto-reconnect
// after failover.  Set a password of 8+ chars to secure the network.
#define UVAD_AP_SSID        "UVAD-AP"
#define UVAD_AP_PASSWORD    ""          // "" = open AP; set e.g. "mypass123" to secure
#define UVAD_AP_CHANNEL     1           // Fixed channel — keeps ESP-NOW and SoftAP in sync

// =============== Timing (ms) ===============
#define BEACON_INTERVAL_MS        3000  // Client → broadcast "I exist"
#define HEARTBEAT_INTERVAL_MS     2000  // Coordinator → broadcast "I'm alive"
#define STATUS_INTERVAL_MS         100  // Client → broadcast sensor readings (faster UI feedback)
#define COORDINATOR_TIMEOUT_MS   10000  // Client declares coordinator dead after this
#define DEVICE_OFFLINE_MS         8000  // Coordinator marks a client offline after this
#define SELF_STATUS_INTERVAL_MS    100  // Coordinator self-sensor update interval
#define REELECTION_MAX_DELAY_MS   3000  // Random backoff ceiling for coordinator election

// =============== Limits ===============
#define MAX_DEVICES         10
#define DEVICE_NAME_LEN     16          // Including null terminator

// =============== Message Types ===============
enum MsgType : uint8_t {
  MSG_BEACON    = 0x01,   // Client  → Coordinator  (broadcast)
  MSG_HEARTBEAT = 0x02,   // Coordinator → All       (broadcast)
  MSG_COMMAND   = 0x03,   // Coordinator → Client    (unicast)
  MSG_STATUS    = 0x04,   // Client  → Coordinator   (broadcast)
  MSG_RENAME    = 0x05,   // Coordinator → Client    (unicast)
};

// =============== Command Sub-types ===============
enum CmdType : uint8_t {
  CMD_VELOCITY    = 1,
  CMD_POSITION    = 2,    // value 1-4 maps to angle buttons
  CMD_STOP        = 3,
  CMD_ZERO_ANGLE  = 4,
};

// =============== Status Codes ===============
enum PgCode    : uint8_t { PG_GOOD = 0, PG_BAD = 1 };
enum DrvCode   : uint8_t { DRV_OK = 0, DRV_TEMP_WARN = 1, DRV_TEMP_SHUTDOWN = 2, DRV_HW_DISABLED = 3 };
enum StallCode : uint8_t { STALL_NONE = 0, STALL_DETECTED = 1 };

// =============== Message Structures (packed for ESP-NOW) ===============

// Beacon: client announces itself to the coordinator
struct __attribute__((packed)) MsgBeacon {
  uint8_t type;                     // MSG_BEACON
  char    name[DEVICE_NAME_LEN];    // Human-readable name
};

// Heartbeat: coordinator says "I'm alive"
struct __attribute__((packed)) MsgHeartbeat {
  uint8_t type;                     // MSG_HEARTBEAT
  uint8_t deviceCount;              // How many devices the coordinator knows about
};

// Command: coordinator tells a client what to do
struct __attribute__((packed)) MsgCommand {
  uint8_t type;                     // MSG_COMMAND
  uint8_t cmd;                      // CmdType
  int16_t value;                    // Velocity value  -or-  position button 1-4
};

// Status: client reports its sensor readings to the coordinator
struct __attribute__((packed)) MsgStatus {
  uint8_t type;                     // MSG_STATUS
  char    name[DEVICE_NAME_LEN];    // Current device name (may have been renamed)
  int16_t voltage_mv;               // VBus in millivolts (e.g. 12340 = 12.34 V)
  uint8_t pg;                       // PgCode
  uint8_t driver;                   // DrvCode
  uint8_t stall;                    // StallCode
  int32_t angle_deg;                // Output-shaft angle in whole degrees
};

// Rename: coordinator pushes a new name to a client
struct __attribute__((packed)) MsgRename {
  uint8_t type;                     // MSG_RENAME
  char    name[DEVICE_NAME_LEN];    // New name to store in NVS
};

// Broadcast address constant
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#endif // ESPNOW_PROTOCOL_H
