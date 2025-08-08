#pragma once

#include "system.hpp"
#include <filesystem>
#include <string_view>

enum class Platform
{
    Win,
    Unix,
    Apple
};

#if defined(_WIN32) || defined(__CYGWIN__)
    #include <windows.h>

    using dlID_t = HINSTANCE;
    using sym_t = FARPROC
    constexpr Platform PlatformID = Platform::Win;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    #include <dlfcn.h>
    #include <unistd.h>
    #include <climits>

    using dlID_t = void*;
    using sym_t = void*;
    constexpr Platform PlatformID = Platform::Unix;
#elif defined(__APPLE__) || defined(__MACH__)
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <climits>

    using dlID_t = void*;
    using sym_t = void*;
    constexpr Platform PlatformID = Platform::Apple;
#endif

dlID_t DLLoad(std::string_view path);
bool DLUnload(dlID_t dlID);
std::filesystem::path GetExePath();

template<typename T>
T DLSym(dlID_t dlID, std::string_view name)
{
    dlID_t addr;
#if defined(_WIN32) || defined(__CYGWIN__)
    addr = GetProcAddress(dlID, name.data());
    if (addr)
        return reinterpret_cast<T>(addr);
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
    addr = dlsym(dlID, name.data());
    if (addr)
        return reinterpret_cast<T>(addr);
#endif   
    return nullptr;
}
