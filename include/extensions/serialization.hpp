#pragma once

#include <bit>
#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>

#include "CSRConfig.hpp"

template<std::integral T, typename U>
T IntegerFromBytes(const U* bytes)
{
    std::make_unsigned_t<T> ures { 0 };
    for (char i = 0; i < sizeof(T); i++)
    {
        ures <<= sizeof(uchar_t);
        ures |= static_cast<typeof(ures)>(bytes[i]);
    }
    return static_cast<T>(ures);
}


template<typename T>
concept iterable = requires(T& t)
{
    std::begin(t) != std::end(t);
    ++std::declval<decltype(std::begin(t))&>();
    *std::begin(t);
};


//
// Serialization
//
template<std::integral T>
void SerializeInteger(const T& data, std::ostream& stream)
{
    using uchar = std::make_unsigned_t<char>;
    using UT = std::make_unsigned_t<T>;

    if (std::endian::native == std::endian::little)
        stream.write(reinterpret_cast<const char*>(&data), sizeof(T));
    else
    {
        UT temp;

        for (char i = 0; i < sizeof(UT); i++)
        {
            uchar swapData { static_cast<uchar>(static_cast<uchar>(data) >> i*8) };
            temp <<= 8;
            temp |= swapData;
        }

        stream.write(reinterpret_cast<char*>(&temp), sizeof(temp));
    }
}



template<iterable ContT, std::integral SizeT, typename ElemT>
void SerializeContainer(
        const ContT& container, 
        std::ostream& stream,
        std::function<void(const ElemT&, std::ostream&)> serializer
)
{
    SizeT size { static_cast<SizeT>(container.size()) };
    SerializeInteger(size, stream);

    for (const ElemT& element : container)
       serializer(element, stream); 
}

//
// Deserialization
//
template<std::integral T>
void DeserializeInteger(T& data, std::istream& stream)
{
    using uchar = std::make_unsigned_t<char>;
    using UT = std::make_unsigned_t<T>;

    UT temp;
    stream.read(reinterpret_cast<char*>(&temp), sizeof(UT));

    if (std::endian::native == std::endian::little)
        data = temp;
    else
    {
        for (char i = 0; i < sizeof(UT); i++)
        {
            uchar swapData { static_cast<uchar>(temp >> i*8) };
            data <<= 8;
            data |= swapData;
        }
    }
}

template<iterable ContT, std::integral SizeT, typename ElemT>
void DeserializeContainer(
        ContT& container,
        std::istream& stream,
        std::function<void(ElemT&, std::istream&)> deserializer
)
{
    SizeT size;
    DeserializeInteger(size, stream);
    
    for (; size > 0; size--)
    {
        ElemT element;
        deserializer(element, stream);
        container.push_back(std::move(element));
    }
}
