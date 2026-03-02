#include <cstdint>
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

    void CSR_Print(const char* strToPrint)
    {
        sysbit_t size { IntegerFromBytes<sysbit_t>(strToPrint) };
        std::cout.write(strToPrint+4, size);
    }

    float CSR_U32ToFloat(const sysbit_t num) { return static_cast<float>(num); }
    float CSR_I32ToFloat(const int32_t num) { return static_cast<float>(num); }
    uint32_t CSR_FloatToU32(const float num) { return static_cast<uint32_t>(num); }
    int32_t CSR_FloatToI32(const float num) { return static_cast<int32_t>(num); }

    void CSR_FreePointer(void* ptr) { std::free(ptr); }

    dlID_t CSR_LoadDL(const char* path)
    {
        sysbit_t size { IntegerFromBytes<sysbit_t>(path) };
        std::string realPath { path+4, size };

        return SysCallHandler::CurrentHandler().LoadDl(realPath);
    }
    bool CSR_CheckLoadStatus(dlID_t dl) { return dl != dlFalse; }
}

const struct { const char* sign; FFIFunc fn; } test[] {
    { "void CSR_Println byte*", reinterpret_cast<FFIFunc>(CSR_Println) },
    { "void CSR_Print byte*", reinterpret_cast<FFIFunc>(CSR_Print) },
    { "float CSR_U32ToFloat uint", reinterpret_cast<FFIFunc>(CSR_U32ToFloat) },
    { "float CSR_I32ToFloat int", reinterpret_cast<FFIFunc>(CSR_I32ToFloat) },
    { "uint CSR_FloatToU32 float", reinterpret_cast<FFIFunc>(CSR_FloatToU32) },
    { "int CSR_FloatToI32 float", reinterpret_cast<FFIFunc>(CSR_FloatToI32) },
    { "void CSR_FreePointer ptr", reinterpret_cast<FFIFunc>(CSR_FreePointer) },
    { "ptr CSR_LoadDL byte*", reinterpret_cast<FFIFunc>(CSR_LoadDL) },
    { "bool CSR_CheckLoadStatus ptr", reinterpret_cast<FFIFunc>(CSR_CheckLoadStatus) },
};

Error InitStandardLibrary(SysCallHandler& handler)
{
    using Extensions::String::Hash;
    
    for (const auto& pair : test)
        handler.BindFunction(Hash(pair.sign), MakeCifFromSignature(pair.sign, pair.fn));

    return System::ErrorCode::Ok;
}
