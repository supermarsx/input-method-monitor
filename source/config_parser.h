#pragma once

#include <map>
#include <string>
#include <vector>
#include <istream>

// Parse a textual boolean value, returning L"1" for true and L"0" for false.
// Accepts common representations such as "1", "true", "yes" and "on".
std::wstring ParseBoolOrDefault(const std::wstring& value, bool def);

std::map<std::wstring, std::wstring> ParseConfigLines(const std::vector<std::wstring>& lines);
std::map<std::wstring, std::wstring> ParseConfigStream(std::wistream& stream);

