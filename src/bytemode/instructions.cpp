#include <cstring>
#include <string>

#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
#include "extensions/stringextensions.hpp"
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

#define Enumc(regn) static_cast<char>(regn)
#define Is8BitReg(reg) (reg >= Enumc(RegisterModeFlags::al)) && (reg <= Enumc(RegisterModeFlags::flg))

static sysbit_t& GetRegister32Bit(RegisterModeFlags reg, const CPU::State& state)
{
    static sysbit_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::eax: return state.eax;
        case RegisterModeFlags::ebx: return state.ebx;
        case RegisterModeFlags::ecx: return state.ecx;
        case RegisterModeFlags::edx: return state.edx;
        case RegisterModeFlags::esi: return state.esi;
        case RegisterModeFlags::edi: return state.edi;
        case RegisterModeFlags::pc: return state.pc;
        case RegisterModeFlags::sp: return state.sp;
        default: 
            CRASH(
                System::ErrorCode::InvalidSpecifier,
                RegisterModeFlagsString(reg), " is not a 32bit register."
            );
            return dummy;
    } 
}

static uchar_t& GetRegister8Bit(RegisterModeFlags reg, const CPU::State& state)
{
    static uchar_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::al: return state.al;
        case RegisterModeFlags::bl: return state.bl;
        case RegisterModeFlags::dl: return state.dl;
        case RegisterModeFlags::flg: return state.flg;
        default:
            CRASH(
                System::ErrorCode::InvalidSpecifier,
                RegisterModeFlagsString(reg), " is not an 8bit register."
            );
            return dummy;
    }
}

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

OPR CPU::StoreFromSymbol(CPU& cpu) noexcept
{
    sysbit_t size { 
        static_cast<sysbit_t>
        (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::stes ? 1 : 4)
    };

    try_catch(
        const Slice symbolData { cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4) };
        const sysbit_t symbol { IntegerFromBytes<sysbit_t>(symbolData.data) };
        const Slice valueData { cpu.board.assembly.Rom().ReadSome(symbol, size) };

        Error err { cpu.PushSome(valueData) };
        if (err == Error::Ok)
            cpu.state.pc+=4;     
        return err;,

        return exc.GetCode();,
        return Error::UnhandledException;
    )
}

OPR CPU::LoadFromStack(CPU& cpu) noexcept
{
    sysbit_t size {
        static_cast<sysbit_t>
        (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::ldt ? 4 : 1)
    };

    try_catch(
        const Slice values { cpu.board.ram.ReadSome(cpu.state.sp-size, size) };
        const sysbit_t alloc { cpu.board.ram.Allocate(size) };

        Error errc { cpu.board.ram.WriteSome(alloc, values) };
        if (errc != System::ErrorCode::Ok)
             return cpu.board.ram.Deallocate(alloc, size);

        cpu.state.eax = alloc;
        cpu.state.pc++;
        return errc;, 

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::ReadFromHeap(CPU& cpu) noexcept
{
    sysbit_t size {
        static_cast<sysbit_t>
        (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::rdt ? 4 : 1) 
    };

    try_catch(
        const Slice values { cpu.board.ram.ReadSome(cpu.state.ebx, size) };

        Error errc { cpu.PushSome(values) };
        if (errc != System::ErrorCode::Ok)
            return errc;

        cpu.state.pc++;
        return errc;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::ReadFromRegister(CPU& cpu) noexcept
{
    char reg { cpu.board.assembly.Rom().Read(cpu.state.pc) };
    sysbit_t size { 
        static_cast<sysbit_t>
        (Is8BitReg(reg) ? 1 : 4 )
    };

    if (Is8BitReg(reg))
    {
        uchar_t& regVal { GetRegister8Bit(reg, cpu.DumpState()) };
    }
    else
    {

    }
}
#undef OPR
