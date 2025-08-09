#include <string>
#include <string_view>

#include "bytemode/nativecalls.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"

SysCallHandler::SysCallHandler() :
    boundFuncs(),
    dlList()
{ }

SysCallHandler::SysCallHandler(SysFunctionMap map) :
    boundFuncs(rval(map))
{ }

SysCallHandler::~SysCallHandler()
{
    for (dlID_t id : dlList)
        DLUnload(id);
}

char SysCallHandler::BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept
{
    if (boundFuncs.contains(id))
        return ReturnCode::DuplicateSysBind;

    boundFuncs[id] = rval(handler);
    return ReturnCode::Ok;
}

char SysCallHandler::UnbindFunction(sysbit_t id) noexcept
{
    if (!boundFuncs.contains(id))
        return ReturnCode::InvalidKey;
    boundFuncs.erase(id);
    return ReturnCode::Ok;
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

sysfnh_t SysCallHandler::MakeFunctionHandler(dlID_t dll, std::string_view functionName) const
{
    sysfnh_t handler { DLSym<sysfnh_t>(dll, functionName) };

    if (handler)
        return handler;

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
        "Couldn't get symbol ", functionName,
        "\n\tInfo: ", errMsg
    );
    return nullptr;
}
