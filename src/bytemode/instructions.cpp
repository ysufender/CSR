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

#define OPR Error
#define NOT_IMP(name) \
        LOGE(System::LogLevel::Low, "Implement ", #name); \
        return System::ErrorCode::Ok;

OPR CPU::NoOperation(CPU& cpu) noexcept
{
    cpu.state.pc++;
    return System::ErrorCode::Ok;
}

OPR CPU::StoreThirtyTwo(CPU& cpu) noexcept
{
    Error err { cpu.PushSome(cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4)) };
    if (err == System::ErrorCode::Ok)
        cpu.state.pc+=4;
    return err;
}

OPR CPU::StoreEight(CPU& cpu) noexcept
{
    Error code { cpu.Push(cpu.board.assembly.Rom().Read(cpu.state.pc)) };
    if (code == System::ErrorCode::Ok)
        cpu.state.pc++;
    return System::ErrorCode::Ok;
}

OPR CPU::StoreThirtyTwoSymbol(CPU& cpu) noexcept
{
    NOT_IMP(StoreThirtyTwoSymbol);
}

OPR CPU::StoreEightSymbol(CPU& cpu) noexcept
{
    NOT_IMP(StoreEightSymbol);
}
#undef OPR
