#include <catch2/catch_test_macros.hpp>
#include <string>
#include "../source/utils.h"

TEST_CASE("QuotePath wraps the path in quotes") {
    std::wstring path = L"C:/Program Files/kbdlayoutmon.exe";
    REQUIRE(QuotePath(path) == L"\"C:/Program Files/kbdlayoutmon.exe\"");
}
