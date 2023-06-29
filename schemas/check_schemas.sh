#!/bin/bash

# https://check-jsonschema.readthedocs.io/en/latest/
# https://github.com/python-jsonschema/check-jsonschema
# pip install check-jsonschema

set -e -x

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

SCHEMAS_DIR=$SCRIPT_DIR
SCHEMA_RUUVI_GW_CFG=$SCHEMAS_DIR/ruuvi_gw_cfg.schema.json
EXAMPLES=$SCRIPT_DIR/examples

check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$SCRIPT_DIR/../gw_cfg_default/gw_cfg_default.json"

check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_remote_cfg_auth_none.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_remote_cfg_auth_no.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_remote_cfg_auth_basic.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_remote_cfg_auth_bearer.json"

check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_http_auth_none.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_http_auth_basic.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_http_auth_bearer.json"
check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/gw_cfg_http_auth_token.json"

check-jsonschema --schemafile "$SCHEMA_RUUVI_GW_CFG" "$EXAMPLES/ruuvi_1.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_gw_status.schema.json" "$EXAMPLES/gw_status_after_reboot.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_gw_status.schema.json" "$EXAMPLES/gw_status_reset_reason_panic.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_history.schema.json" "$EXAMPLES/history.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_history.schema.json" "$EXAMPLES/history_with_decoding_df5.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_http_data_with_timestamps.schema.json" "$EXAMPLES/http_data_with_timestamps.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_http_data_without_timestamps.schema.json" "$EXAMPLES/http_data_without_timestamps.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_mqtt_data_with_timestamps.schema.json" "$EXAMPLES/mqtt_data_with_timestamps.json"

check-jsonschema --schemafile "$SCHEMAS_DIR/ruuvi_mqtt_data_without_timestamps.schema.json" "$EXAMPLES/mqtt_data_without_timestamps.json"
