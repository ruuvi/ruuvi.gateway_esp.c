#!/bin/bash

# Check if mosquitto_sub is installed
if ! command -v mosquitto_sub &>/dev/null
then
    echo "mosquitto_sub could not be found"
    echo "To install mosquitto_sub:"

    case "$(uname -s)" in
        Linux*)
            echo "On Linux:"
            echo "    sudo apt-get update && sudo apt-get install mosquitto-clients"
            ;;
        Darwin*)
            echo "On macOS:"
            echo "    brew install mosquitto"
            ;;
        *)
            echo "Could not detect the OS or OS not supported. Please install mosquitto_sub manually."
            ;;
    esac

    exit
fi

# Enable printing command to stderr
set -x
# Set exit on error
set -e

SCRIPT_ABS_PATH=$(realpath "$0")
FILENAME=$(basename "$0" .sh)
TEST_NAME=$(echo "$FILENAME" | tr '[:lower:]' '[:upper:]')

ROOT_ABS_PATH=$(realpath "$SCRIPT_ABS_PATH/..")

RUUVI_GW_FLASH=$ROOT_ABS_PATH/ruuvi_gw_flash.py

GWUI=$ROOT_ABS_PATH/ruuvi.gwui.html
GWUI_SCRIPTS=$GWUI/scripts

pushd "$GWUI"
npm install
popd

if [ ! -d "$GWUI_SCRIPTS/.venv" ]; then
    pushd "$GWUI_SCRIPTS"
    python3 -m venv .venv
    source ".venv/bin/activate"
    pip3 install -r requirements.txt
    deactivate
    popd
fi

TEST_RESULTS_ASB_PATH=$ROOT_ABS_PATH/integration_tests/.test_results
SECRETS_JSON=$TEST_RESULTS_ASB_PATH/secrets.json

TEST_RESULTS_REL_PATH=.test_results/$TEST_NAME
mkdir -p "$TEST_RESULTS_REL_PATH"
mkdir -p "$TEST_RESULTS_REL_PATH/logs"

pushd "$TEST_RESULTS_REL_PATH"
TEST_RESULTS_ABS_PATH=$(pwd)
TIMESTAMP=$(date +%Y-%m-%dT%H-%M-%S)
LOG_FILE=logs/$TIMESTAMP.log
exec > >(tee "$LOG_FILE") 2>&1
popd

PID_LOG_UART_FILE=$TEST_RESULTS_ABS_PATH/pid_gw_uart_log.txt

cleanup() {
    set +x +e
    echo "Cleaning up..."
    if [ -f "$PID_LOG_UART_FILE" ]; then
        echo "Stop UART logging"
        PID_LOG_UART=$(cat "$PID_LOG_UART_FILE")
        kill -INT "$PID_LOG_UART"
        sleep 1
        if kill -0 "$PID_LOG_UART" 2>/dev/null; then
            if [ -z "$RESULT_SUCCESS" ]; then
                sleep 10
            fi
            kill -9 "$PID_LOG_UART"
        fi
        rm "$PID_LOG_UART_FILE"
    fi
    if [ -n "$RESULT_SUCCESS" ]; then
        echo "Test passed"
    else
        echo "Test failed"
    fi
    cd "$TEST_RESULTS_ASB_PATH"
    zip -q -r "ruuvi_gw_tests_${TIMESTAMP}_${TEST_NAME}.zip" "$TEST_NAME/logs/"
}

trap cleanup EXIT

echo ========================================
cd "$TEST_RESULTS_ABS_PATH"
#python3 "$RUUVI_GW_FLASH" v1.15.0 --erase_flash --log
python3 "$RUUVI_GW_FLASH" - --reset --log --log_uart --log_dir ./logs >/dev/null 2>&1 </dev/null &
echo $!>"$PID_LOG_UART_FILE"
PID_LOG_UART=$(cat "$PID_LOG_UART_FILE")
sleep 2
if kill -0 "$PID_LOG_UART" 2>/dev/null; then
    echo "UART logging started"
  else
    echo "Failed to start UART logging"
    exit 1
fi

sleep 23

source "$GWUI_SCRIPTS/.venv/bin/activate"

pushd "$GWUI"

## TST-008-AE001: Test sending data to MQTT server via TCP
node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_tcp.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}" "$@"

## TST-008-AE002: Test sending data to MQTT server via SSL
node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_ssl.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}" "$@"

## TST-008-AE003: Test sending data to MQTT server via WebSocket
node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_ws.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}" "$@"

## TST-008-AE004: Test sending data to MQTT server via Secure WebSocket
node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_wss.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}" "$@"

## TST-008-AE005: Test sending data to AWS MQTT server
node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_aws.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}" "$@"

popd

RESULT_SUCCESS=1
