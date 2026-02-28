#pragma once

#include <cstddef>
#include <functional>
#include <chrono>
#include <istream>

#include "bytemode/assemblyinfo.hpp"
#include "bytemode/flat/flatrom.hpp"
#include "bytemode/flat/flatram.hpp"
#include "bytemode/flat/flatcpu.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/syscall.hpp"
#include "bytemode/jit.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

class FlatVM
{
    public:
        struct VMSettings
        {
#ifndef NDEBUG
            bool step;
#endif
            std::filesystem::path path;
#ifdef ENABLE_JIT
    #warning "Even though JIT builds are allowed and has performance benefits, real JIT compilation is not complete yet. So your program will NOT run if you set the VMSettings::jit to true."
            bool jit;
#endif
        };

        // to create from filepath
        // fiel must be in proper bytecode format, including assemblyinfo at the end.
        FlatVM(VMSettings settings);

#ifdef TOOLCHAIN_MODE 
        // to create from an already read buffer.
        // bytecode must not contain assemblyinfo.
        FlatVM(VMSettings settings, AssemblyInfo info, char* const buf, const sysbit_t bufsize);

        // to create from given stream
        FlatVM(VMSettings settings, AssemblyInfo info, std::istream& source);

        VM_INLINE Slice DumpROM() const { return rom.Data(); }
        VM_INLINE Slice DumpRAM() const { return ram.Dump(); }
        VM_INLINE FlatCPU::State DumpCPU() const { return cpu.DumpState(); }
        VM_INLINE SysCallHandler& GetSysCallHandler() { return handler; }
#endif

        FlatVM(FlatVM const&) = delete;
        FlatVM(FlatVM const&&) = delete;
        FlatVM& operator=(FlatVM const&) = delete;
        FlatVM& operator=(FlatVM const&&) = delete;


        VM_INLINE const VMSettings& GetSettings() const noexcept
        { return this->settings; }

        const System::ErrorCode Run() noexcept;

#ifdef TOOLCHAIN_MODE
    public:
#else
    private:
#endif
        const System::ErrorCode Cycle() noexcept;

    private:
        AssemblyInfo assembly;
        FlatROM rom;
        FlatRAM ram;
        FlatCPU cpu;
        SysCallHandler handler;
        VMSettings settings;
        std::chrono::time_point<std::chrono::steady_clock> startT;
#ifdef ENABLE_JIT
        BlockCounterCollection blocks;
        JITContext jitContext;

        VM_INLINE static bool IsCompiled(void* vm, const uint32_t pos)
        {
            if (!reinterpret_cast<FlatVM*>(vm)->blocks.Contains(pos))
                return false;
            return reinterpret_cast<FlatVM*>(vm)->blocks[pos].IsCompiled();
        }
        VM_INLINE static JITEntry GetEntry(void* vm, const uint32_t pos) { return reinterpret_cast<FlatVM*>(vm)->blocks[pos].GetEntry(); }
#endif

        void SetUpCommon();
        std::pair<std::unique_ptr<const char[]>, std::streamoff> ReadBytecode(std::istream& bytecode);

        const System::ErrorCode BitLogic(std::array<OpCodes, 3> op, std::function<sysbit_t(sysbit_t, sysbit_t)> bitwise) noexcept;

        void* GetRealAddress(const uint32_t addr) { return (&this->ram)+addr; }

        uint32_t GetVMAddress(void* ptr) { return static_cast<uint32_t>((char*)ptr - &this->ram); }
};
