#include <cstring>
#include <string>

#include "CSRConfig.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/board.hpp"
#include "bytemode/cpu.hpp"
#include "system.hpp"

constexpr uchar_t NoMode = 0x00;

#define NUMER(E) \
    E(Float) \
    E(Byte) \
    E(UIntr) \
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

#define OPR const System::ErrorCode
#define NOT_IMP(name) LOGE(System::LogLevel::Low, "Implement ", #name)
OPR CPU::Nop(CPU& cpu) noexcept
{
    cpu.state.pc++;
    return System::ErrorCode::Ok;
}

OPR CPU::STT(CPU& cpu) noexcept
{
    // Store 32-bit raw value on stack
    System::ErrorCode code { cpu.board.ram.WriteSome(
        cpu.state.sp,
        4,
        cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
    )};

    cpu.state.pc+=5;
    cpu.state.sp+=4;
    return code;
}

OPR CPU::STE(CPU& cpu) noexcept
{
    NOT_IMP(STE);
    System::ErrorCode code { cpu.board.ram.WriteSome(
        cpu.state.sp,
        1,
        cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 1).data
    )};

    cpu.state.pc+=2;
    cpu.state.sp+=1;
    return System::ErrorCode::Ok;
}

OPR CPU::MovReg(CPU& cpu) noexcept
{
    NOT_IMP(MoveReg);
    return System::ErrorCode::Ok;
}
#undef OPR
