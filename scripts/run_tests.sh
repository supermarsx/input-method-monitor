#!/bin/sh
set -e

g++ -std=c++17 -I/usr/include/catch2 tests/*.cpp -o tests/run_tests -lCatch2Main -lCatch2 && ./tests/run_tests
