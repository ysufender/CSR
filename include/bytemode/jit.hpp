#pragma once

#include "CSRConfig.hpp"
#include "bytemode/instructions.hpp"
#include "extensions/syntaxextensions.hpp"

#ifdef ENABLE_JIT
#define ASMJIT_EMBED
#define ASMJIT_NO_FOREIGN
#define ASMJIT_NO_BUILDER

#include "platform.hpp"
#include "system.hpp"

#ifdef BUILD_FLAT
#include "bytemode/flat/flatrom.hpp"
#endif

#ifdef BUILD_STRUCTURED
#include "bytemode/structured/rom.hpp"
#endif

#define ERJIT(E) \
    E(NonexistentJIT) \
    E(InternalErr) \
    E(CompilationErr)
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

using JITEntry = void (*)(JITContext*);

class BlockCounter
{
    public:
        VM_INLINE BlockCounter() : entry(nullptr), count(0) { }

        VM_INLINE bool IsHot() const { return count > THRESHOLD; }
        VM_INLINE void Increment() { count++; }
        VM_INLINE bool IsCompiled() const { return entry != nullptr; }

#ifdef BUILD_FLAT
        JITError JIT(const FlatROM& rom, JITContext* context)
        {
            return JITError::NonexistentJIT;
        }
#endif
#ifdef BUILD_STRUCTURED
        JITError JIT(const ROM& rom);
#endif

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
};

// I might change the way BlockCounters are stored
// so I wanted to create a stable API by wrapping
// whatever container I use.
class BlockCounterCollection
{
    public:
        VM_INLINE BlockCounterCollection() : collection() { }

        VM_INLINE void Add(const uint32_t pos) { collection[pos] = { }; }
        VM_INLINE void Remove(const uint32_t pos) { collection.erase(pos); }
        VM_INLINE bool Contains(const uint32_t pos) { return collection.contains(pos); }
        VM_INLINE BlockCounter& operator[](const uint32_t pos) { return collection[pos]; }

    private:
        std::unordered_map<uint32_t, BlockCounter> collection;
};

#ifdef BUILD_FLAT
VM_INLINE JITError BranchIncrease(BlockCounterCollection& blocks, const uint32_t pos, JITContext* context, const FlatROM& rom)
{
    JITError err;
    const uint32_t backup { *context->reg32[6] };
    if (blocks[pos].IsHot() && blocks[pos].IsCompiled()) [[unlikely]]
        err = blocks[pos](context);
    else if (blocks[pos].IsHot()) [[unlikely]]
        err = blocks[pos].JIT(rom, context);
    else [[likely]]
    {
        blocks[pos].Increment();
        err = JITError::NonexistentJIT;
    }

    if (err == JITError::NonexistentJIT) [[likely]]
        return err;
    else if (err != JITError::Ok)
    {
        blocks.Remove(pos);
        *context->reg32[6] = backup;
    }

    return err;
}
#endif

#ifdef BUILD_STRUCTURED
VM_INLINE JITError BranchIncrease(BlockCounterCollection& blocks, const uint32_t pos, JITContext* context)
{
}
#endif


#else
#endif
