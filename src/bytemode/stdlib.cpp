#include <cstdint>
#include <cstring>
#include <fstream>

#include "system.hpp"
#include "extensions/converters.hpp"
#include "bytemode/syscall.hpp"
#include "extensions/stringextensions.hpp"

extern "C"
{
    //
    // IO
    //
    void CSR_Println(const char* strToPrint)
    {
        uint32_t size { IntegerFromBytes<uint32_t>(strToPrint) };
        std::cout.write(strToPrint+4, size);
        std::cout.put('\n');
    }

    void CSR_Print(const char* strToPrint)
    {
        uint32_t size { IntegerFromBytes<uint32_t>(strToPrint) };
        std::cout.write(strToPrint+4, size);
    }

    void CSR_Flush() { std::cout.flush(); }

    //
    // Conversions
    //
    float CSR_U32ToFloat(const uint32_t num) { return static_cast<float>(num); }
    float CSR_I32ToFloat(const int32_t num) { return static_cast<float>(num); }
    uint32_t CSR_FloatToU32(const float num) { return static_cast<uint32_t>(num); }
    int32_t CSR_FloatToI32(const float num) { return static_cast<int32_t>(num); }

    //
    // Memory
    //
    void CSR_Free(void* ptr) { std::free(ptr); }
    void* CSR_Malloc(uint32_t size) { return std::malloc(size); }

    //
    // Dynamic Loading Functions
    //
    dlID_t CSR_LoadDL(const char* path)
    {
        uint32_t size { IntegerFromBytes<uint32_t>(path) };
        std::string realPath { path+4, size };

        return SysCallHandler::CurrentHandler().LoadDl(realPath);
    }
    uint8_t CSR_CheckLoadStatus(dlID_t dl) { return dl != dlFalse; }

    //
    // File
    //
    static std::unordered_map<uint32_t, std::fstream> ioMap(32);

    enum OpenMode : uint32_t 
    {
        Overwrite = 1,
        Append = 2,
    };

    constexpr std::ios::openmode GetOpenMode(const OpenMode mode)
    {
        std::ios::openmode ret { };

        if (mode & Overwrite)
            ret |= std::ios::trunc;
        if (mode & Append)
            ret |= std::ios::app;

        return ret;
    }

    uint8_t CSR_Exists(const char* path)
    {
        return std::filesystem::exists(std::string_view{
            path+4,
            IntegerFromBytes<uint32_t>(path)
        });
    }

    uint32_t CSR_OpenFile(const char* fileName, OpenMode mode)
    {
        static uint32_t fileID { 1 };

        if (fileID == 0) [[unlikely]]
            fileID++;

        uint32_t size { IntegerFromBytes<uint32_t>(fileName) };
        std::string realName { fileName+4, size };

        const auto [_, status] { ioMap.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(fileID),
            std::forward_as_tuple(realName, std::ios::in | std::ios::out | GetOpenMode(mode)))
        };

        if (status) [[likely]]
            return fileID++;

        return 0;
    }

    uint8_t CSR_CloseFile(uint32_t handle)
    {
        decltype(ioMap)::node_type node { ioMap.extract(handle) };
        if (node.empty()) [[unlikely]]
            return false;

        std::fstream& file { node.mapped() };
        file.close();

        if (!file.fail()) [[likely]]
            return true;

        ioMap.insert(std::move(node));
        return false;
    }

    void CSR_Write(const char* string, uint32_t handle)
    {
        if (!ioMap.contains(handle))
            return;

        ioMap.at(handle).write(string+4, IntegerFromBytes<uint32_t>(string));
    }
}

const struct { const char* sign; FFIFunc fn; } test[] {
    { "void CSR_Println byte*", reinterpret_cast<FFIFunc>(CSR_Println) },
    { "void CSR_Print byte*", reinterpret_cast<FFIFunc>(CSR_Print) },
    { "void CSR_Flush", reinterpret_cast<FFIFunc>(CSR_Flush) },

    { "float CSR_U32ToFloat uint", reinterpret_cast<FFIFunc>(CSR_U32ToFloat) },
    { "float CSR_I32ToFloat int", reinterpret_cast<FFIFunc>(CSR_I32ToFloat) },
    { "uint CSR_FloatToU32 float", reinterpret_cast<FFIFunc>(CSR_FloatToU32) },
    { "int CSR_FloatToI32 float", reinterpret_cast<FFIFunc>(CSR_FloatToI32) },

    { "void CSR_Free ptr", reinterpret_cast<FFIFunc>(CSR_Free) },
    { "ptr CSR_Malloc uint", reinterpret_cast<FFIFunc>(CSR_Malloc) },

    { "ptr CSR_LoadDL byte*", reinterpret_cast<FFIFunc>(CSR_LoadDL) },
    { "bool CSR_CheckLoadStatus ptr", reinterpret_cast<FFIFunc>(CSR_CheckLoadStatus) },

    { "bool CSR_Exists byte*", reinterpret_cast<FFIFunc>(CSR_Exists) },
    { "uint CSR_OpenFile byte* uint", reinterpret_cast<FFIFunc>(CSR_OpenFile) },
    { "bool CSR_CloseFile uint", reinterpret_cast<FFIFunc>(CSR_CloseFile) },
    { "void CSR_Write byte* uint", reinterpret_cast<FFIFunc>(CSR_Write) },
};

Error InitStandardLibrary(SysCallHandler& handler)
{
    using Extensions::String::Hash;
    
    for (const auto& pair : test)
        handler.BindFunction(Hash(pair.sign), MakeCifFromSignature(pair.sign, pair.fn));

    return System::ErrorCode::Ok;
}
