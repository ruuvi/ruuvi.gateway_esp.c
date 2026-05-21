# AGENTS.md — Ruuvi Gateway ESP32 Unit Tests

## Project overview
- This directory is an **independent CMake project** (`ruuvi_gateway_esp-tests`) containing host-side unit tests for the [Ruuvi Gateway ESP32 firmware](https://github.com/ruuvi/ruuvi.gateway_esp.c).
- Tests run on the **host machine** (not on ESP32 target) using **Google Test (C++)** and a **POSIX FreeRTOS simulator**.
- The project can be opened independently in an IDE (CLion, VS Code, etc.) — it has its own `CMakeLists.txt` as the project root.
- Parent firmware source lives in `../main/`; component source in `../components/`.

## Scope and entry points
- Each test subdirectory (`test_<module>/`) mirrors a source file from `../main/`.
- Start reading from `CMakeLists.txt` to see all test targets and their registrations.
- Key infrastructure directories:
  - `esp_simul/` — Stubs for ESP-IDF APIs (NVS, WiFi, event loop, etc.)
  - `posix_gcc_simulator/` — POSIX FreeRTOS simulator (tasks, queues, semaphores, timers on pthreads)
  - `googletest/` — Google Test/Mock framework (submodule)
  - `lwip/` — lwIP stubs for host compilation

## Test structure
- Each `test_<module>/` directory contains:
  - `CMakeLists.txt` — Builds a test executable, compiles selected `../main/*.c` files directly plus stubs
  - `test_<module>.cpp` — Google Test file with `TEST_F` / `TEST` macros
  - C function stubs wrapped in `extern "C" {}` blocks
- Test binaries are named `ruuvi_gateway_esp-test-<module>`.
- Test modules include: `test_adv_table`, `test_adv_mqtt_*`, `test_adv_post_*`, `test_gw_cfg*`, `test_http_json`, `test_http_server_cb`, `test_leds*`, `test_nrf52fw`, `test_nrf52swd`, `test_mqtt_json`, `test_bin2hex`, `test_cjson_wrap`, `test_event_mgr`, `test_flashfatfs`, `test_hmac_sha256`, `test_json_ruuvi`, `test_metrics`, `test_ruuvi_auth`, `test_time_str`, `test_url_encode`, and more.

## Build and test workflow
- **Environment setup (required before compiling):** source the ESP-IDF environment script first:
```bash
source ~/esp-idf-env.sh
```
  This script sets `IDF_PATH` to `~/esp-idf-v4.2.5`, configures Python 3.8 as the default `python`, and runs ESP-IDF's `export.sh` to add the toolchain to `PATH`.
- Typical local flow from this directory:
```bash
cmake -S . -B cmake-build-unit-tests -G Ninja
cmake --build cmake-build-unit-tests
ctest --test-dir cmake-build-unit-tests --output-on-failure
```
- Run one suite while iterating:
```bash
ctest --test-dir cmake-build-unit-tests -R test_gw_cfg --output-on-failure
```
- Alternative build directory names: `build/` (Makefiles) or `cmake-build-unit-tests/` (Ninja).

## Code coverage
- Coverage is **already enabled** in every test target — each `test_*/CMakeLists.txt` adds `-fprofile-arcs -ftest-coverage --coverage` via `target_compile_options` and links with `gcov --coverage`. No extra CMake flags are needed.
- Standard build already produces `.gcno`/`.gcda` files:
```bash
cmake -S . -B cmake-build-unit-tests -G Ninja
cmake --build cmake-build-unit-tests
ctest --test-dir cmake-build-unit-tests --output-on-failure
```
- Generate a local coverage report with `lcov` and `genhtml`:
```bash
lcov --capture --directory cmake-build-unit-tests --output-file cmake-build-unit-tests/coverage_all.info
lcov --extract cmake-build-unit-tests/coverage_all.info '*/main/*' \
     --output-file cmake-build-unit-tests/coverage_main.info
lcov --list cmake-build-unit-tests/coverage_main.info
genhtml cmake-build-unit-tests/coverage_main.info --output-directory cmake-build-unit-tests/coverage_html
```
- CI uses `gcovr -r . --sonarqube` for SonarCloud integration (not `lcov`).

## Test patterns to reuse
- Each test target compiles selected `../main/*.c` files directly plus stubs and wrapper libs (see any `test_*/CMakeLists.txt` for examples).
- ESP-IDF APIs are stubbed in `esp_simul/` — add new stubs there when testing modules that use additional ESP-IDF features.
- FreeRTOS primitives (tasks, queues, semaphores, timers) are provided by `posix_gcc_simulator/` using pthreads.
- Linker wraps for `malloc/calloc/free` are used in some tests for memory-leak detection.
- Each test file typically wraps C function stubs in `extern "C" {}` blocks at the top.

## Project conventions
- Use `os_*` wrappers (`os_mutex`, `os_timer`, `os_malloc`, `os_task`) from `ruuvi.esp_wrappers.c` instead of raw OS/libc APIs.
- The `RUUVI_TESTS=1` and `_GNU_SOURCE` defines are set globally for all test targets.
- Component paths are referenced via CMake variables: `RUUVI_GW_SRC` (main source), `WIFI_MANAGER_SRC`, `RUUVI_ESP_WRAPPERS`, `RUUVI_JSON_STREAM_GEN`, etc.

## Code style
- Code is formatted with **clang-format 14** using the `../.clang-format` config (based on BARR-C:2018).
- To reformat all source and test files from the project root:
```bash
cd .. && scripts/clang_format_all.sh
```
- After formatting, verify no diff remains: `git diff --exit-code`.

## CI / GitHub Actions (`../.github/workflows/`)

Relevant workflows:

1. **`google-tests.yml` (Google Tests)** — on ubuntu-22.04, sets up Python 3.8, installs ESP-IDF v4.2.5 (cached at `~/esp/esp-idf`), builds tests with Ninja in `tests/cmake-build-unit-tests`, runs `ctest --output-on-failure`. Requires `de_DE.UTF-8` locale. Install locally with `sudo locale-gen de_DE.UTF-8`.
2. **`sonar-scan.yml` (SonarCloud Analysis)** — builds tests in `tests/build` (coverage is already enabled per-target), runs `ctest`, then generates `gcovr -r . --sonarqube` from the repository root for SonarCloud upload.
3. **`code-style.yml` (Clang-Format)** — installs clang-format-14, runs `scripts/clang_format_all.sh`, and fails if any file changes.

Key CI details for reproducing locally:
- CI uses `ubuntu-22.04`, `gcc/g++`, `cmake`, `ninja-build`.
- ESP-IDF is cloned to `~/esp/esp-idf` in CI (vs `~/esp-idf-v4.2.5` locally).
- Required system packages: `gcc g++ cmake make ninja-build mtools git wget libncurses-dev flex bison gperf ccache libffi-dev libssl-dev`.
- The `de_DE.UTF-8` locale is required by certain tests — install locally with `sudo locale-gen de_DE.UTF-8` if missing.

