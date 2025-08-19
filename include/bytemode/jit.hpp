#pragma once

#include "CSRConfig.hpp"

#ifdef ENABLE_JIT
#include <type_traits>
#include <cstdint>
#include <cassert>
#include <string>
#include <csetjmp>
#include <utility>
#include <csignal>

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include "asmjit/core/globals.h"
#include "asmjit/core/logger.h"

#ifndef ASMJIT_ARCH_X86
    #error "CSR Only supports x86 targeted JIT for now."
#endif

#include "extensions/syntaxextensions.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/baserom.hpp"
#include "extensions/converters.hpp"
#include "platform.hpp"
#include "fastcout.hpp"

#define ERJIT(E) \
    E(NonexistentJIT) \
    E(CompilationError) \
    E(UnsupportedInstruction) \
    E(VMLevelError) \
    E(ExecutionError)
MAKE_ENUM(JITError, Ok, 0, ERJIT, OUT_CLASS)
#undef ERJIT

static constexpr uint32_t THRESHOLD { 1000 };
static constinit size_t TOTAL { 0 };

struct JITContext
{
    // in order: eax, ebx, ecx, edx, esi, edi, pc, sp, bp
    uint32_t* reg32[9];
    
    // in order: al, bl, cl, dl, flg
    uint8_t* reg8[5];

    // pointing at the start of the virtual RAM
    char* ram; 

    // JIT Branching functions
    void* vm;
    bool (*IsHotBlock)(void* vm, const uint32_t);
    bool (*IsCompiled)(void* vm, const uint32_t);
    void (*Increment)(void* vm, const uint32_t);
    void (*(*GetEntry)(void* vm, const uint32_t))(JITContext*);
};

using namespace asmjit;

#ifdef CSR_WIN
    static constexpr x86::Gp contextReg { x86::rcx };
#else
    static constexpr x86::Gp contextReg { x86::rdi };
#endif

#define EAX x86::eax
#define EBX x86::r8d
#define ECX x86::r12d // callee
#define EDX x86::edx
#define PC  x86::r9d
#define SP  x86::r10d
#define BP  x86::r11d
#define T1  x86::r13 // callee
#define T2  x86::r14 // callee

#define FLG static_assert(false, "Unsupported register")
#define AL  x86::sil
#define BL  x86::r15b
#define CL  x86::bl
#define DL  static_assert(false, "Unsupported register")

#define RF1 x86::xmm0
#define RF2 x86::xmm1

#define OFF_REG32   offsetof(JITContext, reg32)
#define OFF_REG8    offsetof(JITContext, reg8)
#define OFF_RAM     offsetof(JITContext, ram)
#define OFF_FN(fn)  offsetof(JITContext, fn)

