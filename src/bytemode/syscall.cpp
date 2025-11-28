#include <ffi.h>
#include <string>
#include <string_view>

#include "bytemode/nativecalls.hpp"
#include "extensions/syntaxextensions.hpp"
#include "extensions/stringextensions.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"

// TODO: Rewrite with libffi

SysCallHandler::SysCallHandler() :
    boundFuncs()
{ }

SysCallHandler::SysCallHandler(SysFunctionMap map) :
    boundFuncs(rval(map))
{ }

SysCallHandler::~SysCallHandler()
{
    for (dlID_t id : dlList)
        DLUnload(id);
}

System::ErrorCode SysCallHandler::BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept
{
    if (boundFuncs.contains(id))
        return System::ErrorCode::DuplicateSysBind;
    boundFuncs[id] = handler;
    return System::ErrorCode::Ok;
}

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

SysFunctionHandler SysCallHandler::MakeFunctionHandler(std::string_view functionSignature) const
{
    void (*handler)();

    for (dlID_t dl : dlList)
    {
        handler = DLSym<void(*)()>(dl, functionSignature);
        if (handler)
            break;
    }

    if (handler)
        return MakeCifFromSignature(
            ParseFunctionSignature(functionSignature),
            handler
        );

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
//      - (u)int32
//      - bool
//      - (u)int8
//      - float
//      - pointers to any of these
//      - or native pointers
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

SysFunctionHandler MakeCifFromSignature(const FunctionHandlerSignature& signature, void (*fn)())
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
