#!/bin/bash

# Enable printing command to stderr
set -x
# Set exit on error
set -e

mkdir -p .test_results

exec > >(tee ".test_results/build_docker_image.log") 2>&1

GITHUB_REPO_URL=https://github.com/ruuvi/ruuvi.gateway_esp.c.git
GITHUB_REPO_BRANCH=$(git branch --show-current)

if [ -z "$GITHUB_REPO_BRANCH" ]; then
    GITHUB_REPO_BRANCH=$(git rev-parse --abbrev-ref HEAD)
fi
if [ -z "$GITHUB_REPO_BRANCH" ]; then
    echo "Failed to determine the branch name."
    exit 1
fi

docker build \
  --build-arg GITHUB_REPO_URL=$GITHUB_REPO_URL \
  --build-arg GITHUB_REPO_BRANCH="$GITHUB_REPO_BRANCH" \
  -t ruuvi_gw_testing .
