#pragma once
#include "CSRConfig.hpp"
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <type_traits>

template<typename T>
concept byte_t =
    std::same_as<T, char> ||
    std::same_as<T, unsigned char>;

template<typename U>
VM_INLINE constexpr U byteswap(U v) noexcept {
    static_assert(std::is_unsigned_v<U>);
    if constexpr (sizeof(U)==1) return v;
    else if constexpr (sizeof(U)==2) {
        return U((v & 0x00FFu)<<8 |
                 (v & 0xFF00u)>>8);
    }
    else if constexpr (sizeof(U)==4) {
        return U((v & 0x000000FFu)<<24 |
                 (v & 0x0000FF00u)<< 8 |
                 (v & 0x00FF0000u)>> 8 |
                 (v & 0xFF000000u)>>24);
    }
    else if constexpr (sizeof(U)==8) {
        return U((v & 0x00000000000000FFull)<<56 |
                 (v & 0x000000000000FF00ull)<<40 |
                 (v & 0x0000000000FF0000ull)<<24 |
                 (v & 0x00000000FF000000ull)<< 8 |
                 (v & 0x000000FF00000000ull)>> 8 |
                 (v & 0x0000FF0000000000ull)>>24 |
                 (v & 0x00FF000000000000ull)>>40 |
                 (v & 0xFF00000000000000ull)>>56);
    }
    else {
        U out = 0;
        for (size_t i = 0; i < sizeof(U); ++i)
            out = (out << 8) | ((v >> (i*8)) & 0xFF);
        return out;
    }
}

template<std::integral T, byte_t U>
VM_INLINE T IntegerFromBytes(const U* bytes) noexcept
{
    using UTI = std::make_unsigned_t<T>;
    UTI u{};
    std::memcpy(&u, bytes, sizeof(u));
    if constexpr (std::endian::native == std::endian::little)
        u = byteswap(u);
    return static_cast<T>(u);
}

template<std::integral T, byte_t U>
VM_INLINE void BytesFromInteger(const T integer, U* bytes) noexcept
{
    using UTI = std::make_unsigned_t<T>;
    UTI u = static_cast<UTI>(integer);
    if constexpr (std::endian::native == std::endian::little)
        u = byteswap(u);
    std::memcpy(bytes, &u, sizeof(u));
}

template<byte_t T>
VM_INLINE float FloatFromBytes(const T* bytes) noexcept
{
    uint32_t u{};
    std::memcpy(&u, bytes, sizeof(u));
    if constexpr (std::endian::native == std::endian::little)
        u = byteswap(u);
    return std::bit_cast<float>(u);
}

template<byte_t T>
VM_INLINE void BytesFromFloat(const float val, T* bytes) noexcept
{
    uint32_t u = std::bit_cast<uint32_t>(val);
    if constexpr (std::endian::native == std::endian::little)
        u = byteswap(u);
    std::memcpy(bytes, &u, sizeof(u));
}
