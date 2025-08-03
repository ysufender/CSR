#include <system.hpp>

#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"

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
}

Error InitStandardLibrary(SysCallHandler& handler)
{
    handler.BindFunction(0, ::PrintLine);
    handler.BindFunction(1, ::Print);
    return System::ErrorCode::Ok;
}
