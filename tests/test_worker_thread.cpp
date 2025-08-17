#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <atomic>

void StartWorkerThread();
void StopWorkerThread();
extern std::atomic<bool> g_workerRunning;

TEST_CASE("Worker thread can be started and stopped concurrently") {
    std::thread starter([](){
        for (int i = 0; i < 1000; ++i)
            StartWorkerThread();
    });
    std::thread stopper([](){
        for (int i = 0; i < 1000; ++i)
            StopWorkerThread();
    });
    starter.join();
    stopper.join();
    StopWorkerThread();
    REQUIRE_FALSE(g_workerRunning.load());
}
