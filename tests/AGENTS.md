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
  - `sdkconfig.h` — SDK config header (copy from an existing test directory, e.g., `test_ruuvi_nvs/sdkconfig.h`)
  - C function stubs wrapped in `extern "C" {}` blocks
- Test binaries are named `ruuvi_gateway_esp-test-<module>`.
- Test modules include: `test_adv_table`, `test_adv_mqtt_*`, `test_adv_post_*`, `test_gw_cfg*`, `test_http_json`, `test_http_server_cb`, `test_leds*`, `test_nrf52fw`, `test_nrf52swd`, `test_mqtt_json`, `test_bin2hex`, `test_cjson_wrap`, `test_event_mgr`, `test_flashfatfs`, `test_hmac_sha256`, `test_json_ruuvi`, `test_metrics`, `test_ruuvi_auth`, `test_time_str`, `test_url_encode`, and more.

## Build and test workflow
- **Environment setup (required before compiling):** ESP-IDF v4.2.5 strictly requires Python 3.8.
- Set `IDF_PATH`, ensure `python` resolves to `python3.8`, and source ESP-IDF's `export.sh`.
  The recommended way is to use a convenience script — see
  [README.md § Build Environment Setup](../README.md#build-environment-setup) for the full script
  and setup instructions.
```bash
source ~/esp-idf-env.sh
```
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
- Generate coverage for a **single test module** (faster, targeted at one source file):
```bash
lcov --capture --directory cmake-build-unit-tests/test_<module> \
     --output-file cmake-build-unit-tests/coverage_<module>_all.info
lcov --extract cmake-build-unit-tests/coverage_<module>_all.info '*/main/<module>.c' \
     --output-file cmake-build-unit-tests/coverage_<module>.info
lcov --list cmake-build-unit-tests/coverage_<module>.info
```
- CI uses `gcovr -r . --sonarqube` for SonarCloud integration (not `lcov`).

## Test patterns to reuse
- Each test target compiles selected `../main/*.c` files directly plus stubs and wrapper libs (see any `test_*/CMakeLists.txt` for examples).
- ESP-IDF APIs are stubbed in `esp_simul/` — add new stubs there when testing modules that use additional ESP-IDF features.
- FreeRTOS primitives (tasks, queues, semaphores, timers) are provided by `posix_gcc_simulator/` using pthreads.
- Linker wraps for `malloc/calloc/free` are used in some tests for memory-leak detection.
- Each test file typically wraps C function stubs in `extern "C" {}` blocks at the top.

## Creating a new test module — step-by-step checklist

When adding a new test module `test_<module>` for `main/<module>.c`, **all** of the following
steps are required. Missing any step will cause a CMake or build failure.

### Step 1: Create the test directory with three files

```
tests/test_<module>/
├── CMakeLists.txt
├── sdkconfig.h
└── test_<module>.cpp
```

- **`sdkconfig.h`** — Copy from an existing test (e.g., `test_ruuvi_nvs/sdkconfig.h`).
  All tests share the same SDK config.
- **`CMakeLists.txt`** — Use an existing test as template. Key elements:
  ```cmake
  cmake_minimum_required(VERSION 3.13)

  project(ruuvi_gateway_esp-test-<module>)
  set(ProjectId ruuvi_gateway_esp-test-<module>)

  add_executable(${ProjectId}
          test_<module>.cpp
          ${RUUVI_GW_SRC}/<module>.c
          ${RUUVI_GW_SRC}/<module>.h
          ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
          ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
          ${RUUVI_ESP_WRAPPERS}/src/snprintf_with_esp_err_desc.c
          ${RUUVI_ESP_WRAPPERS}/include/snprintf_with_esp_err_desc.h
          ${RUUVI_ESP_WRAPPERS}/include/wrap_esp_err_to_name_r.h
  )

  set_target_properties(${ProjectId} PROPERTIES
          C_STANDARD 11
          CXX_STANDARD 14
  )

  target_include_directories(${ProjectId} PUBLIC
          ${gtest_SOURCE_DIR}/include
          ${gtest_SOURCE_DIR}
          ${RUUVI_GW_SRC}
          include
          ../include
          ${COMPONENTS}/nvs_flash/include
          ${COMPONENTS}/spi_flash/include
          ${CMAKE_CURRENT_SOURCE_DIR}
          $ENV{IDF_PATH}/components/esp_common/include
          $ENV{IDF_PATH}/components/spi_flash/include
  )

  target_compile_definitions(${ProjectId} PUBLIC
          RUUVI_TESTS_<MODULE_UPPER>=1
  )

  target_compile_options(${ProjectId} PUBLIC
          -g3
          -ggdb
          -fprofile-arcs
          -ftest-coverage
  )

  target_link_options(${ProjectId} PUBLIC
          --coverage
  )

  target_link_libraries(${ProjectId}
          gtest
          gtest_main
          gcov
          ruuvi_esp_wrappers-common_test_funcs
  )
  ```
  If the source file under test uses headers from other components (e.g.,
  `esp_http_client_stream.h`), add the corresponding component include path:
  ```cmake
  ${COMPONENTS}/esp_http_client/include
  ```
  The `${COMPONENTS}` variable points to `../components/`. Available component include
  directories include: `nvs_flash/include`, `spi_flash/include`, `esp_http_client/include`,
  `esp-tls`, `fatfs/src`, `mqtt/esp-mqtt/include`, `mbedtls/mbedtls/include`, etc.

### Step 2: Register in top-level `tests/CMakeLists.txt` — TWO separate entries

**⚠ CRITICAL: Both entries below are required. Missing either one will cause CMake failure.**

The top-level `tests/CMakeLists.txt` has **two separate blocks** and both must be updated:

1. **`add_subdirectory()` block** (approximately lines 58–101) — Defines and builds the target.
   Add in **alphabetical order** alongside existing entries:
   ```cmake
   add_subdirectory(test_<module>)
   ```

2. **`add_test()` block** (approximately lines 102+) — Registers the target with CTest.
   Add alongside existing entries:
   ```cmake
   add_test(NAME test_<module>
           COMMAND ruuvi_gateway_esp-test-<module>
               --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-<module>>/gtestresults.xml
   )
   ```

If `add_subdirectory()` is missing but `add_test()` is present, CMake will fail with:
```
CMake Error: Error evaluating generator expression:
  $<TARGET_FILE_DIR:ruuvi_gateway_esp-test-<module>>
  No target "ruuvi_gateway_esp-test-<module>"
```

### Step 3: Build and verify

Always do a **clean cmake reconfigure** after adding a new test module:
```bash
source ~/esp-idf-env.sh
cd tests
rm -rf cmake-build-unit-tests
cmake -S . -B cmake-build-unit-tests -G Ninja
cmake --build cmake-build-unit-tests --target ruuvi_gateway_esp-test-<module>
ctest --test-dir cmake-build-unit-tests -R test_<module> --output-on-failure
```

### Step 4: Check code coverage
```bash
lcov --capture --directory cmake-build-unit-tests/test_<module> \
     --output-file cmake-build-unit-tests/coverage_<module>_all.info
lcov --extract cmake-build-unit-tests/coverage_<module>_all.info '*/main/<module>.c' \
     --output-file cmake-build-unit-tests/coverage_<module>.info
lcov --list cmake-build-unit-tests/coverage_<module>.info
```

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

---

## Log assertion technique

**Always use `esp_log_wrapper_get_logs()` to assert log output** — it fetches *all* pending records as a
single string and clears the queue in one call. Do **not** use `ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD` /
`esp_log_wrapper_pop()` (per-record popping is brittle and verbose).

The format produced by `esp_log_wrapper_get_logs()` is:
```
"LEVEL tag: parsed_message\n"
```
where `LEVEL` is a single character (`E`, `W`, `I`, `D`, `V`).

### Log format by macro

| Macro | Level char | Format of `parsed_message` |
|---|---|---|
| `LOG_ERR(fmt, ...)` | `E` | `fmt` expanded (no file/line in message body) |
| `LOG_WARN(fmt, ...)` | `W` | `fmt` expanded |
| `LOG_INFO(fmt, ...)` | `I` | `fmt` expanded |
| `LOG_DBG(fmt, ...)` | `D` | `fmt` expanded (only emitted when `LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG`) |
| `LOG_ERR_ESP(err, fmt, ...)` | `E` | `"fmt, err=CODE (Error 0xHEX(DEC))"` |

`LOG_DUMP_DBG` / `LOG_DUMP_INFO` etc. are only emitted when the corresponding level is active.
At `LOG_LOCAL_LEVEL = LOG_LEVEL_INFO` (the default for production source), `LOG_DBG` and
`LOG_DUMP_DBG` produce **no log records** — do not assert them.

### LOG_ERR_ESP error string format

The `wrap_esp_err_to_name_r` mock (required in every test file that uses `LOG_ERR_ESP`) produces:
```c
"Error 0x%x(%d)"   // e.g., "Error 0x1102(4354)"
```
So `LOG_ERR_ESP(esp_err, "Some message '%s'", key)` produces the parsed message:
```
"Some message 'key', err=4354 (Error 0x1102(4354))"
```

### Common NVS error codes used in assertions

| Constant | Hex | Decimal |
|---|---|---|
| `ESP_ERR_NVS_NOT_FOUND` | `0x1102` | `4354` |
| `ESP_ERR_NVS_NOT_ENOUGH_SPACE` | `0x1105` | `4357` |

### Example assertion pattern

```cpp
ASSERT_EQ(
    "I gw_cfg: Read file 'cfg.json' as string from NVS\n"
    "E gw_cfg: Can't find string key 'cfg.json' in flash, err=4354 (Error 0x1102(4354))\n"
    "E gw_cfg: Failed to read file 'cfg.json' (string) from NVS\n",
    esp_log_wrapper_get_logs());
```

For success paths with no expected logs use `ASSERT_EQ("", esp_log_wrapper_get_logs())`.

---

## Memory leak detection pattern

Use a `MemAllocTrace` helper class (tracks all live `os_malloc`/`os_calloc` pointers) together with
`os_malloc` / `os_free_internal` / `os_calloc` mocks that call `m_mem_alloc_trace.add()` /
`.remove()`. After every test assert:
```cpp
ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
```
This catches any allocation that was not freed (e.g., early return without `str_buf_free_buf`).

`MemAllocTrace` uses `assert()` internally, so a double-free or invalid-free will abort the test
immediately with a clear failure.

### Malloc failure injection

The test class holds two counters:
```cpp
uint32_t m_malloc_cnt {};        // incremented on every os_malloc/os_calloc call
uint32_t m_malloc_fail_on_cnt {}; // if non-zero: return nullptr when cnt reaches this value
```
Set `m_malloc_fail_on_cnt = 1` to fail the **first** allocation in the call under test.
Set `m_malloc_fail_on_cnt = 2` to fail the **second**, etc.

**Important:** `LOG_ERR_ESP` internally calls `esp_err_to_name_with_alloc_str_buf` which does one
`os_malloc(120)` + one `os_free`. Count these allocations when choosing `m_malloc_fail_on_cnt`.
For functions that call `LOG_ERR` (without `_ESP`) no extra allocation occurs.

---

## In-memory NVS mock pattern

When testing modules that use NVS (`nvs_get_str`, `nvs_set_str`, `nvs_get_blob`, `nvs_set_blob`,
`nvs_erase_key`, `nvs_close`, `ruuvi_nvs_open_gw_cfg_storage`, etc.), implement a full in-memory
NVS simulation backed by a `std::map<std::string, std::string>` on the test class.

### Required test-class fields

```cpp
// In-memory NVS storage (key -> data string)
std::map<std::string, std::string> m_nvs_storage;

// Control flags
bool     m_nvs_open_fail {};           // make ruuvi_nvs_open_gw_cfg_storage return false
uint32_t m_nvs_get_str_call_cnt {};
uint32_t m_nvs_get_str_fail_on_cnt {}; // fail nvs_get_str on this call (0 = never)
uint32_t m_nvs_get_blob_call_cnt {};
uint32_t m_nvs_get_blob_fail_on_cnt {};
bool     m_nvs_set_str_fail {};        // make nvs_set_str return ESP_ERR_NVS_NOT_ENOUGH_SPACE
bool     m_nvs_set_blob_fail {};
bool     m_nvs_erase_key_fail {};      // make nvs_erase_key return ESP_ERR_NVS_NOT_FOUND

// Tracking counters (verify calls were made)
uint32_t m_nvs_close_call_cnt {};
uint32_t m_ruuvi_nvs_deinit_call_cnt {};
uint32_t m_ruuvi_nvs_erase_call_cnt {};
uint32_t m_ruuvi_nvs_init_call_cnt {};
```

Reset all fields to zero/false in `SetUp()`.

### Mock handle

```cpp
static const nvs_handle_t MOCK_NVS_VALID_HANDLE = 0xABCDEF42U;
```

`ruuvi_nvs_open_gw_cfg_storage` writes this into `*p_handle` on success.
`nvs_close` asserts the handle equals this value and increments `m_nvs_close_call_cnt`.

### Two-phase NVS read mocks

Both `nvs_get_str` and `nvs_get_blob` are called **twice** by the production code:
1. **Size probe**: `out_value == NULL` → write `*length` and return `ESP_OK` (or error if not found)
2. **Data read**: `out_value != NULL` → copy data and return `ESP_OK`

Key difference between string and blob:

| | `nvs_get_str` | `nvs_get_blob` |
|---|---|---|
| `*length` on probe | `val.size() + 1` (includes `\0`) | `val.size()` (no `\0`) |
| Null terminator | included in the stored data's length | caller adds `+1` manually |

Use `m_nvs_get_str_fail_on_cnt = 2` (or blob equivalent) to test the second-read failure path.

### `ruuvi_nvs_erase_gw_cfg_storage` mock

Must call `m_nvs_storage.clear()` to simulate erasing the NVS partition. This is essential for
testing functions like `gw_cfg_storage_deinit_erase_init` that rely on the partition being empty
after erase and written fresh afterwards.

---

## Static function testing strategy

Functions declared `static` in a `.c` file **cannot be called directly** from a C++ test file and
**must not** be listed in the public header. Test their code paths only through the **public API**
functions that call them.

### Decision tree

1. Is the function `static`? → Test only via its public callers.
2. Remove any direct test cases calling the static function by name.
3. Identify every error/success branch inside the static function.
4. For each branch, write a test that uses the public API with mock state that exercises that branch.

### Example: `gw_cfg_storage_read_string_from_nvs` (static)

The function has four branches: key not found, malloc failure, second read failure, success.
All four are exercised through `gw_cfg_storage_read_file_as_string`:

```cpp
// malloc failure inside static helper:
TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_malloc_fail)
{
    this->m_nvs_storage["my_key"] = "hello";
    this->m_malloc_fail_on_cnt    = 1;  // fail first malloc (file content)

    str_buf_t result = gw_cfg_storage_read_file_as_string("my_key");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'my_key' as string from NVS\n"
        "E gw_cfg: Can't allocate 6 bytes for file\n"
        "E gw_cfg: Failed to read file 'my_key' (string) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
```

---

## Typical test anatomy

```cpp
TEST_F(TestModule, test_function_scenario) // NOLINT
{
    // 1. Arrange — set mock state
    this->m_nvs_storage["key"] = "value";
    this->m_nvs_set_str_fail   = true;

    // 2. Act — call the function under test
    const bool result = function_under_test("key", "new_value");

    // 3. Assert — verify return value, side effects, logs, no leaks
    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I tag: Info log message\n"
        "E tag: Error log message, err=4357 (Error 0x1105(4357))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
```

Key assertions to include in every test:
- **Return value** — most important
- **NVS state** — verify `m_nvs_storage` entries were added/removed/unchanged
- **Call counts** — verify `m_nvs_close_call_cnt`, `m_ruuvi_nvs_*_call_cnt` as appropriate
- **Logs** — `ASSERT_EQ("...\n", esp_log_wrapper_get_logs())` — verifies exact log output and clears queue
- **Memory** — `ASSERT_TRUE(this->m_mem_alloc_trace.is_empty())` — verifies no leaks

For public API functions returning `str_buf_t` (heap-allocated buffer), always call
`str_buf_free_buf(&result)` before the memory leak assertion.


