#include <cstring>
#include <bitset>

#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
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
#define Is8BitReg(reg) (Enumc(reg) >= Enumc(RegisterModeFlags::al)) && (Enumc(reg) <= Enumc(RegisterModeFlags::flg))

static sysbit_t& GetRegister32Bit(RegisterModeFlags reg, CPU::State& state)
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

static uchar_t& GetRegister8Bit(RegisterModeFlags reg, CPU::State& state)
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
    // nop
    return System::ErrorCode::Ok;
}

OPR CPU::StoreThirtyTwo(CPU& cpu) noexcept
{
    // stc %i/ui/f <value>
    // stt <byte0..1..2..3>
    
    try_catch(
        Error err { cpu.PushSome(cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4)) };
        if (err == System::ErrorCode::Ok)
            cpu.state.pc+=4;
        return err;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    ) 
}

OPR CPU::StoreEight(CPU& cpu) noexcept
{
    // stc %b/ub <value>
    // ste <byte>
    try_catch(
        Error code { cpu.Push(cpu.board.assembly.Rom().Read(cpu.state.pc)) };
        if (code == System::ErrorCode::Ok)
        cpu.state.pc++;
        return code;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::StoreFromSymbol(CPU& cpu) noexcept
{
    // stc %i/ui/f <symbol>
    // stc %b/ub <symbol>
    //
    // stt <byte0..1..2..3>
    // ste <byte0..1..2..3>
    try_catch(
        sysbit_t size { 
            static_cast<sysbit_t>
            (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::stes ? 1 : 4)
        };

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
    // lda %i/ui/f
    // lda %b/ub
    //
    // ldt
    // lde
    try_catch(
        sysbit_t size {
            static_cast<sysbit_t>
            (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::ldt ? 4 : 1)
        };

        const Slice values { cpu.board.ram.ReadSome(cpu.state.sp-size, size) };
        const sysbit_t alloc { cpu.board.ram.Allocate(size) };

        Error errc { cpu.board.ram.WriteSome(alloc, values) };
        if (errc != System::ErrorCode::Ok)
             return cpu.board.ram.Deallocate(alloc, size);

        cpu.state.ebx = alloc;
        return errc;, 

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::ReadFromHeap(CPU& cpu) noexcept
{
    // rda %i/ui/f/b//ub
    //
    // rdt
    // rde
    try_catch(
        sysbit_t size {
            static_cast<sysbit_t>
            (cpu.board.assembly.Rom().Read(cpu.state.pc-1) == (char)OpCodes::rdt ? 4 : 1) 
        };

        const Slice values { cpu.board.ram.ReadSome(cpu.state.ebx, size) };

        Error errc { cpu.PushSome(values) };
        if (errc != System::ErrorCode::Ok)
            return errc;

        return errc;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::ReadFromRegister(CPU& cpu) noexcept
{
    // rda &eax/ebx/ecx/edx/esi/edi/al/bl/cl/dl/flg/pc/sp
    //
    // rdr <byte>
    try_catch(
        RegisterModeFlags reg { cpu.board.assembly.Rom().Read(cpu.state.pc) };
        sysbit_t size { Is8BitReg(reg) ? sysbit_t{1} : sysbit_t{4} };
        char* data;

        if (Is8BitReg(reg))
            data = BytesFromInteger<uchar_t>(GetRegister8Bit(reg, cpu.state));
        else
            data = BytesFromInteger<sysbit_t>(GetRegister32Bit(reg, cpu.state));

        Error err = cpu.PushSome({
            data,
            size
        });
        
        delete[] data;
        cpu.state.pc++;
        return err;,

        return exc.GetCode();,
        return Error::UnhandledException;
    )
}

OPR CPU::Move(CPU& cpu) noexcept
{
    // mov &eax/ebx/ecx/edx/esi/edi/al/bl/cl/dl/flg/pc/sp
    // mov &eax/ebx/ecx/edx/esi/edi/al/bl/cl/dl/flg/pc/sp &eax/ebx/ecx/edx/esi/edi/al/bl/cl/dl/flg/pc/sp
    // mov <value> &eax/ebx/ecx/edx/esi/edi/al/bl/cl/dl/flg/pc/sp
    //
    // movs <byte>
    // movr <byte> <byte>
    // movc <byte> <byte>
    // movc <byte> <byte0..1..2..3>
    try_catch(
        System::ErrorCode err;
        RegisterModeFlags regFlag { cpu.board.assembly.Rom().Read(cpu.state.pc) };
        sysbit_t size { Is8BitReg(regFlag) ? sysbit_t{1} : sysbit_t{4} };

        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::movc:
            {
                if (size == 1)
                {
                    GetRegister8Bit(regFlag, cpu.state) = cpu.board.assembly.Rom().Read(cpu.state.pc+1);
                    cpu.state.pc+=2;
                }
                else
                {
                    GetRegister32Bit(regFlag, cpu.state) = IntegerFromBytes<sysbit_t>(
                        cpu.board.assembly.Rom().ReadSome(cpu.state.pc+1, 4).data
                    );
                    cpu.state.pc+=5;
                }
                return System::ErrorCode::Ok;
            }
            
            case OpCodes::movs:
            {
                if (size == 1)
                    GetRegister8Bit(regFlag, cpu.state) = IntegerFromBytes<uchar_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-1, 1).data
                    );
                else
                    GetRegister32Bit(regFlag, cpu.state) = IntegerFromBytes<sysbit_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                    );

                cpu.state.pc++;
                return System::ErrorCode::Ok;
            }

            case OpCodes::movr:
            {
                RegisterModeFlags reg2Flag { cpu.board.assembly.Rom().Read(cpu.state.pc+1)};
                sysbit_t size2 { Is8BitReg(reg2Flag) ? sysbit_t{1} : sysbit_t{4} };
                sysbit_t val;

                if (size == 1)
                    val = static_cast<sysbit_t>(GetRegister8Bit(regFlag, cpu.state));
                else
                    val = GetRegister32Bit(regFlag, cpu.state);

                if (size2 == 1)
                    GetRegister8Bit(reg2Flag, cpu.state) = val;
                else
                    GetRegister32Bit(reg2Flag, cpu.state) = val;
                cpu.state.pc+=2;
                return System::ErrorCode::Ok;
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        },

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    ) 
}

OPR CPU::Add32(CPU& cpu) noexcept
{
    try_catch(
        sysbit_t int1;
        sysbit_t int2;
        int1 = IntegerFromBytes<sysbit_t>(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        cpu.PopSome(4);
        int2 = IntegerFromBytes<sysbit_t>(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        cpu.PopSome(4);

        char* data { BytesFromInteger(int1+int2) };
        Error err { cpu.PushSome({
            data,
            4
        })};

        delete[] data;
        return err;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::AddFloat(CPU& cpu) noexcept
{
    try_catch(
        float float1;
        float float2;
        float1 = FloatFromBytes(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        cpu.PopSome(4);
        float2 = FloatFromBytes(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        cpu.PopSome(4);

        char* data { BytesFromFloat<char>(float1+float2) };
        Error err { cpu.PushSome({
            data,
            4
        })};

        delete[] data;
        return err;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::Add8(CPU& cpu) noexcept
{
    try_catch(
        uchar_t byte1;
        uchar_t byte2;
        byte1 = cpu.board.ram.Read(cpu.state.sp-1);
        cpu.Pop();
        byte2 = cpu.board.ram.Read(cpu.state.sp-1);
        cpu.Pop();
        
        Error err { cpu.Push(byte1+byte2) };
        return err;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}
#undef OPR
