#pragma once

#include "extensions/syntaxextensions.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"
#include <string>

class Board;


enum class OpCodes : char
{
    nop,
    stt, ste,
    stts, stes,
    ldt, lde,
    rdt, rde, rdr,
    movc, movs, movr,
    addi, addf, addb, addri, addrf, addrb, addsi, addsf, addsb,
    mcp,
    inci, incf, incb, incri, incrf, incrb, incsi, incsf, incsb,
    dcri, dcrf, dcrb, dcrri, dcrrf, dcrrb, dcrsi, dcrsf, dcrsb,
    andst, andse, andr,
    orst, orse, orr,
    norst, norse, norr,
    swpt, swpe, swpr,
    dupt, dupe,
    raw , raws,
    invt, inve, invr, invst, invse,
    cmp, cmpr,
    popt, pope,
    jmp, jmpr,
    swr,
    dur,
    rep,
    alc,
    powri, powrf, powrb, powsi, powsf, powsb, powi, powf, powb,
    powrui, powrub, powsui, powsub, powui, powub,
    sqri, sqrf, sqrb, sqrri, sqrrf, sqrrb, sqrsi, sqrsf, sqrsb,
    cnd, cndr,
    cal, calr,
    muli, mulf, mulb, mulri, mulrf, mulrb, mulsi, mulsf, mulsb,
    divi, divf, divb, divri, divrf, divrb, divsi, divsf, divsb,
    ret,
};

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
            uchar_t fl { 0 };
        };

        CPU() = delete;
        CPU(Board& board);
        
        const System::ErrorCode Cycle() noexcept;

        const State& DumpState() const noexcept
        { return this->state; }

        void LoadState(const State& loadFrom) noexcept
        { this->state = loadFrom; }

    private: 
        Board& board;
        State state;

        using OPR = const System::ErrorCode;
        using OperationFunction = OPR (*)(CPU& cpu) noexcept;
        static OPR MoveReg(CPU& cpu) noexcept;
};

