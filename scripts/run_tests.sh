#!/bin/sh
set -e

g++ -std=c++17 -I/usr/include/catch2 tests/test_configuration.cpp -o tests/run_tests -lCatch2Main -lCatch2 && ./tests/run_tests
