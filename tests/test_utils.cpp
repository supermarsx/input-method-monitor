#include <catch2/catch_test_macros.hpp>
#include <string>
#include "../source/utils.h"

TEST_CASE("QuotePath wraps the path in quotes") {
    std::wstring path = L"C:/Program Files/kbdlayoutmon.exe";
    REQUIRE(QuotePath(path) == L"\"C:/Program Files/kbdlayoutmon.exe\"");
}

TEST_CASE("QuotePath escapes internal quotes") {
    std::wstring path = L"C:/Program Files/kbd\"layout\"mon.exe";
    REQUIRE(QuotePath(path) == L"\"C:/Program Files/kbd\\\"layout\\\"mon.exe\"");
}

TEST_CASE("QuotePath preserves trailing spaces") {
    std::wstring path = L"C:/Program Files/kbdlayoutmon.exe  ";
    REQUIRE(QuotePath(path) == L"\"C:/Program Files/kbdlayoutmon.exe  \"");
}
