#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
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

    char Print(VMContext* context, char* params) noexcept
    {
        char* data { (char*)context->GetRealAddress(context, IntegerFromBytes<uint32_t>(params)) };
        std::cout.write(data+4, IntegerFromBytes<uint32_t>(data));
        return_with(0)
    }

    char PrintLine(VMContext* context, char* params) noexcept
    {
        char* data { (char*)context->GetRealAddress(context, IntegerFromBytes<uint32_t>(params)) };
        std::cout.write(data+4, IntegerFromBytes<uint32_t>(data));
        std::cout.write(newline, 1);
        return_with(0)
    }

    char Sleep(VMContext* context, char* params) noexcept
    {
        FastCout::Get().Flush();
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        return_with(0)
    }

    char SleepSilent(VMContext* context, char* params) noexcept
    {
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<sysbit_t>(FloatFromBytes(params))));
        return_with(0)
    }

    char IntToString(VMContext* context,  char* params) noexcept
    {
        int32_t i { IntegerFromBytes<int32_t>(params) };
        std::string str { std::to_string(i) };
        char* target { reinterpret_cast<char*>(context->Allocate(context, str.size()+4)) };
        std::memcpy(target+4, str.data(), str.size());
        BytesFromInteger(str.size(), target);
        params[0] = 4;
        BytesFromInteger(context->GetVMAddress(context, target), params+1);
        return ReturnCode::Ok;
    }

    char UIntToString(VMContext* context, char* params) noexcept
    {
        uint32_t i { IntegerFromBytes<uint32_t>(params) };
        std::string str { std::to_string(i) };
        char* target { reinterpret_cast<char*>(context->Allocate(context, str.size()+4)) };
        std::memcpy(target+4, str.data(), str.size());
        BytesFromInteger<uint32_t>(str.size(), target);
        params[0] = 4;
        BytesFromInteger(context->GetVMAddress(context, target), params+1);
        return ReturnCode::Ok;
    }

    char Clock(VMContext* context, char* params) noexcept
    {
        uint64_t clock { context->Clock(context) };
        params[0] = sizeof(uint64_t); 
        BytesFromInteger(clock, params+1);
        return ReturnCode::Ok;
    }
}

Error InitStandardLibrary(SysCallHandler& handler)
{
    handler.BindFunction(0, ::PrintLine);
    handler.BindFunction(1, ::Print);
    handler.BindFunction(2, ::Sleep);
    handler.BindFunction(3, ::SleepSilent);
    handler.BindFunction(4, ::IntToString);
    handler.BindFunction(5, ::UIntToString);
    handler.BindFunction(6, ::Clock);
    return System::ErrorCode::Ok;
}
