#include <cstddef>
#include <dlfcn.h>
#include <ffi.h>
#include <string>
#include <string_view>
#include <variant>

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

System::Result<dlID_t> SysCallHandler::LoadDl(std::string_view dllPath) 
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

        LOGE(System::LogLevel::High, "Couldn't load DL ", dllPath, "\n\tInfo: ", errMsg);
        return System::ErrorCode::DLLoadError;
    }

#if defined(CSR_UNIX) || defined(CSR_APPLE)
    dlerror();
#endif

    dlList.emplace(dll);
    return dll;
}

using HandlerPair = std::pair<FunctionHandlerSignature, SysFunctionHandler>;

System::Result<HandlerPair> SysCallHandler::MakeFunctionHandler(std::string_view functionSignature)
{
    const FunctionHandlerSignature sign { ParseFunctionSignature(functionSignature) };
    const size_t hash { Extensions::String::Hash(sign.name) };
    if (this->boundFuncs.contains(hash))
        return System::Result<HandlerPair>{
            std::in_place_type<HandlerPair>,
                sign,
                this->boundFuncs.at(hash)
        };

    handle_t handler { nullptr };

    for (dlID_t dl : dlList)
    {
        handler = DLSym<handle_t>(dl, sign.name);
        if (handler) break;
    }

    if (handler)
    {
        System::Result<const SysFunctionHandler> fnHandler { MakeCifFromSignature(sign, handler) };

        if (std::holds_alternative<System::ErrorCode>(fnHandler))
            return std::get<System::ErrorCode>(fnHandler);

        const SysFunctionHandler handler { std::get<const SysFunctionHandler>(fnHandler) };

        this->BindFunction(hash, handler);
        return System::Result<HandlerPair>{
            std::in_place_type<HandlerPair>,
                sign,
                handler
        };
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

    LOGE(System::LogLevel::High, "Couldn't get symbol ", sign.name, "\n\tInfo: ", errMsg);
    return System::ErrorCode::DLSymbolError;
}

// Format must be "returnType fnName ..spaceSeperatedParamTypes..."
// param types must be one of:
//      - (u)int32
//      - (u)int8
//      - float
//      - or pointers to these, with 'ptr'
// return types are the same but extra 'void'
// 'const' keyword is not allowed.
FunctionHandlerSignature SysCallHandler::ParseFunctionSignature(std::string_view signature)
{
    // skips whitespace
    std::vector<std::string> lexed { Extensions::String::Split(signature, ' ') };
    return {
        lexed.at(1),
        lexed.at(0),
        std::vector<std::string> { lexed.begin()+2, lexed.end() }
    };
}

// if the pointer is ends with *, then the argument/return value is
// converted between VM/native pointers. If they're ptr, they're sent
// directly.
System::Result<ffi_type*> SysCallHandler::DetectType(const std::string_view type, bool isReturn)
{
    if (type == "ptr")
        return &ffi_type_pointer;
    else if (type == "int")
        return &ffi_type_sint32;
    else if (type == "uint" || type.ends_with('*'))
        return &ffi_type_uint32;
    else if (type == "float")
        return &ffi_type_float;
    else if (type == "char")
        return &ffi_type_sint8;
    else if (type == "uchar")
        return &ffi_type_uint8;
    else if (type == "void" && isReturn)
        return &ffi_type_void;
    else
    {
        LOGE(System::LogLevel::High, "Given type ", type, " is not supported by CSR in native function calls.");
        return System::ErrorCode::NativeCallError;
    }
}

System::Result<const SysFunctionHandler> SysCallHandler::MakeCifFromSignature(const FunctionHandlerSignature& signature, handle_t fn)
{
    ffi_cif cif;
    ffi_type* args[signature.arguments.size()];

    System::Result<ffi_type*> returnTypeResult { DetectType(signature.returnType, true) };

    if (std::holds_alternative<System::ErrorCode>(returnTypeResult))
        return std::get<System::ErrorCode>(returnTypeResult);

    ffi_type* returnType { std::get<ffi_type*>(returnTypeResult) };

    for (int i = 0; i < signature.arguments.size(); i++)
    {
        System::Result<ffi_type*> argType { DetectType(signature.arguments.at(i), false) };

        if (std::holds_alternative<System::ErrorCode>(argType))
            return std::get<System::ErrorCode>(argType);

        args[i] = std::get<ffi_type*>(argType);
    }

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, signature.arguments.size(), returnType, args) != FFI_OK)
    {
        LOGE(System::LogLevel::High, "Couldn't create a handler for native function '", signature.name, "'.");
        return System::ErrorCode::NativeCallError;
    }

    return System::Result<const SysFunctionHandler>{
        std::in_place_type<const SysFunctionHandler>,
            fn,
            cif
    };
}

System::Result<int> SysCallHandler::GetTypeSize(const ffi_type* type, bool isReturn)
{
    /*if (type == &ffi_type_pointer && isReturn)
        return 4;
    else */
    if (type == &ffi_type_pointer)
        return static_cast<int>(sizeof(void*));
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
    else if (type == &ffi_type_void)
        return 0;
    else
    {
        LOGE(System::LogLevel::High, "Given type is not supported by CSR in native function calls.");
        return System::ErrorCode::NativeCallError;
    }
}
