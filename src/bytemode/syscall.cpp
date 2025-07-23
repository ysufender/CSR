#include <string>
#include <string_view>

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
        return (char)Error::DuplicateSysBind;

    boundFuncs[id] = rval(handler);
    return (char)Error::Ok;
}

char SysCallHandler::UnbindFunction(sysbit_t id) noexcept
{
    if (!boundFuncs.contains(id))
        return (char)Error::InvalidKey;
    boundFuncs.erase(id);
    return (char)Error::Ok;
}

const SysFunctionHandler& SysCallHandler::operator[](sysbit_t id) const
{
    if (!boundFuncs.contains(id))
        CRASH(
            Error::InvalidKey,
            "Error while syscall, no handler with key ", std::to_string(id), "."
        ); 
    return boundFuncs.at(id);
}

dlID_t SysCallHandler::LoadDl(std::string_view dllPath) 
{
    dlID_t dll { DLLoad(dllPath) };

    if (!dll)
    {
#if defined(_WIN32) || defined(__CYGWIN__)
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
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
        std::string errMsg { dlerror() };
#endif

        CRASH(
            Error::DLLoadError,
            "Couldn't load DL ", dllPath,
            "\n\tInfo: ", errMsg
        );
    }

#if defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
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

#if defined(_WIN32) || defined(__CYGWIN__)
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
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
    std::string  errMsg { dlerror() };
#endif

    CRASH(
        Error::DLSymbolError,
        "Couldn't get symbol ", functionName,
        "\n\tInfo: ", errMsg
    );
    return nullptr;
}

char SysCallBinder(void *scallH, sysbit_t id, SysFunctionHandler handler) noexcept
{
    return reinterpret_cast<ISysCallHandler*>(scallH)->BindFunction(id, handler);
}

char SysCallUnbinder(void *scallH, sysbit_t id) noexcept
{
    return reinterpret_cast<ISysCallHandler*>(scallH)->UnbindFunction(id);
}
