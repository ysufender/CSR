#pragma once

#include "CSRConfig.hpp"

#ifdef ENABLE_JIT
#include <cstdint>
#include <cstddef>
#include <string>
#include <cassert>
#include <unistd.h>
#include <csetjmp>
#include <utility>
#include <csignal>
#include <unordered_map>

#define DUMP_ASM

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include "asmjit/core/globals.h"
#ifdef DUMP_ASM
#include "asmjit/core/logger.h"

#include "fastcout.hpp"
#endif

#include "platform.hpp"

#if !defined(NDEBUG) && defined(CSR_UNIX)
#include "execinfo.h"
#endif

#if ASMJIT_ARCH_X86 != 64
    #error "CSR Only supports x86_64 64bits targeted JIT for now."
#endif

#include "extensions/syntaxextensions.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/baserom.hpp"

#define ERJIT(E) \
    E(NonexistentJIT) \
    E(CompilationError) \
    E(UnsupportedInstruction) \
    E(VMLevelError) \
    E(ExecutionErrorSegv) \
    E(Finish)
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
    bool (*IsCompiled)(void* vm, const uint32_t);
    void (*(*GetEntry)(void* vm, const uint32_t))(JITContext*);
};

#define reax 0*sizeof(uint32_t*)
#define rebx 1*sizeof(uint32_t*)
#define recx 2*sizeof(uint32_t*)
#define redx 3*sizeof(uint32_t*)
#define resi 4*sizeof(uint32_t*)
#define redi 5*sizeof(uint32_t*)
#define rpc  6*sizeof(uint32_t*)
#define rsp  7*sizeof(uint32_t*)
#define rbp  8*sizeof(uint32_t*)
#define ral  0*sizeof(uint8_t*)
#define rbl  1*sizeof(uint8_t*)
#define rcl  2*sizeof(uint8_t*)
#define rdl  3*sizeof(uint8_t*)
#define rflg 4*sizeof(uint8_t*)

using namespace asmjit;

#ifdef CSR_WIN
    static constexpr x86::Gp firstParamReg { x86::rcx };
    static conxtexpr x86::Gp secondParamReg { x86::rdx };
#else
    static constexpr x86::Gp firstParamReg { x86::rdi };
    static constexpr x86::Gp secondParamReg { x86::rsi };
#endif

#define EAX x86::eax
#define EBX x86::r8d
#define ECX x86::r12d // callee
#define EDX x86::edx
#define PC  x86::r9d
#define SP  x86::r10d
#define BP  x86::r11d

#define T1  x86::rax
#define T2  x86::rdx
#define T3  x86::r8
#define rreg32 x86::r8
#define rreg8 x86::r9
#define rram x86::r10

//#define T1  x86::r13 // callee
//#define T2  x86::r14 // callee
#ifdef CSR_WIN
//#define T3 x86::rdi; // callee
#else
//#define T3 x86::rcx
#endif

#define FLG static_assert(false, "Unsupported register")
#define AL  x86::sil
#define BL  x86::r15b // callee
#define CL  x86::bl
#define DL  static_assert(false, "Unsupported register")

#define RF1 x86::xmm0
#define RF2 x86::xmm1

#ifdef CSR_WIN
#define push_backup_asml() \
    asml.push(r12); \
    asml.push(T1); \
    asml.push(T2); \
    asml.push(r15); \
    asml.push(T3); 
#else
#define push_backup_asml() \
    asml.push(r12); \
    asml.push(T1); \
    asml.push(T2); \
    asml.push(r15);
#endif

#define backup() \
    push_backup_asml() \
    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, reg32))); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 0)); \
    asml.mov(EAX, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 1)); \
    asml.mov(EBX, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 2)); \
    asml.mov(ECX, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 3)); \
    asml.mov(EDX, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 6)); \
    asml.mov(PC, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 7)); \
    asml.mov(SP, dword_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 8)); \
    asml.mov(BP, dword_ptr(T2)); \
    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, reg8))); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 0)); \
    asml.mov(AL, byte_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 1)); \
    asml.mov(BL, byte_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 2)); \
    asml.mov(CL, byte_ptr(T2));

#ifdef CSR_WIN
#define pop_restore_asml() \
    asml.pop(T3); \
    asml.pop(r15); \
    asml.pop(T2); \
    asml.pop(T1); \
    asml.pop(r12);
#else
#define pop_restore_asml() \
    asml.pop(r15); \
    asml.pop(T2); \
    asml.pop(T1); \
    asml.pop(r12);
#endif

#define restore() \
    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, reg32))); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 0)); \
    asml.mov(dword_ptr(T2), EAX); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 1)); \
    asml.mov(dword_ptr(T2), EBX); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 2)); \
    asml.mov(dword_ptr(T2), ECX); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 3)); \
    asml.mov(dword_ptr(T2), EDX); \
    asml.mov(PC, pc); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 6)); \
    asml.mov(dword_ptr(T2), PC); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 7)); \
    asml.mov(dword_ptr(T2), SP); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint32_t*) * 8)); \
    asml.mov(dword_ptr(T2), BP); \
    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, reg8))); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 0)); \
    asml.mov(AL, byte_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 1)); \
    asml.mov(BL, byte_ptr(T2)); \
    asml.mov(T2, qword_ptr(T1, sizeof(uint8_t*) * 2)); \
    asml.mov(CL, byte_ptr(T2)); \
    pop_restore_asml()

