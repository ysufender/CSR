#pragma once

#include <unordered_set>
#include <unordered_map>
#include <functional>

#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"


// returned char array is special
// arr[0] is ErrorCode,
// arr[1] is returned value size in bytes
// rest is returned data 
using SysFunctionHandler = std::function<const char* const(const char* const)>;

using SysFunctionMap = std::unordered_map<sysbit_t, SysFunctionHandler>;
using sysfnh_t = const char* const SYSFN(const char* const);
using DLList = std::unordered_set<dlID_t>;

// Interface for dynamic libraries to bind functions and extend the script capabilities
class ISysCallHandler
{
    public:
        virtual Error BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept = 0;
        virtual Error UnbindFunction(sysbit_t id) noexcept = 0;
};

// Initializer function signature for extender DLs
// name must be specifically InitExtender
using extInit_t = bool SYSFN (ISysCallHandler*);

class SysCallHandler : public ISysCallHandler
{
    public:
        SysCallHandler();
        SysCallHandler(SysFunctionMap map);

        const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        Error BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept override;
        Error UnbindFunction(sysbit_t id) noexcept override;

        const SysFunctionHandler& operator[](sysbit_t id) const;

        const char* const operator()(sysbit_t id, const char* const params) const noexcept
        { return (*this)[id](params); }

        dlID_t LoadDl(std::string_view dlPath);
        sysfnh_t MakeFunctionHandler(dlID_t dl, std::string_view functionName) const;

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};
