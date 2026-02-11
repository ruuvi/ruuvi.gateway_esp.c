#!/usr/bin/env bash

set -euo pipefail

# Create a new build directory for this configuration
mkdir -p cmake-build-variable_buffer
cd cmake-build-variable_buffer

# Configure with the user config file
cmake .. -DMBEDTLS_USER_CONFIG_FILE="../tests/configs/user-config-variable-buffer.h" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
    -DCMAKE_C_FLAGS="-fsanitize=address -g"

# Build and run tests
cmake --build . --target test_suite_ssl
cd tests && ./test_suite_ssl
