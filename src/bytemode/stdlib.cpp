#include <chrono>
#include <cstdint>
#include <system.hpp>
#include <thread>

#include "bytemode/nativecalls.hpp"
#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "fastcout.hpp"

namespace
{
    __attribute__((used)) static const char* const newline = "\n";

    char Print( VMContext* context, char* params) noexcept
    {
        char* data { (char*)context->GetRealAddress(context, IntegerFromBytes<uint32_t>(params)) };
        std::cout.write(data+4, IntegerFromBytes<uint32_t>(data));
        return_with(0)
    }

    char PrintLine( VMContext* context, char* params) noexcept
    {
        char* data { (char*)context->GetRealAddress(context, IntegerFromBytes<uint32_t>(params)) };
        std::cout.write(data+4, IntegerFromBytes<uint32_t>(data));
        std::cout.write(newline, 1);
        return_with(0)
    }

    char Sleep( VMContext* context, char* params) noexcept
    {
        FastCout::Get().Flush();
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        params[0] = 0;
        return ReturnCode::Ok;
    }

    char SleepSilent( VMContext* context, char* params) noexcept
    {
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        params[0] = 0;
        return ReturnCode::Ok;
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
