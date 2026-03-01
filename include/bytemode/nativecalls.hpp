#pragma once

#include <string>
#include <vector>

#include "ffi.h"

#include "CSRConfig.hpp"
#include "system.hpp"

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
    public:
        std::unique_ptr<ffi_type*[]> args;
        void (*nativeFunc)();
        ffi_cif cif;
        FFIType returnType;
        std::unique_ptr<FFIType[]> argTypes;

        SysFunctionHandle() = delete;

        VM_INLINE SysFunctionHandle(
            ffi_cif&& cif,
            void (*func)(),
            std::unique_ptr<FFIType[]>&& argTypes,
            std::unique_ptr<ffi_type*[]>&& nativeArgs,
            FFIType returnType
        ) :
            returnType(returnType),
            nativeFunc(func),
            cif(std::move(cif)),
            argTypes(std::move(argTypes)),
            args(std::move(nativeArgs))
        { }

        VM_INLINE SysFunctionHandle(SysFunctionHandle&& other) :
            cif(std::move(other.cif)),
            nativeFunc(other.nativeFunc),
            args(std::move(other.args)),
            returnType(other.returnType),
            argTypes(std::move(other.argTypes))
        {
            cif.arg_types = args.get();
        }

        VM_INLINE size_t ArgCount() const noexcept { return cif.nargs; }

};

struct FunctionSignature
{
    std::string name;
    std::string returnType;
    std::vector<std::string> arguments;
};

using FFIFunc = void(*)();
