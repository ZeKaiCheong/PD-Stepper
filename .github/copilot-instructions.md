# Copilot instructions - PD Stepper

Keep suggestions narrow and specific to this repository. Focus on Arduino/ESP32 firmware and config files under `Software/`, and reference PCB assumptions in `PCB/` when needed.

- Purpose: help contributors extend ESP32-S3 firmware examples (web servers, ESP-NOW, serial control, ESPHome configs).

- Big picture (major variants):
  - Single-device web server: `Software/PD_Stepper_Web_Server/PD_Stepper_Web_Server.ino` serves `index_html.h` and exposes HTTP endpoints for control.
  - UVAD single-device web server: `Software/UVAD_Web_Server/UVAD_Web_Server.ino` is a similar UI-driven controller with the same hardware assumptions.
  - UVAD multi-device: `Software/UVAD_Multi_Device/UVAD_Multi_Device.ino` runs on every unit; the first booted device becomes coordinator (SoftAP + web UI + ESP-NOW hub), others become clients and report status via ESP-NOW.
  - ESPHome examples: `Software/ESPHome/*.yaml` rely on a custom TMC2209 component and expose encoder and button inputs.
  - Serial control: `Software/Serial_Control/Serial_Control.ino` supports commands on USB `Serial` and AUX `Serial1` (see the Serial_Control README for the command set).

- Build/flash workflow (Arduino IDE):
  - Board: `ESP32S3 Dev Module`, `USB CDC on Boot` = Enabled. See `Software/README.md`.
  - Common libraries: `ESPAsyncWebServer`, `AsyncTCP`, `TMC2209` (janelia-arduino), `Preferences`, `Wire`.
  - Prebuilt binary: `Software/PD_Stepper_Web_Server/PD_Stepper_Web_Server.bin` can be flashed via ESPConnect.

- Hardware and configuration conventions:
  - Pin constants are defined at the top of each sketch; update those macros instead of scattering literals.
  - USB PD voltage selection is driven by `CFG1/CFG2/CFG3` in `configureSettings()`; do not change the mapping without hardware review (R18 note in top-level README).
  - Stepper enable is gated by `PG` (active LOW). Motion logic should respect that guard.
  - Settings persist in `Preferences` namespace `settings` with keys: `enable`, `voltage`, `microsteps`, `current`, `stallThreshold`, `standstillMode`.
  - Web UI templating: `index_html.h` uses placeholders replaced by `processor()` in the sketch. Update both when adding fields.
  - TMC2209 UART uses `Serial2` via `stepper_driver.setup(serial_stream, ...)`.

- HTTP API patterns:
  - Single-device web server endpoints include `GET /powergood`, `/voltage`, `/position`, `/status`, `/stallguard` and `POST /update`, `/save`.
  - UVAD multi-device adds `GET /devices`, `POST /command`, `POST /rename`, and `POST /stop_all` in `registerWebRoutes()`.

- ESP-NOW flow (multi-device):
  - Coordinator maintains a device registry; clients send beacons and status and accept command/rename packets (see `espnow_protocol.h`).
  - Clients promote to coordinator on heartbeat timeout with a randomized backoff.

- Common pitfalls:
  - Encoder wrap handling uses 4096 counts with 1000/3000 thresholds in `readEncoder()`; preserve this logic when refactoring.
  - Velocity is scaled by `microsteps.toInt()` in multiple places; keep units consistent when changing motion code.

If anything is unclear (for example which sketch you want to target or which endpoints are required), ask for clarification before implementing.
