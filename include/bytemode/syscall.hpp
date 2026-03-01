#pragma once

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

        VM_INLINE System::ErrorCode BindFunction(size_t id, SysFunctionHandle&& handler) noexcept
        {
            if (boundFuncs.contains(id)) [[unlikely]]
                return System::ErrorCode::DuplicateSysBind;

            [[likely]]
            boundFuncs.emplace(id, std::move(handler));
            return System::ErrorCode::Ok;
        }

        VM_INLINE void operator()(SysFunctionHandle& handle, void** params, void* returns)
        {
            ffi_call(&handle.cif, handle.nativeFunc, returns, params);
        }

        dlID_t LoadDl(std::string_view dlPath);
        SysFunctionHandle& MakeFunctionHandler(std::string_view functionSignature);

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

SysFunctionHandle MakeCifFromSignature(std::string_view signatureStr, FFIFunc fn);
std::pair<FFIType, ffi_type*> DetectType(std::string_view type, bool isReturn);
int GetTypeSize(FFIType type);
