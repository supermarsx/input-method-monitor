#include <atomic>
#include <string>

std::atomic<bool> g_debugEnabled{false};
bool g_cliMode = false;
std::wstring g_cliIconPath;
std::wstring g_cliTrayTooltip;
