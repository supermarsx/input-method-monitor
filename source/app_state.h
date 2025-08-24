#pragma once
#include <atomic>
#include <mutex>
#include <memory>
#include <cstdint>
#include "utils.h"

struct AppState {
    std::atomic<bool> startupEnabled{false};
    std::atomic<bool> languageHotKeyEnabled{false};
    std::atomic<bool> layoutHotKeyEnabled{false};
    std::atomic<bool> tempHotKeysEnabled{false};
    std::atomic<uint32_t> tempHotKeyTimeout{10000};
    std::atomic<bool> debugEnabled{false};
    std::atomic<bool> trayIconEnabled{true};

    std::mutex tempHotKeyTimerMutex;
    std::unique_ptr<TimerGuard> tempHotKeyTimer;
};

AppState& GetAppState();
