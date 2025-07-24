#include <cstdint>
#include <iostream>
#include <bit>

#if defined(_WIN32) || defined(__CYGWIN__)
#define API(type) extern "C" type __declspec(dllexport) __cdecl
#define HANDLER const char* const __cdecl
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
#define API(type) extern "C" type
#define HANDLER const char* const
#endif

uint32_t IntegerFromBytes(const char* bytes) noexcept
{
    if (std::endian::native == std::endian::big)
        return *const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(bytes)));

    uint32_t ures { 0 };

    for (uint32_t i = 0; i < sizeof(uint32_t); i++)
    {
        ures <<= sizeof(uint8_t)*8;
        ures |= static_cast<uint8_t>(bytes[i]);
    }

    return ures;
}

// Handler
HANDLER PrintLineHandler(const char* const params) noexcept
{
    uint32_t size { IntegerFromBytes(params) };
    std::cout << '\n' << std::string_view {params+4, size} << ", printed in C++!\n";
    return nullptr;
}

// Function pointer types
using binder_t = char (*)(void*, uint32_t, const char* const (*)(const char* const) noexcept) noexcept;
using unbinder_t = char (*)(void*, uint32_t) noexcept;

API(char) InitExtender(void* handler, binder_t binder, unbinder_t unbinder)
{
    binder(handler, 13, &PrintLineHandler);
    return 0;
}
