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
    // TODO: Sanity check
    if (type.ends_with('*') || type == "ptr")
        return &ffi_type_pointer;
    else if (type == "int")
        return &ffi_type_sint32;
    else if (type == "uint")
        return &ffi_type_uint32;
    else if (type == "float")
        return &ffi_type_float;
    else if (type == "bool")
        return &ffi_type_uint8;
    else if (type == "char")
        return &ffi_type_sint8;
    else if (type == "uchar")
        return &ffi_type_uint8;
    else if (type == "void" && isReturn)
        return &ffi_type_void;
    else {
        CRASH(
            System::ErrorCode::NativeCallError,
            "Given type ", type, " is not supported by CSR in native function calls."
        );
        return &ffi_type_void;
    }
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
    // TODO: Sanity check
    if (type == &ffi_type_pointer)
        return sizeof(void*);
    else if (type == &ffi_type_sint32)
        return 4;
    else if (type == &ffi_type_uint32)
        return 4;
    else if (type == &ffi_type_float)
        return 4;
    else if (type == &ffi_type_uint8)
        return 1;
    else if (type == &ffi_type_sint8)
        return 1;
    else if (type == &ffi_type_uint8)
        return 1;
    else if (type == &ffi_type_void)
        return 0;
    else {
        CRASH(
            System::ErrorCode::NativeCallError,
            "Given type is not supported by CSR in native function calls."
        );
        return 0;
    }
}
