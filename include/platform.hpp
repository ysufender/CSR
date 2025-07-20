#pragma once

#include <filesystem>
#include <string_view>

#if defined(_WIN32) || defined(__CYGWIN__)
    #include <windows.h>

    using dlID_t = HINSTANCE;
    using sym_t = FARPROC
#elif defined(unix) || defined(__unix) || defined(__unix__)
    #include <dlfcn.h>
    #include <unistd.h>
    #include <climits>

    using dlID_t = void*;
    using sym_t = void*;
#elif defined(__APPLE__) || defined(__MACH__)
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <climits>

    using dlID_t = void*;
    using sym_t = void*;
#endif

#define SYSFN (*)

dlID_t DLLoad(std::string_view path);
bool DLUnload(dlID_t dlID);
std::filesystem::path GetExePath();

template<typename T>
T DLSym(dlID_t dlID, std::string_view name)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    return reinterpret_cast<T>(GetProcAddress(dlID, name.data()));
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
    return reinterpret_cast<T>(dlsym(dlID, name.data()));
#endif   
}
