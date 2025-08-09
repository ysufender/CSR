#pragma once

#include "CSRConfig.hpp"

#ifdef ENABLE_JIT
#include <type_traits>
#include <cstdint>
#include <string>

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include "asmjit/core/globals.h"

#ifndef ASMJIT_ARCH_X86
    #error "CSR Only supports x86 targeted JIT for now."
#endif

#include "extensions/syntaxextensions.hpp"
#include "bytemode/structured/rom.hpp"
#include "bytemode/flat/flatrom.hpp"
#include "bytemode/instructions.hpp"
#include "extensions/converters.hpp"

#include "platform.hpp"

#define ERJIT(E) \
    E(NonexistentJIT) \
    E(CompilationError) \
    E(UnsupportedInstruction) \
    E(VMLevelError)
MAKE_ENUM(JITError, Ok, 0, ERJIT, OUT_CLASS)
#undef ERJIT

static constexpr uint32_t THRESHOLD = 1000;

struct JITContext
{
    // in order: eax, ebx, ecx, edx, esi, edi, pc, sp, bp
    uint32_t* reg32[9];
    
    // in order: al, bl, cl, dl, flg
    uint8_t* reg8[5];

    // pointing at the start of the virtual RAM
    char* ram; 
};

using namespace asmjit;

#ifdef CSR_WIN
    static x86::Gp constexpr contextReg { x86::rcx };
#else
    static constexpr x86::Gp contextReg { x86::rdi };
#endif

#define EAX x86::rax
#define EBX x86::r10
#define ECX x86::rcx
#define EDX x86::r11
#define PC  x86::rdx
#define SP  x86::r8
#define BP  x86::r9
#define FLG x86::r12b
#define TEMP x86::r13

#define AL  x86::al
#define BL  x86::r10b
#define CL  x86::cl
#define DL  x86::r11b

#define RF1 x86::xmm0
#define RF2 x86::xmm1

#define OFF_REG32 offsetof(JITContext, reg32)
#define OFF_REG8  offsetof(JITContext, reg8)
#define OFF_RAM   offsetof(JITContext, ram)

#define LOAD_REG32(i, reg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG32 + (i) * sizeof(uint32_t*))); \
    asml.mov(reg, x86::dword_ptr(x86::r13));

#define LOAD_REG8(i, reg64, reg8) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + (i) * sizeof(uint8_t*))); \
    asml.movzx(reg64, x86::byte_ptr(x86::r13)); \
    asml.mov(reg8, reg64.r8());

#define STORE_REG32(i, reg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG32 + (i) * sizeof(uint32_t*))); \
    asml.mov(x86::dword_ptr(x86::r13), reg);

#define STORE_REG8(i, reg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + (i) * sizeof(uint8_t*))); \
    asml.mov(x86::byte_ptr(x86::r13), reg);

//#define machinize() \
    asml.push(x86::r13); \
    LOAD_REG32(0, EAX); \
    LOAD_REG32(1, EBX); \
    LOAD_REG32(2, ECX); \
    LOAD_REG32(3, EDX); \
    LOAD_REG32(6, PC); \
    LOAD_REG32(7, SP); \
    LOAD_REG32(8, BP); \
    LOAD_REG8(0, EAX, AL); \
    LOAD_REG8(1, EBX, BL); \
    LOAD_REG8(2, ECX, CL); \
    LOAD_REG8(3, EDX, DL); \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + 4 * sizeof(uint8_t*))); \
    asml.movzx(x86::rcx, x86::byte_ptr(x86::r13)); \
    asml.mov(FLG, x86::rcx.r8());

#define machinize() \
    asml.push(x86::r13); \
    LOAD_REG32(6, PC); \
    LOAD_REG32(7, SP); \
    LOAD_REG32(8, BP); \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + 4 * sizeof(uint8_t*))); \
    asml.movzx(x86::rcx, x86::byte_ptr(x86::r13)); \
    asml.mov(FLG, x86::rcx.r8());

//#define virtualize() \
    STORE_REG32(0, EAX); \
    STORE_REG32(1, EBX); \
    STORE_REG32(2, ECX); \
    STORE_REG32(3, EDX); \
    STORE_REG32(6, PC); \
    STORE_REG32(7, SP); \
    STORE_REG32(8, BP); \
    STORE_REG8(0, AL); \
    STORE_REG8(1, BL); \
    STORE_REG8(2, CL); \
    STORE_REG8(3, DL); \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + 4 * sizeof(uint8_t*))); \
    asml.mov(x86::byte_ptr(x86::r13), FLG); \
    asml.pop(x86::r13);

#define virtualize() \
    STORE_REG32(6, PC); \
    STORE_REG32(7, SP); \
    STORE_REG32(8, BP); \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, OFF_REG8 + 4 * sizeof(uint8_t*))); \
    asml.mov(x86::byte_ptr(x86::r13), FLG); \
    asml.pop(x86::r13);


#define COMPUTE_ADDR(baseReg, offset) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, offsetof(JITContext, ram))); \
    asml.add(x86::r13, baseReg); \
    asml.add(x86::r13, offset);

#define RAM_READ8_SP(offset, targetReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.movzx(x86::rcx, x86::byte_ptr(x86::r13)); \
    asml.mov(targetReg, x86::rcx.r8());

