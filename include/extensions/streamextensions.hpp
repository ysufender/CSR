#pragma once

#include <bit>
#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <cstring>

#include "system.hpp"

#define OStreamPos(stream, varName, errAct) \
    std::streamoff varName { stream.tellp() }; \
    if (varName == -1) [[unlikely]] errAct 

#define IStreamPos(stream, varName, errAct) \
    std::streamoff varName { stream.tellg() }; \
    if (varName == -1) [[unlikely]] errAct 

#pragma once


namespace Instructions { enum class OpCodes : uchar_t; }

namespace Extensions::Serialization
{
    template<typename T>
    concept integer = requires
    {
        std::is_integral_v<T> || std::is_same_v<T, Instructions::OpCodes>;
    };

    template<typename ContT, typename ElemT>
    concept pushable = requires(ContT c, ElemT e)
    {
        c.push_back(e);
    };

    template<typename ContT, typename ElemT>
    concept emplaceable = requires(ContT c, ElemT e)
    {
        c.emplace(e);
    };

    template<typename T, typename U>
    //concept container = requires(T& t)
    //{
    //    std::begin(t) != std::end(t);
    //    ++std::declval<decltype(std::begin(t))&>();
    //    *std::begin(t);
    //};
    concept container = requires
    { 
        requires pushable<T, U> || emplaceable<T, U>;
    };

    template<typename U>
    VM_INLINE constexpr U byteswap(U v) noexcept
    {
        static_assert(std::is_unsigned_v<U>);
        if constexpr (sizeof(U) == 1) return v;
        else if constexpr (sizeof(U) == 2)
        {
            return U((v & 0x00FFu) << 8 |
                     (v & 0xFF00u) >> 8);
        }
        else if constexpr (sizeof(U) == 4)
        {
            return U((v & 0x000000FFu) << 24 |
                     (v & 0x0000FF00u) <<  8 |
                     (v & 0x00FF0000u) >>  8 |
                     (v & 0xFF000000u) >> 24);
        }
        else if constexpr (sizeof(U) == 8)
        {
            return U((v & 0x00000000000000FFull) << 56 |
                     (v & 0x000000000000FF00ull) << 40 |
                     (v & 0x0000000000FF0000ull) << 24 |
                     (v & 0x00000000FF000000ull) <<  8 |
                     (v & 0x000000FF00000000ull) >>  8 |
                     (v & 0x0000FF0000000000ull) >> 24 |
                     (v & 0x00FF000000000000ull) >> 40 |
                     (v & 0xFF00000000000000ull) >> 56);
        }
        else
        {
            U out = 0;
            for (size_t i = 0; i < sizeof(U); ++i)
                out = (out << 8) | ((v >> (i * 8)) & 0xFF);
            return out;
        }
    }

    //
    // Deserialization
    //
    template<integer T>
    VM_INLINE void DeserializeInteger(T& data, std::istream& stream)
    {
        using UT = std::make_unsigned_t<T>;
        UT temp{};
        stream.read(reinterpret_cast<char*>(&temp), sizeof(temp));

        if constexpr (std::endian::native == std::endian::little)
            temp = byteswap(temp);

        std::memcpy(&data, &temp, sizeof(T));
    }

    template<typename ContT, integer SizeT, typename ElemT>
        requires container<ContT, ElemT>
    VM_INLINE void DeserializeContainer(
        ContT& container,
        std::istream& stream,
        std::function<void(ElemT&, std::istream&)> deserializer
    )
    {
        SizeT size{};
        DeserializeInteger(size, stream);
        for (; size > 0; size--)
        {
            ElemT element;
            deserializer(element, stream);
            
            if constexpr (pushable<ContT, ElemT>)
                container.push_back(std::move(element));
            else if constexpr (emplaceable<ContT, ElemT>)
                container.emplace(std::move(element));
            else
                static_assert(sizeof(ContT) == 0, "Container must have push_back or emplace function.");
        }
    }
}