VM_INLINE static bool is_branching(const uchar_t opx)
{
    OpCodes op { opx };
    return
        op == OpCodes::cal || op == OpCodes::calr ||
        /*op == OpCodes::jmp ||*/ op == OpCodes::jmpr ||
        op == OpCodes::cnd || op == OpCodes::cndr ||
        op == OpCodes::cnj ||
        op == OpCodes::ret;
}

static sigjmp_buf env {};
static void SegfaultHandler(int signum)
{
#if !defined(NDEBUG) && defined(CSR_UNIX)
    void* array[10];
    int size { backtrace(array, sizeof(array)/sizeof(void*)) };
    std::cerr << "Error: signal " << signum << '\n';
    backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
    siglongjmp(env, 1);
}

using JITEntry = void (*)(JITContext*);

JITError JITInstruction(const BaseROM& rom, uint32_t& pc, x86::Assembler& asml, JITContext& context, const uint32_t start);

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
#ifdef DUMP_ASM
        VM_INLINE void Dump() const { std::cout.write(logger.data(), logger.dataSize()), std::cout.put('\n'); }
#endif

        VM_INLINE JITError JIT(uint32_t pc, const BaseROM& rom, JITContext* context)
        {
            LOGD("JIT-ing block ", std::to_string(pc));
            // target is the target (which is the start of the current block)
            // context->reg32[6] is where the current call is made. (actually a byte after that)
            CodeHolder code { };
            code.init(runtime.environment());
#ifdef DUMP_ASM
            code.setLogger(&logger);
#endif
            x86::Assembler asml { &code };
            JITError error { JITError::Ok };
            
            using namespace x86;

            // Setup
            // backup()
            asml.mov(rreg32, qword_ptr(firstParamReg, offsetof(JITContext, reg32)));
            asml.mov(rreg8, qword_ptr(firstParamReg, offsetof(JITContext, reg8)));
            asml.mov(rram, qword_ptr(firstParamReg, offsetof(JITContext, ram)));

            const uint32_t start { pc };
            while (pc < rom.Size() && !is_branching(rom[pc]))
            {
                LOGD(OpCodesString(rom[pc]));
                const char* lbln { std::to_string(pc).c_str() };
                Label lbl { asml.newNamedLabel(lbln) };
                asml.bind(lbl);
                error = JITInstruction(rom, pc, asml, *context, start);
                if (error == JITError::Ok) [[likely]]
                    continue;
                else if (error == JITError::Finish || error == JITError::UnsupportedInstruction)
                    break;
                code.reset(ResetPolicy::kHard); 
                LOGD("Error attempting JIT compilation ", JITErrorString(error), " ", OpCodesString(rom[pc]));
                return error;
            }

            if (start-4 <= pc && pc <= start+4)
            {
                code.reset(ResetPolicy::kHard);
                LOGD("Block was not JIT compliant, reverting...");
                return JITError::CompilationError;
            }

            // Finish
            // restore()
            LOGD("Start: ", std::to_string(start), " PC: ", std::to_string(pc));
            asml.mov(dword_ptr(rreg32, rpc), pc);

            asml.ret();

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
                return JITError::Ok;
            }
            else [[unlikely]]
            {
                Dump();
                signal(SIGSEGV, SIG_DFL);
                return JITError::ExecutionErrorSegv;
            }
        }

        VM_INLINE JITEntry GetEntry() const { return entry; }

    private:
        uint32_t count { 0 };
        uint32_t size { 0 };
        JITEntry entry { nullptr };
        JitRuntime runtime { };
#ifdef DUMP_ASM
        StringLogger logger { };
#endif
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
        err = block.JIT(pos, rom, context);
        if (err != JITError::Ok)
        {
            LOGD("Error while JIT-ing block ", std::to_string(pos), " ", JITErrorString(err));
            blocks.Remove(pos);
            return JITError::NonexistentJIT;  
        }
        return JITError::NonexistentJIT;
    }

    const uint32_t pcBackup { *context->reg32[6] };
    err = block(context);
    if (err == JITError::Ok) [[likely]]
    {
#ifndef NDEBUG
        if (pcBackup == *context->reg32[rpc/8])
        {
            block.Dump();
            LOGD("For some reason, PC is ", std::to_string(*context->reg32[rpc/8]));
            FastCout::Flush();
            CRASH(System::ErrorCode::JITError);
        }
#endif
        return JITError::Ok;
    }

    LOGD("Error while executing block ", std::to_string(pos), " ", JITErrorString(err));
    blocks.Remove(pos);
    *context->reg32[6] = pcBackup;
    return err;
}

#else
#endif
