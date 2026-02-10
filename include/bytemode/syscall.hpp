#pragma once

// TODO: Convert to libffi

#include <cstddef>
#include <iterator>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <variant>

#include "ffi.h"

#include "CSRConfig.hpp"
#include "platform.hpp"
#include "system.hpp"
#include "nativecalls.hpp"

using SysFunctionMap = std::unordered_map<sysbit_t, SysFunctionHandler>;
using DLList = std::unordered_set<dlID_t>;
using handle_t = void(*)();

class SysCallHandler
{
    public:
        SysCallHandler();
        SysCallHandler(SysFunctionMap map);

        ~SysCallHandler();

        VM_INLINE const SysFunctionMap& BoundFunctions() const noexcept
        { return this->boundFuncs; }

        System::ErrorCode BindFunction(sysbit_t id, SysFunctionHandler handler) noexcept;

        VM_INLINE System::Result<const SysFunctionHandler*> operator[](sysbit_t id) const
        {
            if (!boundFuncs.contains(id)) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Error while syscall, no handler with key ", std::to_string(id), "."); 
                return System::ErrorCode::InvalidKey;
            }

            [[likely]]
            return &boundFuncs.at(id);
        }

        /*
        VM_INLINE const System::ErrorCode operator()(sysbit_t id, VMContext* context, char* params) const noexcept
        { return static_cast<const System::ErrorCode>((*this)[id](context, params)); }
        */
        VM_INLINE System::ErrorCode operator()(sysbit_t id, void** params, void* returns)
        {
            System::Result<const SysFunctionHandler*> result { this->operator[](id) };

            if (std::holds_alternative<const SysFunctionHandler*>(result))
            {
                this->operator()(*std::get<const SysFunctionHandler*>(result), params, returns);
                return System::ErrorCode::Ok;
            }

            return std::get<System::ErrorCode>(result);
        }

        VM_INLINE void operator()(const SysFunctionHandler& handler, void** params, void* returns)
        {
            ffi_call(const_cast<ffi_cif*>(&handler.cif), handler.nativeFunc, returns, params);
        }

        static SysCallHandler* currentHandler;

        System::Result<dlID_t> LoadDl(std::string_view dlPath);
        System::Result<std::pair<FunctionHandlerSignature, SysFunctionHandler>> MakeFunctionHandler(std::string_view functionSignature);
        static System::Result<const SysFunctionHandler> MakeCifFromSignature(const FunctionHandlerSignature& signature, handle_t fn);
        static FunctionHandlerSignature ParseFunctionSignature(std::string_view signature);
        static System::Result<ffi_type*> DetectType(const std::string_view type, bool isReturn);
        static System::Result<int> GetTypeSize(const ffi_type* type, bool isReturn);

    private:
        SysFunctionMap boundFuncs; 
        DLList dlList;
};

// System::ErrorCode InitExtender(VMContext& context, SysCallHandler& handler, const std::filesystem::path& path) noexcept;
