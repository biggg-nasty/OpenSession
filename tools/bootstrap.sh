#!/usr/bin/env bash
# Fetches the pinned Tracktion Engine + JUCE submodules and configures a build.
# The engine pins JUCE through an SSH URL; we rewrite it to HTTPS so no GitHub account is needed.
set -euo pipefail
cd "$(dirname "$0")/.."
git -c url.https://github.com/.insteadOf=git@github.com: submodule update --init --recursive --depth 1
BUILD_TYPE="${1:-Debug}"
cmake -S . -B "build${BUILD_TYPE/Debug/}" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
echo "Configured. Build with: cmake --build build${BUILD_TYPE/Debug/} -j"
