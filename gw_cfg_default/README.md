# `gw_cfg_default/` — Default Gateway Configuration

This directory contains the **default Ruuvi Gateway configuration** that is flashed to
the `gw_cfg_def` NVS partition on the ESP32 and used by the firmware on first boot
(and after a factory reset).

## Files

| File | Purpose |
|---|---|
| `gw_cfg_default.json` | Human-editable JSON with the default settings shipped on the device. **This is the source of truth for the `gw_cfg_def` partition.** |
| `gw_cfg_def_partition.csv` | NVS partition layout descriptor — tells `nvs_partition_gen.py` to store `gw_cfg_default.json` under namespace `ruuvi_gateway`, key `gw_cfg_default`. |
| `gw_cfg_default_gen.c` | Standalone host program that prints the **hardcoded** defaults compiled into the firmware (i.e. what `gw_cfg_default_get()` in `main/gw_cfg_default.c` produces) as JSON to `stdout`. |
| `CMakeLists.txt` | Builds `gw_cfg_default_gen.c` as a host executable against the same `main/` sources used by the firmware. |
| `run_and_capture.cmake` | Helper CMake script (invoked via `cmake -P`) that runs the generator, pipes its output through `jq`, blanks out runtime-only fields (`mqtt_prefix`, `mqtt_client_id`), and writes the pretty-printed result to `gw_cfg_default_gen.json`. |
| `gw_cfg_default_gen.json` | **Auto-generated** snapshot of the hardcoded defaults. Regenerated on every firmware build. Intended for review / diffing against `gw_cfg_default.json`. |

## How `gw_cfg_default.json` becomes the `gw_cfg_def` partition

The ESP32 firmware reserves a small NVS partition named `gw_cfg_def` (see
`partitions.csv`). The contents of `gw_cfg_default.json` are baked into that partition
image at build time and flashed to the device.

The flow is wired up in `components/gw_cfg_def/CMakeLists.txt`:

1. The top-level `CMakeLists.txt` parses `partitions.csv` to locate the `gw_cfg_def`
   partition and exports `GW_CFG_DEF_ADDR` and `GW_CFG_DEF_SIZE`.
2. `components/gw_cfg_def/CMakeLists.txt` invokes
   `scripts/nvs_partition_gen.py generate gw_cfg_default/gw_cfg_def_partition.csv
   build/gw_cfg_def.bin <SIZE>`. The CSV instructs the generator to store the file
   `gw_cfg_default.json` as a string under namespace `ruuvi_gateway`, key
   `gw_cfg_default`.
3. The resulting `gw_cfg_def.bin` is registered with `esptool_py_flash_target_image()`
   so `idf.py flash` writes it to the correct partition offset.
4. At runtime, the firmware reads this string from NVS via
   `gw_cfg_storage_read_file_as_string(GW_CFG_STORAGE_GW_CFG_DEFAULT)` (see
   `main/gw_cfg_storage.h` — `GW_CFG_STORAGE_GW_CFG_DEFAULT = "gw_cfg_default"`).
   The JSON is parsed and applied as the initial / factory-reset configuration.

### Manual regeneration of the partition image

```shell
python ./scripts/nvs_partition_gen.py \
       generate gw_cfg_default/gw_cfg_def_partition.csv \
       gw_cfg_def.bin 0x40000
```

### Creating a custom default configuration

1. Make a copy of the `gw_cfg_default/` folder (or simply edit
   `gw_cfg_default.json` in place).
2. Adjust the JSON to suit your deployment.
3. Rebuild and flash — the new defaults will be applied on first boot or after a
   factory reset.

## How `gw_cfg_default_gen.json` is generated

`main/gw_cfg_default.c` contains the **hardcoded** factory defaults compiled into the
firmware. These hardcoded values are the ultimate fallback used when the `gw_cfg_def`
partition is missing or unreadable, and they **should** stay in sync with
`gw_cfg_default.json` so that a user editing the JSON file sees exactly the values the
firmware would use by default.

To make any drift visible, the build automatically produces `gw_cfg_default_gen.json`:

1. The top-level `CMakeLists.txt` declares the host-side subproject as an
   `ExternalProject` named `gw_cfg_default`. Its `CMakeLists.txt` compiles
   `gw_cfg_default_gen.c` together with `main/gw_cfg_default.c`,
   `main/gw_cfg_json_generate*.c` and the necessary stubs, producing a host
   executable that prints the hardcoded defaults as JSON.
2. An `ExternalProject_Add_Step(... generate_default_json ...)` step runs after the
   build and invokes `run_and_capture.cmake` via `cmake -P`.
3. `run_and_capture.cmake` executes the pipeline:
   ```
   gw_cfg_default | jq '.mqtt_prefix = "" | .mqtt_client_id = ""'
   ```
   - `jq` pretty-prints the JSON with consistent formatting.
   - `mqtt_prefix` and `mqtt_client_id` are replaced with empty strings because at
     runtime they are derived from the gateway's MAC address and therefore differ
     between the host-generated snapshot and the value shipped in
     `gw_cfg_default.json`.
4. The result is written to `gw_cfg_default/gw_cfg_default_gen.json`.

The step uses `ALWAYS 1`, so the snapshot is refreshed on every firmware build.

### Purpose of `gw_cfg_default_gen.json`

- **Source-of-truth comparison.** Diffing `gw_cfg_default.json` against
  `gw_cfg_default_gen.json` immediately reveals whether the JSON shipped to the
  partition matches the C-level defaults compiled into the firmware. Any
  discrepancy is a bug (or an intentional override that should be documented).
- **Reference for users.** When tweaking `gw_cfg_default.json` for a custom build,
  developers can consult `gw_cfg_default_gen.json` to see every field the firmware
  actually understands, with the same default values the C code would assign.
- **Code-review aid.** Changes to default values in `main/gw_cfg_default.c` show up
  as changes in `gw_cfg_default_gen.json`, making the effect of a code change on the
  shipped configuration visible in the diff.

### Requirements

- `jq` must be available in `PATH`. The build fails with a clear error otherwise.
- The host toolchain (gcc + CMake + Ninja/Make) must be available — the same
  toolchain used to build the unit tests in `tests/`.

### Manual regeneration

The snapshot is normally produced automatically as part of `idf.py build`. To
regenerate it manually:

```shell
cmake -S gw_cfg_default -B build/gw_cfg_default
cmake --build build/gw_cfg_default
cmake -DGW_CFG_DEFAULT_EXE=$PWD/build/gw_cfg_default/gw_cfg_default \
      -DGW_CFG_DEFAULT_OUT=$PWD/gw_cfg_default/gw_cfg_default_gen.json \
      -P gw_cfg_default/run_and_capture.cmake
```

