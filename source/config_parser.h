#pragma once

#include <map>
#include <string>
#include <vector>
#include <istream>

std::map<std::wstring, std::wstring> ParseConfigLines(const std::vector<std::wstring>& lines);
std::map<std::wstring, std::wstring> ParseConfigStream(std::wistream& stream);