#define LOAD_PTR(offset, reg) \
    asml.mov(reg, x86::qword_ptr(contextReg, offset)

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

#define machinize() \
    asml.push(x86::r13); \
    asml.push(x86::r14); \
    LOAD_REG32(6, PC); \
    LOAD_REG32(7, SP); \
    LOAD_REG32(8, BP); 

#define virtualize() \
    STORE_REG32(6, PC); \
    STORE_REG32(7, SP); \
    STORE_REG32(8, BP); \
    asml.pop(x86::r14); \
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

#define RAM_READF_SP(offset, targetReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.movss(targetReg, x86::dword_ptr(x86::r13));

#define RAM_READ32_BP(offset, targetReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.mov(targetReg, x86::dword_ptr(x86::r13));

#define RAM_READF_BP(offset, targetReg) \
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

#define RAM_WRITEF_SP(offset, sourceReg) \
    COMPUTE_ADDR(SP, offset); \
    asml.movss(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_WRITE32_BP(offset, sourceReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.mov(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_WRITEF_BP(offset, sourceReg) \
    COMPUTE_ADDR(BP, offset); \
    asml.movss(x86::dword_ptr(x86::r13), sourceReg);

#define RAM_PUSH32(sourceReg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, offsetof(JITContext, ram))); \
    asml.add(x86::r13, SP); \
    asml.mov(x86::dword_ptr(x86::r13), sourceReg); \
    asml.add(SP, 4);

#define RAM_PUSH8(sourceReg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, offsetof(JITContext, ram))); \
    asml.add(x86::r13, SP); \
    asml.mov(x86::byte_ptr(x86::r13), sourceReg); \
    asml.inc(SP);

#define RAM_PUSHF(sourceReg) \
    asml.mov(x86::r13, x86::qword_ptr(contextReg, offsetof(JITContext, ram))); \
    asml.add(x86::r13, SP); \
    asml.mov(x86::dword_ptr(x86::r13), sourceReg); \
    asml.add(SP, 4);



VM_INLINE static bool is_branching(const uchar_t opx)
{
    OpCodes op { opx };
    return
        op == OpCodes::cal || op == OpCodes::calr ||
        /*op == OpCodes::jmp ||*/ op == OpCodes::jmpr ||
        op == OpCodes::cnd || op == OpCodes::cndr ||
        op == OpCodes::cnj;
}

static sigjmp_buf env {};
static void SegfaultHandler(int signum) { siglongjmp(env, 1); }

using JITEntry = void (*)(JITContext*);

JITError JITInstruction(const BaseROM& rom, uint32_t& pc, x86::Assembler& asml, JITContext& context);

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
        VM_INLINE void Increment() { count++; TOTAL++; }
        VM_INLINE bool IsCompiled() const { return entry != nullptr; }

        VM_INLINE JITError JIT(uint32_t target, const BaseROM& rom, JITContext* context)
        {
            // target is the target (which is the start of the current block)
            // context->reg32[6] is where the current call is made. (actually a byte after that)
            CodeHolder code { };
            code.init(runtime.environment());
#ifndef NDEBUG
            StringLogger logger { };
            code.setLogger(&logger);
#endif
            x86::Assembler asml { &code };
            JITError error { JITError::Ok };
            machinize()
                loop_start:
                while (target < rom.Size() && !is_branching(rom[target]))
                {
                    LOGD("Target Instruction: ", OpCodesString(rom[target]));
                    error = JITInstruction(rom, target, asml, *context);
                    if (error == JITError::Ok) [[likely]]
                        continue;
                    code.reset(ResetPolicy::kHard); 
                    LOGD("Error attempting JIT compilation ", JITErrorString(error), " ", OpCodesString(rom[target]));
                    return error;
                }
                asml.mov(PC, target);
            virtualize()
            asml.ret();
#ifndef NDEBUG
            std::cout.write(logger.data(), logger.dataSize());
            std::cout.put('\n');
            FastCout::Flush();
#endif
            runtime.add(&entry, &code);
            return JITError::Ok;
        }

        JITError operator()(JITContext* context)
        {
            if (entry == nullptr) [[unlikely]]
                return JITError::NonexistentJIT;
            signal(SIGSEGV, SegfaultHandler);
            if (sigsetjmp(env, 1) == 0) [[likely]]
            {
                entry(context);
                signal(SIGSEGV, SIG_DFL);
                //std::cout << "Hah\n";
                return JITError::Ok;
            }
            else [[unlikely]]
            {
                signal(SIGSEGV, SIG_DFL);
                return JITError::ExecutionError;
            }
        }

        VM_INLINE JITEntry GetEntry() const { return entry; }

    private:
        uint32_t count { 0 };
        uint32_t size { 0 };
        JITEntry entry { nullptr };
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

        VM_INLINE void Add(const uint32_t pos) {
            LOGD("Adding block ", std::to_string(pos));
            collection.emplace(std::piecewise_construct, std::forward_as_tuple(pos), std::forward_as_tuple());
        }
        VM_INLINE void Remove(const uint32_t pos) { 
            LOGD("Removing block ", std::to_string(pos));
            collection.erase(pos);
        }
        VM_INLINE bool Contains(const uint32_t pos) { return collection.contains(pos); }
        VM_INLINE BlockCounter& operator[](const uint32_t pos) { return collection[pos]; }
        VM_INLINE decltype(collection)::iterator begin() { return collection.begin(); }
        VM_INLINE decltype(collection)::iterator end() { return collection.end(); }
};

VM_INLINE JITError BranchIncrease(BlockCounterCollection& blocks, const uint32_t pos, JITContext* context, const BaseROM& rom, bool jit)
{
    if (!jit)
        return JITError::NonexistentJIT;

    if (!blocks.Contains(pos))
        return JITError::NonexistentJIT;

    JITError err { JITError::NonexistentJIT };
    BlockCounter& block { blocks[pos] };
    if (!block.IsHot())
    {
        if (TOTAL > THRESHOLD*10) [[unlikely]]
            blocks.Remove(pos);
        else [[likely]]
            block.Increment();
        return err;
    }
    else if (!block.IsCompiled()) 
    {
        if (block.JIT(pos, rom, context) != JITError::Ok)
        {
            blocks.Remove(pos);
            return JITError::NonexistentJIT;  
        }
        return JITError::NonexistentJIT;
    }

    const uint32_t pcBackup { *context->reg32[6] };
    if (block(context) == JITError::Ok) [[likely]]
    {
        assert(pcBackup != *context->reg32[6]);
        return JITError::Ok;
    }
    blocks.Remove(pos);
    *context->reg32[6] = pcBackup;
    return JITError::ExecutionError;
}

#else
#endif
