#pragma once

#include <functional>

#include "CSRConfig.hpp"
#include "bytemode/assemblyinfo.hpp"
#include "bytemode/flat/flatrom.hpp"
#include "bytemode/flat/flatram.hpp"
#include "bytemode/flat/flatcpu.hpp"
#include "bytemode/nativecalls.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/syscall.hpp"
#include "system.hpp"
#include "bytemode/jit.hpp"

class FlatVM
{
    public:
        struct VMSettings
        {
            bool unsafe;
#ifndef NDEBUG
            bool step;
#endif
            std::filesystem::path path;
#ifdef ENABLE_JIT
            //static_assert(false, "JIT Builds are not supported yet with Flat VM.");
            bool jit;
#endif
        };

        FlatVM(VMSettings settings);

        FlatVM(FlatVM const&) = delete;
        FlatVM(FlatVM const&&) = delete;
        FlatVM& operator=(FlatVM const&) = delete;
        FlatVM& operator=(FlatVM const&&) = delete;

        VM_INLINE const VMSettings& GetSettings() const noexcept
        { return this->settings; }

        Error Run() noexcept;

        Error BitLogic(std::array<OpCodes, 3> op, std::function<sysbit_t(sysbit_t, sysbit_t)> bitwise) noexcept;

    private:
        AssemblyInfo assembly;
        VMContext context;
        FlatROM rom;
        FlatRAM ram;
        FlatCPU cpu;
        SysCallHandler handler;
        VMSettings settings;
#ifdef ENABLE_JIT
        BlockCounterCollection blocks;
        JITContext jitContext;
#endif

        VM_INLINE static int Validate(uint64_t size, uint8_t version)
        {
            if (size != sizeof(VMContext)) [[unlikely]]
                return 0;
            if (version != VM_API_VERSION) [[unlikely]]
                return 0;
            [[likely]] return 1; 
        }

        VM_INLINE static void* GetRealAddress(VM_API, const uint32_t addr)
        {
            FlatVM& vm { *reinterpret_cast<FlatVM*>(context->context) };
            return &vm.ram+addr;
        }

        VM_INLINE static void* Allocate(VM_API, const uint32_t size)
        {
            FlatVM& vm { *reinterpret_cast<FlatVM*>(context->context) };
            return &vm.ram+vm.ram.Allocate(size);
        }

        VM_INLINE static Code Deallocate(VM_API, const uint32_t addr, const uint32_t size)
        {
            FlatVM& vm { *reinterpret_cast<FlatVM*>(context->context) };
            return static_cast<char>(vm.ram.Deallocate(addr, size));
        }
        
        VM_INLINE static Code BindFunction(VM_API, const uint32_t id, SysFunctionHandler handler)
        {
            FlatVM& vm { *reinterpret_cast<FlatVM*>(context->context) };
            return vm.handler.BindFunction(id, handler);
        }

        VM_INLINE static Code UnbindFunction(VM_API, const uint32_t id)
        {
            FlatVM& vm { *reinterpret_cast<FlatVM*>(context->context) };
            return vm.handler.UnbindFunction(id);
        }
};
