#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

TIMESTAMP=$(date +%Y-%m-%dT%H-%M-%S)
TEST_RESULTS_DIR=.test_results/$TIMESTAMP
mkdir -p "$TEST_RESULTS_DIR"
LOG_FILE=$TEST_RESULTS_DIR/${TIMESTAMP}_docker.log
exec > >(tee "$LOG_FILE") 2>&1

# Determine the network interface to use
PARENT_IFACE=$(nmcli device status | grep 'ethernet' | grep 'connected' | awk '{print $1}')
if [ -z "$PARENT_IFACE" ]; then
    echo "Failed to retrieve network interface."
    exit 1
fi

# Dynamically get the IP address of the interface
IP_ADDR=$(ip -4 addr show "$PARENT_IFACE" | grep -oP '(?<=inet\s)\d+\.\d+\.\d+\.\d+')
if [ -z "$IP_ADDR" ]; then
    echo "Failed to retrieve IP address."
    exit 1
fi

# Get the network subnet by filtering lines specifically containing the IP address
NETWORK=$(ip -4 route list dev "$PARENT_IFACE" | grep "src $IP_ADDR" | grep -oP '\d+\.\d+\.\d+\.\d+/\d+')
if [ -z "$NETWORK" ]; then
    echo "Failed to retrieve network subnet."
    exit 1
fi

# Determine the default gateway
GATEWAY=$(ip -4 route show default | grep -oP '(?<=via\s)\d+\.\d+\.\d+\.\d+')
if [ -z "$GATEWAY" ]; then
    echo "Failed to retrieve gateway."
    exit 1
fi

# If any of the required settings are empty, abort.
if [ -z "$IP_ADDR" ] || [ -z "$NETWORK" ] || [ -z "$GATEWAY" ]; then
    echo "Failed to retrieve network settings."
    exit 1
fi

SERIAL_PORT=$(python3 ../ruuvi_gw_flash.py - --print_port)
if [ -z "$SERIAL_PORT" ]; then
    echo "Failed to retrieve serial port."
    exit 1
fi
echo "Serial port: '$SERIAL_PORT'"

RUUVI_GW_TESTING_IMAGE=ruuvi_gw_testing
CONTAINER_NAME=ruuvi_gw_testing-container

cleanup() {
    set +e
    docker cp $CONTAINER_NAME:/ruuvi/ruuvi.gateway_esp.c/integration_tests/.test_results/TST-007/logs "$TEST_RESULTS_DIR/TST-007"
    docker cp $CONTAINER_NAME:/ruuvi/ruuvi.gateway_esp.c/integration_tests/.test_results/TST-008/logs "$TEST_RESULTS_DIR/TST-008"
    docker stop $CONTAINER_NAME
    docker network rm ruuvi_gw_bridge_network
    pushd .test_results
    zip -r "ruuvi_gw_tests_${TIMESTAMP}_docker.zip" "$TIMESTAMP/"
    popd
}

trap cleanup EXIT

# Creating the macvlan network
docker network create -d macvlan \
    --subnet="$NETWORK" \
    --gateway="$GATEWAY" \
    -o parent="$PARENT_IFACE" \
    ruuvi_gw_bridge_network

docker run --rm -d -i -t --name $CONTAINER_NAME --network ruuvi_gw_bridge_network "--device=$SERIAL_PORT:/dev/ttyUSB0" $RUUVI_GW_TESTING_IMAGE

docker exec $CONTAINER_NAME /ruuvi/ruuvi.gateway_esp.c/integration_tests/docker_scripts/git_pull_and_update_submodules.sh

pushd ..
modified_files=$(git ls-files -m)
for file in $modified_files; do
    docker cp "$file" "$CONTAINER_NAME:/ruuvi/ruuvi.gateway_esp.c/$file"
done
popd

pushd ../ruuvi.gwui.html
modified_files=$(git ls-files -m)
for file in $modified_files; do
    docker cp "$file" "$CONTAINER_NAME:/ruuvi/ruuvi.gateway_esp.c/ruuvi.gwui.html/$file"
done
popd

docker exec $CONTAINER_NAME /ruuvi/ruuvi.gateway_esp.c/integration_tests/docker_scripts/start_avahi_daemon.sh
docker cp .test_results/secrets.json $CONTAINER_NAME:/ruuvi/ruuvi.gateway_esp.c/integration_tests/.test_results/secrets.json
docker exec $CONTAINER_NAME /ruuvi/ruuvi.gateway_esp.c/integration_tests/docker_scripts/patch_secrets_json.sh

docker exec $CONTAINER_NAME /ruuvi/ruuvi.gateway_esp.c/integration_tests/tst-007.sh --docker
docker exec $CONTAINER_NAME /ruuvi/ruuvi.gateway_esp.c/integration_tests/tst-008.sh --docker
