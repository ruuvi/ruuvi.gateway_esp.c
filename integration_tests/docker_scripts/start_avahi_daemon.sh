#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

service avahi-daemon start
sleep 3
service avahi-daemon status

ping -c 2 ruuvigateway9c2c.local
ping -c 2 "$HOSTNAME.local"
