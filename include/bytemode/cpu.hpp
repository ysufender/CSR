#pragma once

#include <functional>

#include "extensions/syntaxextensions.hpp"
#include "bytemode/instructions.hpp"
#include "slice.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

class Board;

class CPU
{
    public:
        struct State
        {
            sysbit_t eax { 0 };
            sysbit_t ebx { 0 };
            sysbit_t ecx { 0 };
            sysbit_t edx { 0 };
            sysbit_t esi { 0 };
            sysbit_t edi { 0 };

            sysbit_t pc { 0 };
            sysbit_t sp { 0 };

            uchar_t al { 0 };
            uchar_t bl { 0 };
            uchar_t cl { 0 };
            uchar_t dl { 0 };
            uchar_t flg { 0 };
        };

        CPU() = delete;
        CPU(Board& board);
        
        Error Cycle() noexcept;

        const State& DumpState() const noexcept
        { return this->state; }

        void LoadState(const State& loadFrom) noexcept
        { this->state = loadFrom; }

        Error Push(const char value) noexcept;
        Error Pop() noexcept;

        Error PushSome(const Slice values) noexcept;
        Error PopSome(const sysbit_t size) noexcept;

    private: 
        Board& board;
        State state;

        using OperationFunction = Error (*)(CPU& cpu) noexcept;

#define OPFunc(name) static Error name(CPU& cpu) noexcept;
#define CustomOPF(ret, name, ...) static ret name(CPU& cpu, __VA_ARGS__) noexcept;
#define arr std::array
#define fn std::function<sysbit_t(sysbit_t, sysbit_t)>
        OPFunc(NoOperation)
        OPFunc(StoreThirtyTwo) OPFunc(StoreEight) OPFunc(StoreFromSymbol)
        OPFunc(LoadFromStack)
        OPFunc(ReadFromHeap) OPFunc(ReadFromRegister)
        OPFunc(Move)
        OPFunc(Add32) OPFunc(AddFloat) OPFunc(Add8)
        OPFunc(AddReg)
        OPFunc(AddSafe32) OPFunc(AddSafeFloat) OPFunc(AddSafe8)
        OPFunc(MemCopy)
        OPFunc(Increment) OPFunc(IncrementReg) OPFunc(IncrementSafe)
        OPFunc(Decrement) OPFunc(DecrementReg) OPFunc(DecrementSafe)
        CustomOPF(Error, BitLogic, arr<OpCodes, 3>, fn) OPFunc(BitAnd) OPFunc(BitOr) OPFunc(BitNor)
        OPFunc(SwapTop)
        OPFunc(DuplicateTop)
        OPFunc(RawDataStack)
        OPFunc(Invert) OPFunc(InvertSafe)
        OPFunc(Compare)
        OPFunc(PopInstruction)
        OPFunc(Jump)
        OPFunc(SwapRange) OPFunc(DuplicateRange)
        OPFunc(Repeat)
        OPFunc(Allocate)
        OPFunc(PowRegister) OPFunc(PowStack) OPFunc(PowConst)
#undef fn
#undef arr
#undef CustomOPF
#undef OPFunc
};
