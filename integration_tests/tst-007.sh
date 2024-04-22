#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

root_relative_path=".."
pushd "$root_relative_path" >/dev/null
root_abs_path=$(pwd)
popd >/dev/null

RUUVI_GW_FLASH=$root_abs_path/ruuvi_gw_flash.py

GWUI=$root_abs_path/ruuvi.gwui.html
GWUI_SCRIPTS=$GWUI/scripts
HTTP_SERVER_AUTH=$GWUI_SCRIPTS/http_server_auth.py

if [ ! -d "$GWUI_SCRIPTS/.venv" ]; then
    pushd "$GWUI_SCRIPTS"
    python3 -m venv .venv
    source ".venv/bin/activate"
    pip3 install -r requirements.txt
    deactivate
    popd
fi

SECRETS_JSON=$root_abs_path/integration_tests/.test_results/secrets.json

TEST_RESULTS_REL_PATH=.test_results/TST-007
mkdir -p $TEST_RESULTS_REL_PATH

pushd $TEST_RESULTS_REL_PATH
TEST_RESULTS_ABS_PATH=$(pwd)
LOG_FILE=$(date +%Y-%m-%dT%H-%M-%S).log
exec > >(tee "$LOG_FILE") 2>&1
openssl genrsa -out server_key.pem 2048
openssl req -new -key server_key.pem -out server_csr.pem -subj "/C=FI/ST=Uusimaa/L=Helsinki/O=Ruuvi/OU=/CN=my-https-server.local"
openssl x509 -req -in server_csr.pem -signkey server_key.pem -out server_cert.pem -days 365
openssl genrsa -out client_key.pem 2048
openssl req -new -key client_key.pem -out client_csr.pem -subj "/C=FI/ST=Uusimaa/L=Helsinki/O=Ruuvi/OU=/CN=ruuvigateway9c2c.local"
openssl x509 -req -in client_csr.pem -signkey client_key.pem -out client_cert.pem -days 365
popd

PID_LOG_UART_FILE=$TEST_RESULTS_ABS_PATH/pid_gw_uart_log.txt

cleanup() {
    set +e
    echo "Cleaning up..."
    if [ -n "$HTTP_SERVER_PORT" ]; then
        echo "Stop HTTP server on port ${HTTP_SERVER_PORT}"
        curl -X POST "http://localhost:${HTTP_SERVER_PORT}/kill"
        sleep 1
        read -r PID_HTTP_SERVER rest <<< "$(ps ax | grep "[h]ttp_server_auth.py --port ${HTTP_SERVER_PORT}")"
        if [[ -n $PID_HTTP_SERVER ]]; then
            kill -9 "$PID_HTTP_SERVER"
        fi
        unset HTTP_SERVER_PORT
    fi
    if [ -f "$PID_LOG_UART_FILE" ]; then
        echo "Stop UART logging"
        PID_LOG_UART=$(cat "$PID_LOG_UART_FILE")
        kill -INT "$PID_LOG_UART"
        sleep 10
        if [ -z "$RESULT_SUCCESS" ]; then
            sleep 10
        fi
        if kill -0 "$PID_LOG_UART" 2>/dev/null; then
            kill -9 "$PID_LOG_UART"
        fi
        rm "$PID_LOG_UART_FILE"
    fi
    if [ -n "$RESULT_SUCCESS" ]; then
        echo "Test passed"
    else
        echo "Test failed"
    fi
}

trap cleanup EXIT

cd "$TEST_RESULTS_ABS_PATH"
#python3 "$RUUVI_GW_FLASH" v1.15.0 --erase_flash --log
python3 "$RUUVI_GW_FLASH" - --reset --log --log_uart >/dev/null 2>&1 &
echo $!>"$PID_LOG_UART_FILE"

sleep 25

source "$GWUI_SCRIPTS/.venv/bin/activate"

# TST-007-AG001: Test sending data to HTTP server without authentication
HTTP_SERVER_PORT=8000
python3 "$HTTP_SERVER_AUTH" --port ${HTTP_SERVER_PORT} >/dev/null 2>&1 &
pushd "$GWUI"
node scripts/ruuvi_gw_ui_tester.js --config tests/test_http.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}"
popd
curl -X POST "http://localhost:${HTTP_SERVER_PORT}/kill" || true
unset HTTP_SERVER_PORT

# TST-007-AG002: Test sending data to HTTP server with authentication
HTTP_SERVER_PORT=8000
python3 "$HTTP_SERVER_AUTH" --port ${HTTP_SERVER_PORT} -u user1 -p "Qe1_z@3Rt1.j" >/dev/null 2>&1 &
pushd "$GWUI"
node scripts/ruuvi_gw_ui_tester.js --config tests/test_http_with_auth.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}"
popd
curl -X POST "http://localhost:${HTTP_SERVER_PORT}/kill" || true
unset HTTP_SERVER_PORT

# TST-007-AG003: Test sending data to local HTTPS server with the server certificate checking
HTTP_SERVER_PORT=8001
python3 "$HTTP_SERVER_AUTH" --port ${HTTP_SERVER_PORT} \
  --ssl_cert="${TEST_RESULTS_ABS_PATH}/server_cert.pem" \
  --ssl_key="${TEST_RESULTS_ABS_PATH}/server_key.pem" >/dev/null 2>&1 &
sleep 1
read -r PID_HTTP_SERVER rest <<< "$(ps ax | grep "[h]ttp_server_auth.py --port ${HTTP_SERVER_PORT}")"
[[ -z $PID_HTTP_SERVER ]] && exit 1
pushd "$GWUI"
node scripts/ruuvi_gw_ui_tester.js --config tests/test_https_with_server_cert_checking.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}"
popd
curl -X POST "http://localhost:${HTTP_SERVER_PORT}/kill" || true
unset HTTP_SERVER_PORT

# TST-007-AG004: Test sending data to local HTTPS server with the server and client certificates checking
#HTTP_SERVER_PORT=8001
#python3 "$HTTP_SERVER_AUTH" --port ${HTTP_SERVER_PORT} \
#  --ssl_cert="${TEST_RESULTS_ABS_PATH}/server_cert.pem" \
#  --ssl_key="${TEST_RESULTS_ABS_PATH}/server_key.pem" \
#  --ca_cert="${TEST_RESULTS_ABS_PATH}/client_cert.pem" >/dev/null 2>&1 &
#sleep 1
#read -r PID_HTTP_SERVER rest <<< "$(ps ax | grep "[h]ttp_server_auth.py --port ${HTTP_SERVER_PORT}")"
#[[ -z $PID_HTTP_SERVER ]] && exit 1
#pushd "$GWUI"
#node scripts/ruuvi_gw_ui_tester.js --config tests/test_https_with_server_and_client_cert_checking.yaml --secrets "$SECRETS_JSON" --dir_test "${TEST_RESULTS_ABS_PATH}"
#popd
#curl -X POST "http://localhost:${HTTP_SERVER_PORT}/kill" || true
#unset HTTP_SERVER_PORT

RESULT_SUCCESS=1
