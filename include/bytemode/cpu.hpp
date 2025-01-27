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

        sysbit_t eax = 0;
        sysbit_t ebx = 0;
        sysbit_t ecx = 0;
        sysbit_t edx = 0;
        sysbit_t esi = 0;
        sysbit_t edi = 0;
        sysbit_t pc = 0;
        sysbit_t sp = 0;

        uchar_t al = 0;
        uchar_t bl = 0;
        uchar_t cl = 0;
        uchar_t dl = 0;
        uchar_t fl = 0;
};
