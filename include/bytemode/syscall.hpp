#pragma once

// TODO: Convert to libffi

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

        VM_INLINE const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        char BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept;
        char UnbindFunction(sysbit_t id) noexcept;

        VM_INLINE const SysFunctionHandler& operator[](sysbit_t id) const
        {
            if (!boundFuncs.contains(id)) [[unlikely]]
                CRASH(
                    System::ErrorCode::InvalidKey,
                    "Error while syscall, no handler with key ", std::to_string(id), "."
                ); 
            return boundFuncs.at(id);
        }

        VM_INLINE const System::ErrorCode operator()(sysbit_t id, VMContext* context, char* params) const noexcept
        { return static_cast<const System::ErrorCode>((*this)[id](context, params)); }

        dlID_t LoadDl(std::string_view dlPath);
        sysfnh_t MakeFunctionHandler(dlID_t dl, std::string_view functionName) const;

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

System::ErrorCode InitExtender(VMContext& context, SysCallHandler& handler, const std::filesystem::path& path) noexcept;
