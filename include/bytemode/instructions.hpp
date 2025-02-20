#pragma once

#include "extensions/syntaxextensions.hpp"
#include "CSRConfig.hpp"

constexpr uchar_t NoMode = 0x00;

#define NUMER(E) \
    E(Float) \
    E(Byte) \
    E(UInt) \
    E(UByte)
MAKE_ENUM(NumericModeFlags, Int, 1, NUMER, OUT_CLASS)
#undef NUMER

#define MEMOR(E) \
    E(Heap)
MAKE_ENUM(MemoryModeFlags, Stack, 6, MEMOR, OUT_CLASS)
#undef MEMOR

#define REGOR(E) \
    E(ebx) E(ecx) E(edx) E(esi) E(edi) \
    E(pc) E(sp) \
    E(al) E(bl) E(cl) E(dl) \
    E(flg)
MAKE_ENUM(RegisterModeFlags, eax, 8, REGOR, OUT_CLASS)
#undef REGOR

#define CMPER(E) \
    E(gre) E(equ) E(leq) E(geq) E(neq) 
MAKE_ENUM(CompareModeFlags, les, 21, CMPER, OUT_CLASS)
#undef CMPER

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

