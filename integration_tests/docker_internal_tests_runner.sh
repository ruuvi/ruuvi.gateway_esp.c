#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

REPO_DIR=ruuvi.gateway_esp.c

if [ ! -d "$REPO_DIR" ]; then
    echo "Error: '$REPO_DIR' does not exist." >&2
    exit 1
fi

service avahi-daemon start

cd $REPO_DIR
git pull && git submodule update --init --recursive
cd ruuvi.gwui.html
npm install
cd ../integration_tests

service avahi-daemon status

ping -c 2 ruuvigateway9c2c.local
ping -c 2 "$HOSTNAME.local"

sed -i "s/\"hostname\": \"[^\"]*\"/\"hostname\": \"$HOSTNAME.local\"/" .test_results/secrets.json

./tst-007.sh --docker
./tst-008.sh --docker
