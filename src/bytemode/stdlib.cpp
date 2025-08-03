#include <system.hpp>

#include "CSRConfig.hpp"
#include "bytemode/syscall.hpp"
#include "extensions/converters.hpp"
#include "nativecalls.hpp"

char Print(char* params) noexcept
{
    std::cout.write(params+4, IntegerFromBytes<sysbit_t>(params));
    std::cout << '\n';
    params[0] = 0;
    return static_cast<char>(System::ErrorCode::Ok);
}

Error InitStandardLibrary(SysCallHandler& handler)
{
    handler.BindFunction(0, Print);
    return System::ErrorCode::Ok;
}
