#include <cstdint>
#include <cstring>
#include <system.hpp>

#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "CSRConfig.hpp"
#include "extensions/stringextensions.hpp"
#include "fastcout.hpp"

extern "C"
{
    void CSR_Println(sysbit_t addr)
    {
        std::cout << "Hello " << addr << "\n";
        /*sysbit_t size { IntegerFromBytes<sysbit_t>(strToPrint) };
        std::cout.write(strToPrint+4, size);
        */
        FastCout::Flush();
    }

    uint8_t CSR_NativePtrSize() { return sizeof(void*); }

    void CSR_LoadDL(const char* dlPath)
    {
        std::string_view realPath { std::string_view(dlPath+4, IntegerFromBytes<sysbit_t>(dlPath)) };
    }
}

System::ErrorCode InitStandardLibrary(SysCallHandler& handler)
{
    using Extensions::String::Hash;

    // TODO

    return System::ErrorCode::Ok;
}
