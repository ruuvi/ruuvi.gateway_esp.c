# AGENTS.md — Ruuvi Gateway ESP32 Firmware

## Project Overview

ESP32 firmware for the Ruuvi Gateway — a BLE-to-cloud bridge that receives BLE advertisements from Ruuvi sensors via an nRF52 co-processor and forwards them over HTTP/MQTT. Built on **ESP-IDF v4.2.5** (pinned in `CMakeLists.txt` as `EXPECTED_IDF_VERSION`). The ESP32 runs in **unicore mode** (`CONFIG_FREERTOS_UNICORE=y`).

## Architecture

- **`main/`** — All application logic (flat directory, no subdirs). Key subsystems:
  - `ruuvi_gateway_main.c` — Entry point: initializes NVS, nRF52 FW update, WiFi/Ethernet, HTTP server, starts tasks
  - `adv_post_*` / `adv_mqtt_*` — BLE advertisement forwarding via HTTP POST and MQTT (task/signals/timers/events pattern)
  - `gw_cfg_*` — Gateway configuration: JSON parse/generate, storage (NVS), defaults, validation
  - `http_server_cb*` — HTTP server callbacks for the configuration web UI
  - `nrf52fw.*` / `nrf52swd.*` — nRF52 firmware update over SWD from ESP32
  - `network_subsystem.*` — WiFi + Ethernet management
- **`components/`** — Git submodules and ESP-IDF component overrides (esp-tls, mbedtls, fatfs, mqtt, nvs_flash, esp32-wifi-manager, etc.)
- **`ruuvi.gwui.html/`** — Web configurator UI (submodule, see its own `AGENTS.md`). ES modules (.mjs) + jQuery, built with Webpack, served from FATFS partition. Configures Wi-Fi, Ethernet, HTTP/MQTT forwarding, BLE scanning, NTP, firmware updates, remote config. Has its own test suite (`npm test` — Mocha/Chai/Sinon) and a Python gateway simulator (`scripts/ruuvi_gw_http_server.py` on port 8001, password `00:11:22:33:44:55:66:77`)
- **`schemas/`** — JSON schemas for gateway config, HTTP/MQTT data formats, status. Validated via `check_schemas.sh`
- **`tests/`** — Host-side unit tests (independent CMake project, see `tests/AGENTS.md`)

## Build Environment Setup

**Environment setup (required before compiling):** source the ESP-IDF environment script first:
```bash
source ~/esp-idf-env.sh
```
This script sets `IDF_PATH` to `~/esp-idf-v4.2.5`, configures Python 3.8 as the default `python`, and runs ESP-IDF's `export.sh` to add the toolchain to `PATH`.

Alternatively, you can set up manually:
```bash
export IDF_PATH="$HOME/esp-idf-v4.2.5"
source "$IDF_PATH/export.sh"
```

### Prerequisites (Ubuntu 22.04)
```bash
sudo apt-get install -y gcc g++ cmake make ninja-build mtools
sudo apt-get install -y git wget libncurses-dev flex bison gperf ccache libffi-dev libssl-dev
sudo apt-get install -y locales
sudo locale-gen de_DE.UTF-8   # Required by some unit tests
```

### Installing ESP-IDF v4.2.5
```bash
git clone --recursive --branch v4.2.5 https://github.com/espressif/esp-idf.git ~/esp-idf-v4.2.5
cd ~/esp-idf-v4.2.5
./install.sh
```

### Python dependency
The build requires **Python 3.8**. The `bincopy` pip package is needed for firmware builds:
```bash
python -m pip install bincopy
```

## Build Commands

```bash
# Source ESP-IDF environment first
source ~/esp-idf-env.sh

# Build firmware
idf.py build

# Flash and monitor (115200 baud)
idf.py -p /dev/ttyUSB0 flash monitor

# Reproducible release build (builds twice with touch to remove absolute paths)
# All binaries except bootloader are reproducible
idf.py build
touch CMakeLists.txt
idf.py build

# Or use the script:
./build_release.sh

# Unit tests (host-side, Google Test, NOT on target)
cd tests
cmake -S . -B cmake-build-unit-tests -G Ninja
cmake --build cmake-build-unit-tests
ctest --test-dir cmake-build-unit-tests --output-on-failure

# Run a single test suite
ctest --test-dir cmake-build-unit-tests -R test_gw_cfg --output-on-failure

# Web UI (in ruuvi.gwui.html/)
cd ruuvi.gwui.html
npm install
npm test              # Mocha/Chai/Sinon unit tests
npm run build-dev     # Webpack dev build
npm run build-prod    # Webpack prod build (targeting Safari 12)
```

## Testing

Unit tests live in `tests/test_<module>/` — each mirrors a source file from `main/`. Tests are **Google Test (C++)** compiled for the host using a **POSIX FreeRTOS simulator** (`tests/posix_gcc_simulator/`). ESP-IDF APIs are stubbed in `tests/esp_simul/`. Each test file wraps C function stubs in `extern "C" {}` blocks. Test binaries are named `ruuvi_gateway_esp-test-<module>`.

