#pragma once

#include "CSRConfig.hpp"
#include "system.hpp"

class Board;

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
        const System::ErrorCode DumpState(State& dumpTo);
        const System::ErrorCode LoadState(const State& loadFrom);

    private: 
        Board& board;
        State state;
};
