#pragma once

#include "CSRConfig.hpp"
#include "system.hpp"

class Board;

class CPU
{
    public:
        CPU() = delete;
        CPU(Board& board);
        
        const System::ErrorCode Cycle() noexcept;

    private: 
        Board& board;

        systembit_t eax = 0;
        systembit_t ebx = 0;
        systembit_t ecx = 0;
        systembit_t edx = 0;
        systembit_t esi = 0;
        systembit_t edi = 0;
        systembit_t pc = 0;
        systembit_t sp = 0;

        uchar_t al = 0;
        uchar_t bl = 0;
        uchar_t cl = 0;
        uchar_t dl = 0;
        uchar_t fl = 0;
};
