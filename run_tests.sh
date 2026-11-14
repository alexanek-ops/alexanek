#!/usr/bin/env bash
# Builds and runs the JUCE-free core tests (needs only g++ or clang++).
set -euo pipefail
cd "$(dirname "$0")"
${CXX:-g++} -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
    test_core.cpp -o /tmp/bytebeat_test_core
/tmp/bytebeat_test_core
