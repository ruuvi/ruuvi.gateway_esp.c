#!/bin/bash

# https://check-jsonschema.readthedocs.io/en/latest/
# https://github.com/python-jsonschema/check-jsonschema
# pip install check-jsonschema

set -e -x

check-jsonschema --schemafile ./ruuvi_gw_cfg.schema.json ../gw_cfg_default/gw_cfg_default.json

check-jsonschema --schemafile ./ruuvi_gw_status.schema.json ./examples/gw_status_after_reboot.json

check-jsonschema --schemafile ./ruuvi_gw_status.schema.json ./examples/gw_status_reset_reason_panic.json

check-jsonschema --schemafile ./ruuvi_history.schema.json ./examples/history.json

check-jsonschema --schemafile ./ruuvi_http_data_with_timestamps.schema.json ./examples/http_data_with_timestamps.json

check-jsonschema --schemafile ./ruuvi_http_data_without_timestamps.schema.json ./examples/http_data_without_timestamps.json

check-jsonschema --schemafile ./ruuvi_mqtt_data_with_timestamps.schema.json ./examples/mqtt_data_with_timestamps.json

check-jsonschema --schemafile ./ruuvi_mqtt_data_without_timestamps.schema.json ./examples/mqtt_data_without_timestamps.json
