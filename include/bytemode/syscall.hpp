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
// must be freed by CSR
using SysFunctionHandler = const char* const SYSFN(const char* const) noexcept;
using sysfnh_t = SysFunctionHandler;

using SysFunctionMap = std::unordered_map<sysbit_t, SysFunctionHandler>;
using DLList = std::unordered_set<dlID_t>;

// Interface for dynamic libraries to bind functions and extend the script capabilities
class ISysCallHandler
{
    public:
        virtual char BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept = 0;
        virtual char UnbindFunction(sysbit_t id) noexcept = 0;
};

// Initializer function signature for extender DLs
// name must be specifically InitExtender
using extInit_t = char SYSFN (ISysCallHandler*) noexcept;

class SysCallHandler : public ISysCallHandler
{
    public:
        SysCallHandler();
        SysCallHandler(SysFunctionMap map);

        ~SysCallHandler();

        const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        char BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept override;
        char UnbindFunction(sysbit_t id) noexcept override;

        const SysFunctionHandler& operator[](sysbit_t id) const;

        const char* const operator()(sysbit_t id, const char* const params) const noexcept
        { return (*this)[id](params); }

        dlID_t LoadDl(std::string_view dlPath);
        sysfnh_t MakeFunctionHandler(dlID_t dl, std::string_view functionName) const;

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};
