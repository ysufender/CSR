#include <chrono>
#include <system.hpp>
#include <thread>

#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "fastcout.hpp"

namespace
{
    char Print(char* params) noexcept
    {
        std::cout.write(params+4, IntegerFromBytes<sysbit_t>(params));
        params[0] = 0;
        return static_cast<char>(System::ErrorCode::Ok);
    }

    char PrintLine(char* params) noexcept
    {
        std::cout.write(params+4, IntegerFromBytes<sysbit_t>(params)) << '\n';
        params[0] = 0;
        return static_cast<char>(Error::Ok);
    }

    char Sleep(char* params) noexcept
    {
        FastCout::Get().Flush();
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        params[0] = 0;
        return static_cast<char>(Error::Ok);
    }

    char SleepSilent(char* params) noexcept
    {
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        params[0] = 0;
        return static_cast<char>(Error::Ok);
    }
}

Error InitStandardLibrary(SysCallHandler& handler)
{
    handler.BindFunction(0, ::PrintLine);
    handler.BindFunction(1, ::Print);
    handler.BindFunction(2, ::Sleep);
    handler.BindFunction(3, ::SleepSilent);
    return System::ErrorCode::Ok;
}
