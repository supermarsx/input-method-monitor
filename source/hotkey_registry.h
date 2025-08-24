#pragma once
#include <windows.h>

// Access shared application state
#include "app_state.h"

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
