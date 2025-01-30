#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Extensions::String 
{
    std::vector<std::string> Split(const std::string& str, char delimiter, bool removeTrailing = true);

    // I don't know how this works.
    // not the concept part the template<string T> Concat part
    template<typename T>
    concept string = requires(){
        std::is_same_v<T, std::string>;
        std::is_same_v<T, std::string_view>;
    };
    template<string T>
    std::string Concat(const std::vector<std::string>& strings);
    std::string Concat(const std::vector<std::string_view>& strings);

    std::string Join(const std::vector<std::string>& strings, char delimiter);
}
