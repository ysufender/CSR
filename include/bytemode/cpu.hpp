#pragma once

#include "extensions/syntaxextensions.hpp"
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
            uchar_t fl { 0 };
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

#define OPR static Error
#define OPFunc(name) OPR name(CPU& cpu) noexcept;
        using OperationFunction = Error (*)(CPU& cpu) noexcept;

        OPFunc(NoOperation)
        OPFunc(StoreThirtyTwo)
        OPFunc(StoreEight)
        OPFunc(StoreThirtyTwoSymbol)
        OPFunc(StoreEightSymbol)
#undef OPR
};

#define OPER(E) \
    E(stt) E(ste) \
    E(stts) E(stes) \
    E(ldt) E(lde) \
    E(rdt) E(rde) E(rdr) \
    E(movc) E(movs) E(movr) \
    E(addi) E(addf) E(addb) E(addri) E(addrf) E(addrb) E(addsi) E(addsf) E(addsb) \
    E(mcp) \
    E(inci) E(incf) E(incb) E(incri) E(incrf) E(incrb) E(incsi) E(incsf) E(incsb) \
    E(dcri) E(dcrf) E(dcrb) E(dcrri) E(dcrrf) E(dcrrb) E(dcrsi) E(dcrsf) E(dcrsb) \
    E(andst) E(andse) E(andr) \
    E(orst) E(orse) E(orr) \
    E(norst) E(norse) E(norr) \
    E(swpt) E(swpe) E(swpr) \
    E(dupt) E(dupe) \
    E(raw ) E(raws) \
    E(invt) E(inve) E(invr) E(invst) E(invse) \
    E(cmp) E(cmpr) \
    E(popt) E(pope) \
    E(jmp) E(jmpr) \
    E(swr) \
    E(dur) \
    E(rep) \
    E(alc) \
    E(powri) E(powrf) E(powrb) E(powsi) E(powsf) E(powsb) E(powi) E(powf) E(powb) \
    E(powrui) E(powrub) E(powsui) E(powsub) E(powui) E(powub) \
    E(sqri) E(sqrf) E(sqrb) E(sqrri) E(sqrrf) E(sqrrb) E(sqrsi) E(sqrsf) E(sqrsb) \
    E(cnd) E(cndr) \
    E(cal) E(calr) \
    E(muli) E(mulf) E(mulb) E(mulri) E(mulrf) E(mulrb) E(mulsi) E(mulsf) E(mulsb) \
    E(divi) E(divf) E(divb) E(divri) E(divrf) E(divrb) E(divsi) E(divsf) E(divsb) \
    E(ret)
MAKE_ENUM(OpCodes, nop, 0, OPER, OUT_CLASS)
#undef OPER
