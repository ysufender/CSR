#include <dlfcn.h>
#include <string>
#include <string_view>

#include "extensions/stringextensions.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

SysCallHandler::SysCallHandler(SysFunctionMap&& map)
{
    this->boundFuncs = map;
}

Error SysCallHandler::BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept
{
    if (boundFuncs.contains(id))
        return Error::DuplicateSysBind;

    boundFuncs[id] = rval(handler);
    return Error::Ok;
}

Error SysCallHandler::UnbindFunciton(sysbit_t id) noexcept
{
    if (!boundFuncs.contains(id))
        return Error::InvalidKey;
    boundFuncs.erase(id);
    return Error::Ok;
}

const SysFunctionHandler& SysCallHandler::operator[](sysbit_t id) const
{
    if (!boundFuncs.contains(id))
        LOGE(
            System::LogLevel::High,
            "Error while syscall, no handler with key ", std::to_string(id), "."
        ); 
    return boundFuncs.at(id);
}

Error SysCallHandler::LoadDll(std::string_view dllPath) noexcept
{
    dllID_t dll;

#if defined(_WIN32) || defined(__CYGWIN__)
    dll = LoadLibrary(dllPath.data());

    if (!hGetProcIDDLL)
    {
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

        LOGE(
            System::LogLevel::Medium,
            "Couldn't load DLL ", dllPath,
            "\n\tInfo: ", errMsg
        );
        return Error::DLLLoadError;
    }
#elif defined(unix) || defined(__unix) || defined(__unix__)
    dll = dlopen(dllPath.data(), RTLD_NOW);

    if (!dll)
    {
        LOGE(
            System::LogLevel::Medium,
            "Couldn't load DLL ", dllPath,
            ".\n\tInfo: ", dlerror()
        );
        return Error::DLLLoadError;
    }

    dlerror();
#endif

    dllList.emplace(dll);
    return Error::Ok;
}

Error SysCallHandler::MakeFunctionHandler(std::string_view functionName) const noexcept
{
    sysfnh_t handler { nullptr };

    for (dllID_t dll : dllList)
    {
        if (handler)
            break;

#if defined(_WIN32) || defined(__CYGWIN__)
        handler = reinterpret_cast<sysfnh_t>(GetProcAddress(dll, functionName.data()));
#elif defined(unix) || defined(__unix) || defined(__unix__)
        handler = reinterpret_cast<sysfnh_t>(dlsym(dll, functionName.data()));
#endif
    }

    if (handler)
    {
        boundFuncs[GenerateNewFuncionID()] = handler;
    }

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
#elif defined(unix) || defined(__unix) || defined(__unix__)
    std::string  errMsg { dlerror() };
#endif

    LOGE(
        System::LogLevel::Medium,
        "Couldn't get symbol ", functionName,
        "\n\tInfo: ", errMsg
    );
    return Error::DLLSymbolError;
}
