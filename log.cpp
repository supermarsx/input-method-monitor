Log::write(const wchar_t* message) {
    if (!g_debugEnabled) return; // Exit if debug is not enabled

    std::lock_guard<std::mutex> guard(g_mutex);
    wchar_t logPath[MAX_PATH];
    GetModuleFileName(g_hInst, logPath, MAX_PATH);
    PathRemoveFileSpec(logPath);
    PathCombine(logPath, logPath, L"kbdlayoutmon.log");

    std::wofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    } else {
        OutputDebugString(L"Failed to open log file.");
    }
}