#pragma once

#include "system.hpp"
#include <filesystem>
#include <string_view>

#if defined(_WIN32) || defined(__CYGWIN__)
#define CSR_WIN
    #include <windows.h>

    #define EXPORT __declspec(dllexport)

    using dlID_t = HINSTANCE;
    using sym_t = FARPROC
#elif defined(unix) || defined(__unix) || defined(__unix__)
#define CSR_UNIX
    #include <dlfcn.h>
    #include <unistd.h>
    #include <climits>

    #define EXPORT __attribute__((visibility("default")))

    using dlID_t = void*;
    using sym_t = void*;
#elif defined(__APPLE__) || defined(__MACH__)
#define CSR_APPLE
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <climits>

    #define EXPORT __attribute__((visibility("default")))

    using dlID_t = void*;
    using sym_t = void*;
#endif

dlID_t DLLoad(std::string_view path);
bool DLUnload(dlID_t dlID);
std::filesystem::path GetExePath();

template<typename T>
T DLSym(dlID_t dlID, std::string_view name)
{
    dlID_t addr;
#ifdef CSR_WIN
    addr = GetProcAddress(dlID, name.data());
    if (addr)
        return reinterpret_cast<T>(addr);
#elif defined(CSR_UNIX) || defined(CSR_APPLE)
    addr = dlsym(dlID, name.data());
    if (addr)
        return reinterpret_cast<T>(addr);
#endif   
    return nullptr;
}
