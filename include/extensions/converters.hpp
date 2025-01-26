#pragma once

#include <bitset>
#include <cmath>
#include <type_traits>

#include "CSRConfig.hpp"

template<std::integral T, typename U>
T IntegerFromBytes(const U* bytes) noexcept
{
    // bytes must be in big endian order
    std::make_unsigned_t<T> ures { 0 };

    for (char i = 0; i < sizeof(T); i++)
    {
        ures <<= sizeof(uchar_t);
        ures |= static_cast<typeof(ures)>(bytes[i]);
    }

    return reinterpret_cast<T>(ures);
}

template<std::integral T, typename U>
char* BytesFromInteger(const T integer) noexcept
{
    // bytes will be in big endian order
    std::make_unsigned_t<T> uinteger { reinterpret_cast<std::make_unsigned_t<T>>(integer) };
    char* bytes { new char[sizeof(uinteger)] { 0 } };

    for (char i = 0; i < sizeof(T); i++)
        bytes[i] |= (uinteger >> (sizeof(T)-i-1));

    return bytes;
}
