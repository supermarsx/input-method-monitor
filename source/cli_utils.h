#pragma once
#include <string>

// Return a short usage string describing supported options.
std::wstring GetUsageString();

// Log and display a message for unrecognized command line options.
void WarnUnrecognizedOption(const wchar_t* option);
