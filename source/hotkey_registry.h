#pragma once
#include <windows.h>
#include <atomic>

// Global variables
extern bool g_startupEnabled;
extern std::atomic<bool> g_languageHotKeyEnabled;
extern std::atomic<bool> g_layoutHotKeyEnabled;
extern std::atomic<bool> g_tempHotKeysEnabled;
extern DWORD g_tempHotKeyTimeout;

// Function pointers provided by the hook DLL
using SetLanguageHotKeyEnabledFunc = void(*)(bool);
using SetLayoutHotKeyEnabledFunc = void(*)(bool);

extern SetLanguageHotKeyEnabledFunc SetLanguageHotKeyEnabled;
extern SetLayoutHotKeyEnabledFunc SetLayoutHotKeyEnabled;

bool IsStartupEnabled();
void AddToStartup();
void RemoveFromStartup();

bool IsLanguageHotKeyEnabled();
bool IsLayoutHotKeyEnabled();

void ToggleLanguageHotKey(HWND hwnd, bool overrideState=false, bool state=false);
void ToggleLayoutHotKey(HWND hwnd, bool overrideState=false, bool state=false);
void TemporarilyEnableHotKeys(HWND hwnd);
void OnTimer(HWND hwnd);