#define RAM_READ8_BP(offset, targetReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.movzx(x86::rcx, x86::byte_ptr(x86::r13)); \
    asml.mov(targetReg, x86::rcx.r8());

#define RAM_READ32_SP(offset, targetReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.mov(targetReg, x86::dword_ptr(x86::r13));

#define RAM_READ32F_SP(offset, targetReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.movss(targetReg, x86::dword_ptr(x86::r13));

#define RAM_READ32_BP(offset, targetReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.mov(targetReg, x86::dword_ptr(x86::r13));

#define RAM_READ32F_BP(offset, targetReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.movss(targetReg, x86::dword_ptr(x86::r13));

#define RAM_WRITE8_SP(offset, sourceReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.mov(x86::byte_ptr(x86::r13), sourceReg);

#define RAM_WRITE8_BP(offset, sourceReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.mov(x86::byte_ptr(x86::r13), sourceReg);

#define RAM_WRITE32_SP(offset, sourceReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.mov(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_WRITE32F_SP(offset, sourceReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.movss(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_WRITE32_BP(offset, sourceReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.mov(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_WRITE32F_BP(offset, sourceReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.movss(x86::dword_ptr(x86::r13), sourceReg);

VM_INLINE static bool is_branching(const uchar_t opx)
{
    OpCodes op { opx };
    return
        /*op == OpCodes::cal ||*/ op == OpCodes::calr ||
        /*op == OpCodes::jmp ||*/ op == OpCodes::jmpr ||
        /*op == OpCodes::cnd ||*/ op == OpCodes::cndr //||
        /*op == OpCodes::cnj*/;
}

using JITEntry = void (*)(JITContext*);

JITError JITInstruction(const BaseROM& rom, uint32_t& pc, x86::Assembler& asml);

class BlockCounter
{
    public:
        VM_INLINE ~BlockCounter()
        {
            if (entry != nullptr)
                runtime.release(entry); 
            runtime.reset(ResetPolicy::kHard);
        }

        VM_INLINE bool IsHot() const { return count > THRESHOLD; }
        VM_INLINE void Increment() { count++; }
        VM_INLINE bool IsCompiled() const { return entry != nullptr; }

        VM_INLINE JITError JIT(uint32_t target, const BaseROM& rom, JITContext* context)
        {
            // target is the target (which is the start of the current block)
            // context->reg32[6] is where the current call is made. (actually a byte after that)
            code.init(runtime.environment());
            x86::Assembler asml { &code };
            JITError error { JITError::Ok };
            machinize()
                while (target < rom.Size() && !is_branching(rom[target]))
                {
                    error = JITInstruction(rom, target, asml);
                    if (error == JITError::Ok) [[likely]]
                        continue;
                    code.reset(ResetPolicy::kHard); 
                    LOGW("Error attempting JIT compilation ", JITErrorString(error), " ", OpCodesString(rom[target]));
                    return error;
                }
                asml.mov(EAX, 150);
                asml.mov(PC, target);
            virtualize()
            asml.ret();
            runtime.add(&entry, &code);
            return JITError::NonexistentJIT;
        }

        VM_INLINE JITError operator()(JITContext* context)
        {
            if (entry == nullptr) [[unlikely]]
                return JITError::NonexistentJIT;
            entry(context);
            return JITError::Ok;
        }

    private:
        uint32_t count { 0 };
        uint32_t size { 0 };
        JITEntry entry { nullptr };
        CodeHolder code { };
        JitRuntime runtime { };
};

// I might change the way BlockCounters are stored
// so I wanted to create a stable API by wrapping
// whatever container I use.
class BlockCounterCollection
{
    private:
        std::unordered_map<uint32_t, BlockCounter> collection;

    public:
        VM_INLINE BlockCounterCollection() : collection() { }

        VM_INLINE void Add(const uint32_t pos) { collection.emplace(); }
        VM_INLINE void Remove(const uint32_t pos) { collection.erase(pos); }
        VM_INLINE bool Contains(const uint32_t pos) { return collection.contains(pos); }
        VM_INLINE BlockCounter& operator[](const uint32_t pos) { return collection[pos]; }
        VM_INLINE decltype(collection)::iterator begin() { return collection.begin(); }
        VM_INLINE decltype(collection)::iterator end() { return collection.end(); }
};

VM_INLINE JITError BranchIncrease(BlockCounterCollection& blocks, const uint32_t pos, JITContext* context, const BaseROM& rom)
{
    JITError err;
    const uint32_t backup { *context->reg32[6]-1 };
    if (blocks[pos].IsHot() && blocks[pos].IsCompiled())
    {
        std::cout << "INS: " << OpCodesString(rom[pos]) << '\n';
        err = blocks[pos](context);
    }
    else if (blocks[pos].IsHot())
        err = blocks[pos].JIT(pos, rom, context);
    else [[likely]]
    {
        blocks[pos].Increment();
        err = JITError::NonexistentJIT;
    }

    if (err == JITError::NonexistentJIT) [[likely]]
        return err;
    else if (err != JITError::Ok)
    {
        std::cout << "a\n";
        blocks.Remove(pos);
        *context->reg32[6] = backup;
    }

    return err;
}

#else
#endif
