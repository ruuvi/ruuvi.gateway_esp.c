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

pushd $REPO_DIR
git pull && git submodule update --init --recursive
cd ruuvi.gwui.html
npm install
popd
