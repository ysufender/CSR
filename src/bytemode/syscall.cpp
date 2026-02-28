#include <string>
#include <string_view>

#include "ffi.h"

#include "bytemode/nativecalls.hpp"
#include "extensions/stringextensions.hpp"
#include "bytemode/syscall.hpp"
#include "platform.hpp"
#include "system.hpp"

dlID_t SysCallHandler::LoadDl(std::string_view dllPath) 
{
    dlID_t dll { DLLoad(dllPath) };

    if (!dll)
    {
#ifdef CSR_WIN
        DWORD errID { GetLastError() };
        LPSTR messageBuffer { nullptr };
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errorMessageID,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer,
            0,
            NULL
        );
        std::string errMsg { messageBuffer, size };
        LocalFree(messageBuffer);
#elif defined(CSR_UNIX) || defined(CSR_APPLE)
        std::string errMsg { dlerror() };
#endif

        CRASH(
            System::ErrorCode::DLLoadError,
            "Couldn't load DL ", dllPath,
            "\n\tInfo: ", errMsg
        );
    }

#if defined(CSR_UNIX) || defined(CSR_APPLE)
    dlerror();
#endif

    dlList.emplace(dll);
    return dll;
}

SysFunctionHandle SysCallHandler::MakeFunctionHandler(std::string_view functionSignature)
{
    const size_t hash { Extensions::String::Hash(functionSignature) };

    if (this->boundFuncs.contains(hash))
        return this->boundFuncs.at(hash);

    void (*nativeFunc)();

    for (dlID_t dl : dlList)
    {
        nativeFunc = DLSym<void(*)()>(dl, functionSignature);
        if (nativeFunc)
            break;
    }

    if (nativeFunc)
    {
        SysFunctionHandle handle { MakeCifFromSignature(
            ParseFunctionSignature(functionSignature),
            nativeFunc
        )};

        this->BindFunction(hash, handle);
        return handle;
    }


#ifdef CSR_WIN
    DWORD errID { GetLastError() };
    LPSTR messageBuffer { nullptr };
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorMessageID,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );
    std::string errMsg { messageBuffer, size };
    LocalFree(messageBuffer);
#elif defined(CSR_UNIX) || defined(CSR_APPLE)
    std::string  errMsg { dlerror() };
#endif

    CRASH(
        System::ErrorCode::DLSymbolError,
        "Couldn't get symbol ", functionSignature,
        "\n\tInfo: ", errMsg
    );
    return {};
}

// Format must be "returnType fnName ..spaceSeperatedParamTypes..."
// param types must be one of:
//      - (u)int
//      - bool
//      - (u)byte
//      - float
//      - type* (VM ptr)
//      - ptr (native ptr)
// return types are the same but extra 'void'
// 'const' keyword is not allowed.
FunctionHandlerSignature ParseFunctionSignature(std::string_view signature)
{
    std::vector<std::string> lexed { Extensions::String::Split(signature, ' ') };
    return {
        lexed.at(0),
        lexed.at(1),
        std::vector<std::string> { lexed.begin()+2, lexed.end() }
    };
}

ffi_type* DetectType(const std::string_view type, bool isReturn)
{
    __builtin_unreachable();
}

SysFunctionHandle MakeCifFromSignature(const FunctionHandlerSignature& signature, void (*fn)())
{
    ffi_cif cif;
    ffi_type* args[signature.arguments.size()];
    ffi_type* returnType { DetectType(signature.returnType, true) };

    for (int i = 0; i < signature.arguments.size(); i++)
        args[i] = DetectType(signature.arguments.at(i), false);

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, signature.arguments.size(), returnType, args) != FFI_OK)
        CRASH(
            System::ErrorCode::NativeCallError,
            "Couldn't create a handler for native function '", signature.name, "'."
        );
    return { fn, cif };
}

int GetTypeSize(const ffi_type* type)
{
    __builtin_unreachable();
}