The `tests/` directory is an **independent CMake project** that can be opened separately in an IDE. See `tests/AGENTS.md` for details.

## Code Style

- **BARR-C:2018** style enforced via `.clang-format` (clang-format v14)
- Return type on its own line (`AlwaysBreakAfterReturnType: All`)
- Braces on new lines for all blocks
- `snake_case` for functions/variables, prefixed by module (e.g., `adv_table_init`, `gw_cfg_json_parse`)
- Static tag pattern: `static const char TAG[] = "module_name";`
- Header guards: `#ifndef RUUVI_MODULE_H` / `#define RUUVI_MODULE_H`
- Format all files: `scripts/clang_format_all.sh`
- Verify: `git diff --exit-code --diff-filter=d --color`

## Key Conventions

- **Partition layout** (`partitions.csv`): dual OTA slots (`ota_0`/`ota_1`), dual FATFS for web UI and nRF52 FW, plus `gw_cfg_def` for default config
- **nRF52 FW config** in top-level `CMakeLists.txt`: `RUUVI_NRF52_FW_CONFIG` selects production (downloads from GitHub releases) or dev (local `../ruuvi.gateway_nrf.c/`)
- **Submodules** are critical — always `git submodule update --init --recursive` after clone
- **JSON handling** uses cJSON (`cjson_wrap.*`) and a streaming generator (`ruuvi.json_stream_gen.c`)
- **Secure boot** — firmware builds require a signing key (`~/.signing_keys/secure_boot_signing_key.pem`), provisioned from `SECURE_BOOT_SIGNING_KEY` secret in CI

## CI / GitHub Actions (`.github/workflows/`)

Eight workflows run on push/PR:

1. **`google-tests.yml` (Google Tests)** — on ubuntu-22.04, sets up Python 3.8, installs ESP-IDF v4.2.5 (cached at `~/esp/esp-idf`), builds tests with Ninja in `tests/cmake-build-unit-tests`, runs `ctest --output-on-failure`. Requires `de_DE.UTF-8` locale.
2. **`code-style.yml` (Clang-Format)** — installs clang-format-14, runs `scripts/clang_format_all.sh`, and fails if any file changes (`git diff --exit-code`).
3. **`check-schemas.yml` (Check JSON Schemas)** — installs `check-jsonschema` pip package, runs `schemas/check_schemas.sh`.
4. **`build-fw-dev.yml` (Build Firmware — dev)** — builds firmware in dev environment on push/PR, runs reproducible build (build → touch → build), uploads artifact with all binary images. Requires `bincopy` pip package and secure boot signing key.
5. **`build-fw-prod.yml` (Build Firmware — prod)** — same as dev but runs only on push to master/tags, uses prod environment.
6. **`sonar-scan.yml` (SonarCloud Analysis)** — builds firmware with SonarSource build-wrapper, builds tests (coverage is already enabled per-target via `target_compile_options` in each `test_*/CMakeLists.txt`), generates `gcovr -r . --sonarqube` coverage report, uploads to SonarCloud (project key configured in `sonar-project.properties`).
7. **`test-mbedtls.yml` (Test mbedTLS)** — runs mbedTLS test suites (`make test`) plus SSL sanitize/reduced-buffer/variable-buffer test scripts in `components/mbedtls/mbedtls/`.
8. **`test-nvs_flash.yml` (Test nvs_flash)** — builds and runs NVS host tests in `components/nvs_flash/test_nvs_host/` (`make -j`, then `./test_nvs -d yes exclude:[long]`). Requires `jsonschema` pip package.

Key CI details for reproducing locally:
- CI uses `ubuntu-22.04`, `gcc/g++`, `cmake`, `ninja-build`.
- ESP-IDF is cloned to `~/esp/esp-idf` in CI (vs `~/esp-idf-v4.2.5` locally).
- SonarCloud coverage uses `gcovr -r . --sonarqube` (not `lcov`) from the repository root.
- The `de_DE.UTF-8` locale is required by certain tests — install locally with `sudo locale-gen de_DE.UTF-8` if missing.

## Subprojects

| Subproject | Path | Description |
|---|---|---|
| Web UI | `ruuvi.gwui.html/` | Gateway configurator web interface (see `ruuvi.gwui.html/AGENTS.md`) |
| esp32-wifi-manager | `components/esp32-wifi-manager/` | Wi-Fi/HTTP server component (see `components/esp32-wifi-manager/AGENTS.md`) |
| esp32-wifi-manager tests | `components/esp32-wifi-manager/tests/` | Independent CMake test project (see `components/esp32-wifi-manager/tests/AGENTS.md`) |
| Unit tests | `tests/` | Main firmware unit tests, independent CMake project (see `tests/AGENTS.md`) |
