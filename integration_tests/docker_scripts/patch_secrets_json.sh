#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

sed -i "s/\"hostname\": \"[^\"]*\"/\"hostname\": \"$HOSTNAME.local\"/" .test_results/secrets.json
