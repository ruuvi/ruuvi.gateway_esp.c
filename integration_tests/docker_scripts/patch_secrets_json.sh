#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

sed -i "s/\"hostname\": \"[^\"]*\"/\"hostname\": \"$HOSTNAME.local\"/" ruuvi.gateway_esp.c/integration_tests/.test_results/secrets.json
