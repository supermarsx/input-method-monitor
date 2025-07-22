#!/bin/sh
set -e

HEADER="/usr/include/catch2/catch_test_macros.hpp"
if [ ! -f "$HEADER" ]; then
    echo "Catch2 headers not found. Install Catch2 (e.g. sudo apt install catch2)" >&2
    exit 1
fi

g++ -std=c++17 -I/usr/include/catch2 tests/test_configuration.cpp -o tests/run_tests -lCatch2Main -lCatch2
./tests/run_tests
