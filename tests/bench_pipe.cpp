#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

TEST_CASE("Persistent handle reduces connection overhead", "[benchmark]") {
    namespace fs = std::filesystem;
    fs::path temp = fs::temp_directory_path() / "immon_bench.log";

    BENCHMARK("open/close for each write") {
        for (int i = 0; i < 100; ++i) {
            std::ofstream f(temp);
            f << "x";
        }
    };

    BENCHMARK("persistent open") {
        std::ofstream f(temp);
        for (int i = 0; i < 100; ++i) {
            f << "x";
        }
    };

    fs::remove(temp);
}
