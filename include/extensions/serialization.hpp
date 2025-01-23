#pragma once

#include <cmath>
#include <type_traits>

#include "CSRConfig.hpp"

template<std::integral T, typename U>
T IntegerFromBytes(const U* bytes) noexcept
{
    std::make_unsigned_t<T> ures { 0 };
    for (char i = 0; i < sizeof(T); i++)
    {
        ures <<= sizeof(uchar_t);
        ures |= static_cast<typeof(ures)>(bytes[i]);
    }
    return static_cast<T>(ures);
}
