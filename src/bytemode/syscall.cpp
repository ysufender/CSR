#include <string>
#include <string_view>

#include "ffi.h"

#include "bytemode/nativecalls.hpp"
#include "extensions/stringextensions.hpp"
#include "bytemode/syscall.hpp"
#include "platform.hpp"
#include "system.hpp"

SysCallHandler* SysCallHandler::currentHandler { nullptr };

dlID_t SysCallHandler::LoadDl(std::string_view dllPath) 
{
    dlID_t dll { DLLoad(dllPath) };

    if (dll == dlFalse)
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

        LOGE(
            System::LogLevel::Medium,
            "Couldn't load DL ", dllPath,
            "\n\tInfo: ", errMsg
        );

        return dll;
    }

#if defined(CSR_UNIX) || defined(CSR_APPLE)
    dlerror();
#endif

    dlList.emplace(dll);
    return dll;
}

SysFunctionHandle& SysCallHandler::MakeFunctionHandle(std::string_view functionSignature)
{
    const size_t hash { Extensions::String::Hash(functionSignature) };

    if (this->boundFuncs.contains(hash))
        return this->boundFuncs.at(hash);

    FFIFunc nativeFunc;

    FunctionSignature signature { ParseFunctionSignature(functionSignature) };

    for (dlID_t dl : this->dlList)
    {
        nativeFunc = DLSym<FFIFunc>(dl, signature.name);
        if (nativeFunc)
            break;
    }

    if (nativeFunc)
    {
        this->BindFunction(hash, MakeCifFromSignature(signature, nativeFunc));
        return this->boundFuncs.at(hash);
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

    __builtin_unreachable();
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
FunctionSignature ParseFunctionSignature(std::string_view signature)
{
    std::vector<std::string> lexed { Extensions::String::Split(signature, ' ') };
    return {
        lexed.at(1),
        lexed.at(0),
        std::vector<std::string> { lexed.begin()+2, lexed.end() }
    };
}

std::pair<FFIType, ffi_type*> DetectType(std::string_view type, bool isReturn)
{
    if (type.ends_with('*'))
        return { FFIType::VMPointer, &ffi_type_pointer };
    if (type == "ptr")
        return { FFIType::NativePointer, &ffi_type_pointer };
    else if (type == "int")
        return { FFIType::Int, &ffi_type_sint32 };
    else if (type == "uint")
        return { FFIType::UInt, &ffi_type_uint32 };
    else if (type == "float")
        return { FFIType::Float, &ffi_type_float };
    else if (type == "bool")
        return { FFIType::Bool, &ffi_type_uint8 };
    else if (type == "byte")
        return { FFIType::Byte, &ffi_type_sint8 };
    else if (type == "ubyte")
        return { FFIType::UByte, &ffi_type_uint8 };
    else if (type == "void" && isReturn)
        return { FFIType::Void, &ffi_type_void };
    else
    {
        CRASH(
            System::ErrorCode::NativeCallError,
            "Given type ", type, " is not supported by CSR in native function calls."
        );
        __builtin_unreachable();
    }
}

SysFunctionHandle MakeCifFromSignature(FunctionSignature signature, FFIFunc fn)
{
    ffi_cif cif;
    std::unique_ptr<FFIType[]> argsUnderlying { std::make_unique_for_overwrite<FFIType[]>(signature.arguments.size()) };
    std::unique_ptr<ffi_type*[]> argsNative { std::make_unique_for_overwrite<ffi_type*[]>(signature.arguments.size()) };
    std::pair<FFIType, ffi_type*> returnType { DetectType(signature.returnType, true) };

    for (int i = 0; i < signature.arguments.size(); i++)
    {
        std::pair<FFIType, ffi_type*> pair { DetectType(signature.arguments.at(i), false) };
        argsUnderlying[i] = pair.first;
        argsNative[i] = pair.second;
    }

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, signature.arguments.size(), returnType.second, argsNative.get()) != FFI_OK)
        CRASH(
            System::ErrorCode::NativeCallError,
            "Couldn't create a handler for native function '", signature.name, "'."
        );

    return {
        std::move(cif),
        fn,
        std::move(argsUnderlying),
        std::move(argsNative),
        returnType.first
    };
}

SysFunctionHandle MakeCifFromSignature(std::string_view signatureStr, FFIFunc fn)
{
    return MakeCifFromSignature(ParseFunctionSignature(signatureStr), fn); 
}

int GetTypeSize(FFIType type)
{
    switch (type)
    {
        case FFIType::Int:
        case FFIType::UInt:
        case FFIType::VMPointer:
        case FFIType::NativePointer:
        case FFIType::Float:
            return 4;

        case FFIType::Byte:
        case FFIType::UByte:
        case FFIType::Bool:
            return 1;

        case FFIType::Void:
            return 0;
    }

    __builtin_unreachable();
}
