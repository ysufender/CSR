#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "CSRConfig.hpp"

namespace Extensions::String 
{
    std::vector<std::string> Split(const std::string_view& str, char delimiter, bool removeTrailing = true);

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

    VM_INLINE size_t Hash(std::string_view str)
    {
        static constexpr auto hasher { std::hash<std::string_view>{} };;
        return hasher(str);
    }

    constexpr size_t ConstHash(std::string_view str)
    {
        if (str == "CSR_Println") return 1;
        if (str == "CSR_Print") return 2;
        if (str == "CSR_U32ToFloat") return 3;
        if (str == "CSR_I32ToFloat") return 4;
        if (str == "CSR_FloatToU32") return 5;
        if (str == "CSR_FloatToI32") return 6;
        if (str == "CSR_PrintU32") return 7;
        if (str == "CSR_Clock") return 8;

        return 0;
    }
}
