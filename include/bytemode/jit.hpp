#pragma once

#include "CSRConfig.hpp"

#ifdef ENABLE_JIT
#include "platform.hpp"
#include "system.hpp"

#ifdef BUILD_FLAT
#include "bytemode/flat/flatrom.hpp"
#endif

#ifdef BUILD_STRUCTURED
#include "bytemode/structured/rom.hpp"
#endif

enum class JITError : char
{
    Ok,
    NonexistentJIT,
    InternalErr,
    CompilationErr
};

static constexpr uint32_t TRESHOLD = 1000;

struct JITContext
{
    // in order: eax, ebx, ecx, edx, esi, edi, pc, sp, bp
    uint32_t* reg32[9];
    
    // in order: al, bl, cl, dl, flg
    uint8_t* reg8[5];
};

using JITEntry = void (*)();

class BlockCounter
{
    public:
        VM_INLINE BlockCounter() : entry(nullptr), count(0) { }

        VM_INLINE bool IsHot() const { return count > TRESHOLD; }
        VM_INLINE void Increase() { count++; }

#ifdef BUILD_FLAT
        JITError JIT(const FlatROM& rom);
#endif
#ifdef BUILD_STRUCTURED
        JITError JIT(const ROM& rom);
#endif

        VM_INLINE JITError operator()()
        {
            if (entry == nullptr)
                return JITError::NonexistentJIT;
            return JITError::Ok;
        }

    private:
        uint32_t count;
        uint32_t size;
        JITEntry entry;
};

// I might change the way BlockCounters are stored
// so I wanted to create a stable API by wrapping
// whatever container I use.
class BlockCounterCollection
{
    public:
        VM_INLINE BlockCounterCollection() : collection() { }

        VM_INLINE BlockCounter& operator[](const uint32_t id) { return collection[id]; }

    private:
        std::unordered_map<uint32_t, BlockCounter> collection;
};

#endif
