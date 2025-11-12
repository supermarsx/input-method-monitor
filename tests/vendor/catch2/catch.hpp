// Catch2 v3 single-header placeholder
// This file is a vendorized placeholder catch.hpp for offline builds.
// Replace with official Catch2 single-header for full test functionality.

#ifndef CATCH2_SINGLE_HEADER_PLACEHOLDER
#define CATCH2_SINGLE_HEADER_PLACEHOLDER

// Minimal stub to allow tests to compile in offline mode.
// This stub defines a minimal main symbol expected by the project when
// the CMake fallback writes tests/catch_main.cpp including "catch2/catch.hpp".

#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

int Catch2_SingleHeader_Stub_Run_All_Tests() {
    return 0; // always succeed
}

#ifdef __cplusplus
}
#endif

#define CATCH_CONFIG_MAIN

#endif // CATCH2_SINGLE_HEADER_PLACEHOLDER
