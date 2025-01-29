#include <string>

#include "CSRConfig.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/board.hpp"
#include "bytemode/cpu.hpp"
#include "system.hpp"

constexpr uchar_t NoMode = 0x00;

#define NUMER(ENUMER) \
    ENUMER(Int = 1) \
    ENUMER(Float) \
    ENUMER(Byte) \
    ENUMER(UIntr) \
    ENUMER(UByte)
MAKE_ENUM(NumericModeFlags, NUMER, 1)
#undef NUMER

#define MEMOR(ENUMER) \
    ENUMER(Stack = 6) \
    ENUMER(Heap)
MAKE_ENUM(MemoryModeFlags, MEMOR, 6)
#undef MEMOR

#define REGOR(ENUMER) \
    ENUMER(eax = 8) ENUMER(ebx) ENUMER(ecx) ENUMER(edx) ENUMER(esi) ENUMER(edi) \
    ENUMER(pc) ENUMER(sp) \
    ENUMER(al) ENUMER(bl) ENUMER(cl) ENUMER(dl) \
    ENUMER(flg)
MAKE_ENUM(RegisterModeFlags, REGOR, 8)
#undef REGOR

enum class CompareModeFlags : uchar_t
{
    les = 21, gre, equ, leq, geq, neq,
};

using OPR = const System::ErrorCode;
OPR CPU::MoveReg(CPU& cpu) noexcept
{
    LOGE(System::LogLevel::Medium, "Implement ", nameof(MoveReg));
    LOGD(std::to_string(cpu.board.Assembly().Rom().Size()));

    return System::ErrorCode::Ok;
}
