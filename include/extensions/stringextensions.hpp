#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace Extensions::String 
{
    std::vector<std::string> Split(const std::string& str, char delimiter, bool removeTrailing = true);
    std::string Concat(const std::vector<std::string>& strings);
    std::string Join(const std::vector<std::string>& strings, char delimiter);
}
