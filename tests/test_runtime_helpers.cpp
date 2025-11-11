#include "windows_stub.h"
#include <atomic>

// Provide minimal adapters and definitions expected by tests/runtime when the hook DLL
// isn't linked. These are simple fallbacks used only for unit tests.

// The real definition of g_workerRunning lives in tests/stubs.cpp; declare it extern here.
extern std::atomic<bool> g_workerRunning;


void StartWorkerThread() {
    g_workerRunning.store(true);
}

void StopWorkerThread() {
    g_workerRunning.store(false);
}

// Provide a minimal ApplyConfig so tests that reference it can link.
void ApplyConfig(HWND hwnd) {}


