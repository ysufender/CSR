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
            boundFuncs.emplace(id, handler);
            return System::ErrorCode::Ok;
        }

        dlID_t LoadDl(std::string_view dlPath);
        SysFunctionHandle& MakeFunctionHandler(std::string_view functionSignature);

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

FunctionSignature ParseFunctionSignature(std::string_view signature);
SysFunctionHandle MakeCifFromSignature(const FunctionSignature& signature, void (*fn)());
std::pair<FFIType, ffi_type*> DetectType(std::string_view type, bool isReturn);
int GetTypeSize(ffi_type* type);
