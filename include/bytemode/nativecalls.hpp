#pragma once

#include <string>
#include <vector>

#include "ffi.h"

#include "CSRConfig.hpp"

enum class FFIType
{
    VMPointer,
    NativePointer,
    Int,
    UInt,
    Byte,
    UByte,
    Bool,
    Void,
    Float
};

class SysFunctionHandle
{
    private:
        ffi_cif cif;
        void (*nativeFunc)();
        std::vector<ffi_type*> args;

    public:
        FFIType returnType;
        std::vector<FFIType> argTypes;

        SysFunctionHandle() = delete;

        VM_INLINE SysFunctionHandle(
            ffi_cif&& cif,
            void (*func)(),
            std::vector<FFIType>&& argTypes,
            std::vector<ffi_type*>&& nativeArgs,
            FFIType returnType
        ) :
            returnType(returnType),
            nativeFunc(func),
            cif(std::move(cif)),
            argTypes(std::move(argTypes)),
            args(std::move(nativeArgs))
        { }

        VM_INLINE size_t ArgCount() const noexcept { return cif.nargs; }

        VM_INLINE void operator()(void** params, void* returns)
        {
            ffi_call(&cif, nativeFunc, returns, params);
        }
};

struct FunctionSignature
{
    std::string name;
    std::string returnType;
    std::vector<std::string> arguments;
};
