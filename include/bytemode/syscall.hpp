#pragma once

#include <unordered_set>
#include <unordered_map>

#include "CSRConfig.hpp"
#include "platform.hpp"
#include "nativecalls.hpp"

using SysFunctionMap = std::unordered_map<sysbit_t, SysFunctionHandler>;
using DLList = std::unordered_set<dlID_t>;

class SysCallHandler
{
    public:
        SysCallHandler();
        SysCallHandler(SysFunctionMap map);

        ~SysCallHandler();

        const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        char BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept;
        char UnbindFunction(sysbit_t id) noexcept;

        const SysFunctionHandler& operator[](sysbit_t id) const;

        Error operator()(sysbit_t id, char* params) const noexcept
        { return static_cast<Error>((*this)[id](params)); }

        dlID_t LoadDl(std::string_view dlPath);
        sysfnh_t MakeFunctionHandler(dlID_t dl, std::string_view functionName) const;

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

inline char SysCallBinder(void* scallH, sysbit_t id, SysFunctionHandler handler) noexcept
{
    return reinterpret_cast<SysCallHandler*>(scallH)->BindFunction(id, handler);
}

inline char SysCallUnbinder(void* scallH, sysbit_t id) noexcept
{
    return reinterpret_cast<SysCallHandler*>(scallH)->UnbindFunction(id);
}
