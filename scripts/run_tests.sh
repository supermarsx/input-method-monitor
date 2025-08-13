#!/bin/sh
set -e

# Verify that Catch2 is available before attempting to build tests.
if ! echo '#include <catch2/catch_test_macros.hpp>' | g++ -std=c++17 -x c++ - -fsyntax-only >/dev/null 2>&1; then
    echo "Catch2 not found. Install Catch2 (e.g. sudo apt install catch2) to build and run tests." >&2
    exit 1
fi

g++ -std=c++17 tests/test_configuration.cpp source/configuration.cpp -o tests/run_tests -lCatch2Main -lCatch2
./tests/run_tests
