#pragma once

#include <bit>
#include <bitset>
#include <cmath>
#include <iostream>
#include <iterator>
#include <type_traits>

#include "CSRConfig.hpp"

template<typename T>
concept byte_t = requires(){
    std::is_same_v<T, char>;
    std::is_same_v<T, uchar_t>;
};

template<std::integral T, byte_t U>
T IntegerFromBytes(const U* bytes) noexcept
{
    if (std::endian::native == std::endian::big)
        return *reinterpret_cast<T*>(const_cast<uchar_t*>(reinterpret_cast<const uchar_t*>(bytes)));

    // bytes must be in big endian order
   
    std::make_unsigned_t<T> ures { 0 };

    for (char i = 0; i < sizeof(T); i++)
    {
        ures <<= sizeof(uchar_t);
        ures |= static_cast<uchar_t>(bytes[i]);
    }

    return reinterpret_cast<T>(ures);
}

template<byte_t T>
float FloatFromBytes(const T* bytes) noexcept
{
    if (std::endian::native == std::endian::big)
        return *reinterpret_cast<float*>(const_cast<uchar_t*>(reinterpret_cast<const uchar_t*>(bytes)));

    float returnFloat;
    uchar_t* tmpB { reinterpret_cast<uchar_t*>(&returnFloat) };

    tmpB[0] = bytes[3];
    tmpB[1] = bytes[2];
    tmpB[2] = bytes[1];
    tmpB[3] = bytes[0];

    return returnFloat;
}

template<std::integral T, byte_t U = char>
U* BytesFromInteger(const T integer) noexcept
{
    if (std::endian::native == std::endian::big)
        return reinterpret_cast<U*>(new T{integer});

    // bytes will be in big endian order
    std::make_unsigned_t<T> uinteger { reinterpret_cast<std::make_unsigned_t<T>>(integer) };
    uchar_t* bytes { new uchar_t[sizeof(uinteger)] };

    for (char i = 0; i < sizeof(T); i++)
        bytes[i] = static_cast<uchar_t>((uinteger << ((sizeof(T)-i-1)*8)));

    return reinterpret_cast<U*>(bytes);
}

template<byte_t T>
T* BytesFromFloat(const float val) noexcept
{
    if (std::endian::native == std::endian::big)
        return reinterpret_cast<T*>(new float{val});

    uchar_t* bytes { new uchar_t[4] };
    const uchar_t* tmpB { reinterpret_cast<const uchar_t*>(&val) };

    bytes[0] = tmpB[3];
    bytes[1] = tmpB[2];
    bytes[2] = tmpB[1];
    bytes[3] = tmpB[0];

    return reinterpret_cast<T*>(bytes);
}
