#!/usr/bin/env bash

set -euo pipefail

# Create a new build directory for this configuration
mkdir -p cmake-build-sanitize
cd cmake-build-sanitize

# Configure with the user config file
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g" -DCMAKE_C_FLAGS="-fsanitize=address -g"

# Build and run tests
cmake --build . --target test_suite_ssl
cd tests && ./test_suite_ssl
cd ..

cmake --build . --target test_suite_ssl_pre_allocated_buffers
cd tests && ./test_suite_ssl_pre_allocated_buffers

