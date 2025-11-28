#pragma once

#include <string>
#include <vector>

#include "ffi.h"

struct SysFunctionHandler
{
    void (*nativeFunc)();
    ffi_cif cif;
};

struct FunctionHandlerSignature
{
    std::string name;
    std::string returnType;
    std::vector<std::string> arguments;
};
