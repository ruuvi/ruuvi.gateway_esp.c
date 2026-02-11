#!/usr/bin/env bash

# Create a new build directory for this configuration
mkdir -p cmake-build-reduced_buffer_size
cd cmake-build-reduced_buffer_size

# Configure with the user config file
cmake .. -DMBEDTLS_USER_CONFIG_FILE="../tests/configs/user-config-reduced-buffer-size.h" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
    -DCMAKE_C_FLAGS="-fsanitize=address -g"

# Build and run tests
cmake --build . --target test_suite_ssl
cd tests && ./test_suite_ssl

