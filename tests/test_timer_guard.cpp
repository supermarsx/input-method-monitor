#include <catch2/catch_test_macros.hpp>
#include "windows_stub.h"
#include "../source/utils.h"
#include <stdexcept>

static int killed = 0;

extern UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC);
extern BOOL (*pKillTimer)(HWND, UINT);

TEST_CASE("TimerGuard calls KillTimer on destruction even during exception") {
    killed = 0;
    pSetTimer = [](HWND, UINT, UINT, TIMERPROC) -> UINT { return 1; };
    pKillTimer = [](HWND, UINT) -> BOOL { ++killed; return TRUE; };
    try {
        TimerGuard guard(nullptr, 1, 10, nullptr);
        throw std::runtime_error("boom");
    } catch (const std::exception&) {
        // swallow
    }
    REQUIRE(killed == 1);
}

