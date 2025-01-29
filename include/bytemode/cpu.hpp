#pragma once

#include "CSRConfig.hpp"
#include "extensions/syntaxextensions.hpp"
#include "system.hpp"

class Board;

using OperationFunction = const System::ErrorCode(*)() noexcept;

class CPU
{
    public:
        struct State
        {
            sysbit_t eax;
            sysbit_t ebx;
            sysbit_t ecx;
            sysbit_t edx;
            sysbit_t esi;
            sysbit_t edi;

            sysbit_t pc;
            sysbit_t sp;

            uchar_t al;
            uchar_t bl;
            uchar_t cl;
            uchar_t dl;
            uchar_t fl;
        };

        CPU() = delete;
        CPU(Board& board);
        
        const System::ErrorCode Cycle() noexcept;
        const State& DumpState(State& dumpTo) noexcept;
        const State& LoadState(const State& loadFrom) noexcept;

    private: 
        Board& board;
        State state;
};

namespace
{
    namespace ALU
    {
        using OPR = const System::ErrorCode;

        OPR MoveReg() noexcept
        {
            LOGE(System::LogLevel::Medium, "Implement ", nameof(MoveReg));
            return System::ErrorCode::Ok;
        }
    }
}
