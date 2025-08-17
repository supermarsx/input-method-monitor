#include <catch2/catch_test_macros.hpp>
#include "../source/hotkey_registry.h"

TEST_CASE("Startup registry flag toggles") {
    g_startupEnabled = false;
    AddToStartup();
    REQUIRE(g_startupEnabled);
    RemoveFromStartup();
    REQUIRE_FALSE(g_startupEnabled);
}

