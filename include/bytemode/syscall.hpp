#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #include <windows.h>
#elif defined(unix) || defined(__unix) || defined(__unix__)
    #include <dlfcn.h>
#endif

#include <unordered_set>
#include <unordered_map>
#include <functional>

#include "CSRConfig.hpp"
#include "system.hpp"
#include "slice.hpp"
#include "cpu.hpp"

#if defined(_WIN32) || defined(__CYGWIN__)
    using sysfnh_t = Error (__cdecl*)(const Slice, CPU& cpu);
    using dllID_t = HINSTANCE;
    using DLLList = std::unordered_set<dllID_t>;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    using sysfnh_t = Error (*)(const Slice, CPU& cpu);
    using dllID_t = void*;
    using DLLList = std::unordered_set<dllID_t>;
#endif

using SysFunctionHandler = std::function<Error(const Slice, CPU& cpu)>;
using SysFunctionMap = std::unordered_map<sysbit_t, SysFunctionHandler>;

class SysCallHandler
{
    public:
        SysCallHandler() = default;
        SysCallHandler(SysFunctionMap&& map);

        const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        Error BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept;
        Error UnbindFunciton(sysbit_t id) noexcept;

        const SysFunctionHandler& operator[](sysbit_t id) const;

        Error operator()(sysbit_t id, const Slice params, CPU& cpu) const noexcept
        { return (*this)[id](params, cpu); }

        Error LoadDll(std::string_view dllPath) noexcept;
        Error MakeFunctionHandler(std::string_view functionName) const noexcept;

    private:
        SysFunctionMap boundFuncs; 
        DLLList dllList;

        sysbit_t GenerateNewFuncionID() const noexcept;
};
