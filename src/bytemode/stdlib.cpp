#include <cstring>

#include "CSRConfig.hpp"
#include "system.hpp"
#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "extensions/stringextensions.hpp"

extern "C"
{
    void CSR_Println(const char* strToPrint)
    {
        sysbit_t size { IntegerFromBytes<sysbit_t>(strToPrint) };
        std::cout.write(strToPrint+4, size);
        std::cout.put('\n');
    }
}

Error InitStandardLibrary(SysCallHandler& handler)
{
    using Extensions::String::Hash;

    handler.BindFunction(
        Hash("void CSR_Println char*"),
        MakeCifFromSignature(ParseFunctionSignature("void CSR_Println char*"), (void(*)())CSR_Println)
    );

    return System::ErrorCode::Ok;
}
