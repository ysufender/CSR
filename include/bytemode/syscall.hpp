#pragma once

// TODO: Convert to libffi

#include <unordered_set>
#include <unordered_map>

#include "ffi.h"

#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"
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

        System::ErrorCode BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept;

        VM_INLINE const SysFunctionHandler& operator[](sysbit_t id) const
        {
            if (!boundFuncs.contains(id)) [[unlikely]]
                CRASH(
                    System::ErrorCode::InvalidKey,
                    "Error while syscall, no handler with key ", std::to_string(id), "."
                ); 
            return boundFuncs.at(id);
        }

        /*
        VM_INLINE const System::ErrorCode operator()(sysbit_t id, VMContext* context, char* params) const noexcept
        { return static_cast<const System::ErrorCode>((*this)[id](context, params)); }
        */
        VM_INLINE void operator()(sysbit_t id, void** params, void* returns)
        {
            const SysFunctionHandler& handler { (*this)[id] };
            ffi_call(const_cast<ffi_cif*>(&handler.cif), handler.nativeFunc, returns, params);
        }

        static SysCallHandler* currentHandler;

        dlID_t LoadDl(std::string_view dlPath);
        SysFunctionHandler MakeFunctionHandler(std::string_view functionSignature) const;

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

FunctionHandlerSignature ParseFunctionSignature(std::string_view signature);
SysFunctionHandler MakeCifFromSignature(const FunctionHandlerSignature& signature, void (*fn)());
ffi_type* DetectType(const std::string_view type, bool isReturn);
int GetTypeSize(const ffi_type* type);
// System::ErrorCode InitExtender(VMContext& context, SysCallHandler& handler, const std::filesystem::path& path) noexcept;
