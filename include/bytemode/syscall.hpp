#pragma once

// TODO: Convert to libffi

#include <unordered_set>
#include <unordered_map>

#include "ffi.h"

#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"
#include "nativecalls.hpp"

using SysFunctionMap = std::unordered_map<size_t, SysFunctionHandle>;
using DLList = std::unordered_set<dlID_t>;

class SysCallHandler
{
    public:
        VM_INLINE SysCallHandler() :
            boundFuncs()
        { }

        VM_INLINE SysCallHandler(SysFunctionMap map) :
            boundFuncs(std::move(map))
        { }

        VM_INLINE ~SysCallHandler()
        {
            for (dlID_t id : dlList)
                DLUnload(id);
        }

        VM_INLINE const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        VM_INLINE System::ErrorCode BindFunction(size_t id, SysFunctionHandle handler) noexcept
        {
            if (boundFuncs.contains(id)) [[unlikely]]
                return System::ErrorCode::DuplicateSysBind;

            [[likely]]
            boundFuncs[id] = handler;
            return System::ErrorCode::Ok;
        }

        VM_INLINE void operator()(const SysFunctionHandle& handle, void** params, void* returns)
        {
            ffi_call(const_cast<ffi_cif*>(&handle.cif), handle.nativeFunc, returns, params);
        }

        static SysCallHandler* currentHandler;

        dlID_t LoadDl(std::string_view dlPath);
        SysFunctionHandle MakeFunctionHandler(std::string_view functionSignature);

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

FunctionHandlerSignature ParseFunctionSignature(std::string_view signature);
SysFunctionHandle MakeCifFromSignature(const FunctionHandlerSignature& signature, void (*fn)());
ffi_type* DetectType(const std::string_view type, bool isReturn);
int GetTypeSize(const ffi_type* type);
