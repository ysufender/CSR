#include <cstring>
#include <string>

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

OPR CPU::AddReg(CPU& cpu) noexcept
{
    try_catch(
        OpCodes op { cpu.board.assembly.Rom().Read(cpu.state.pc-1) };
        RegisterModeFlags reg1 { cpu.board.assembly.Rom().Read(cpu.state.pc) };
        RegisterModeFlags reg2 { cpu.board.assembly.Rom().Read(cpu.state.pc+1) };
        
        if (
            /*case 1*/ 
            ((op == OpCodes::addrb)
            &&
            (!Is8BitReg(reg1) || !Is8BitReg(reg2)))
            ||

            /*case 2*/
            ((op != OpCodes::addrb)
            &&
            (Is8BitReg(reg1) || Is8BitReg(reg2)))
        )
            CRASH(System::ErrorCode::InvalidSpecifier,
                "In ", cpu.board.Stringify(), ", PC: ", std::to_string(cpu.state.pc-1),
                " ", OpCodesString(op),
                " ", RegisterModeFlagsString(reg1),
                " ", RegisterModeFlagsString(reg2),
                " Given registers are not compatible with given numeric type."
            );

        cpu.state.pc+=2;

        if (Is8BitReg(reg1))
        {
            uchar_t reg1ref { GetRegister8Bit(reg1, cpu.state) };
            uchar_t& reg2ref { GetRegister8Bit(reg2, cpu.state) };
            reg2ref += reg1ref;
        }
        else if (op == OpCodes::addrf)
        {
            sysbit_t reg1ref { GetRegister32Bit(reg1, cpu.state) };
            sysbit_t& reg2ref { GetRegister32Bit(reg2, cpu.state) };

            char* data { BytesFromInteger(reg1ref) };
            float float1 { FloatFromBytes(
                data
            )};
            delete[] data;

            data = BytesFromInteger(reg2ref);
            float float2 { FloatFromBytes(
                data
            )};
            delete[] data;

            data = BytesFromFloat(float1+float2);
            reg2ref = IntegerFromBytes<sysbit_t>(
                data
            );
            delete[] data;
        }
        else
        {
            sysbit_t reg1ref { GetRegister32Bit(reg1, cpu.state) };
            sysbit_t& reg2ref { GetRegister32Bit(reg2, cpu.state) };
            reg2ref += reg1ref;
        }

        return System::ErrorCode::Ok;,

        return exc.GetCode();,
        return Error::UnhandledException;
    )
}

OPR CPU::AddSafe32(CPU& cpu) noexcept
{
    try_catch(
        sysbit_t int1;
        sysbit_t int2;
        int1 = IntegerFromBytes<sysbit_t>(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        int2 = IntegerFromBytes<sysbit_t>(cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data);

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

OPR CPU::AddSafeFloat(CPU& cpu) noexcept
{
    try_catch(
        float float1;
        float float2;
        float1 = FloatFromBytes(cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data);
        float2 = FloatFromBytes(cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data);

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

OPR CPU::AddSafe8(CPU& cpu) noexcept
{
    try_catch(
        uchar_t byte1;
        uchar_t byte2;
        byte1 = cpu.board.ram.Read(cpu.state.sp-1);
        byte2 = cpu.board.ram.Read(cpu.state.sp-2);
        
        Error err { cpu.Push(byte1+byte2) };
        return err;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::MemCopy(CPU& cpu) noexcept
{
    // mcp <4bits> <4bits>
    // bits are memory mode flags
    try_catch(
        uchar_t compressedModes { static_cast<uchar_t>(cpu.board.assembly.Rom().Read(cpu.state.pc)) };
        MemoryModeFlags from { MemoryModeFlags(compressedModes >> 4) };
        MemoryModeFlags to { MemoryModeFlags(compressedModes & 0x0F) };

        sysbit_t fromAddr { GetRegister32Bit(RegisterModeFlags::eax, cpu.state) };
        sysbit_t toAddr { GetRegister32Bit(RegisterModeFlags::ebx, cpu.state) };
        sysbit_t size { GetRegister32Bit(RegisterModeFlags::ecx, cpu.state) };

        auto modeCheck = [&cpu](MemoryModeFlags flag, sysbit_t addr) -> bool {
            if (flag == MemoryModeFlags::Stack)
                return (addr < cpu.board.ram.StackSize());
            return ((addr >= cpu.board.ram.StackSize()) && (addr < cpu.board.ram.Size()));
        };

        if (!modeCheck(from, fromAddr) || !modeCheck(to, toAddr))
        {
            LOGE(
                System::LogLevel::Medium,
                "In ", cpu.board.Stringify(), nameof(MemCopy) , " missmatching memory modes and addresses.\n",
                "From Flag: ", MemoryModeFlagsString(from), " From Addr: ", std::to_string(fromAddr),
                "\nTo Flag: ", MemoryModeFlagsString(to), " To Addr: ", std::to_string(toAddr),
                "\n Heap Start: ", std::to_string(cpu.board.ram.StackSize())
            );
            return System::ErrorCode::RAMAccessError;
        }
        
        if ((toAddr + size) > cpu.board.ram.Size())
        {
            LOGE(
                System::LogLevel::Medium,
                "In ", cpu.board.Stringify(), nameof(MemCopy), " instruction will cause memory overflow.",
                "\nTo Address: ", std::to_string(toAddr), " Size: ", std::to_string(size),
                "\nMemory Size: ", std::to_string(cpu.board.ram.Size())
            );
            return System::ErrorCode::MemoryOverflow;
        }

        if (to == MemoryModeFlags::Stack && (toAddr + size) >= cpu.board.ram.StackSize())
            LOGW(
                "In ", cpu.board.Stringify(), nameof(MemCopy), 
                " instruction will overflow from stack and overwrite heap."
            );

        Slice dataToCopy { cpu.board.ram.ReadSome(fromAddr, size) };
        Error code { cpu.board.ram.WriteSome(toAddr, dataToCopy) };

        if (code == System::ErrorCode::Ok)
            cpu.state.pc++;

        return code;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::Increment(CPU& cpu) noexcept
{
    // inc[type] <value> 
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::inci:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data 
                )};

                sysbit_t stack { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                char* data { BytesFromInteger(stack+amount) };
                Error code { cpu.board.ram.WriteSome(
                    cpu.state.sp-4,
                    {data, 4}
                )};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::incf:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )}; 

                float stack { FloatFromBytes(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data      
                )};

                char* data { BytesFromFloat(amount+stack) };
                Error code { cpu.board.ram.WriteSome(
                    cpu.state.sp-4,
                    {data, 4}
                )};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::incb:
            {
                if (cpu.state.sp < 1)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                )};
                uchar_t stack { static_cast<uchar_t>(
                    cpu.board.ram.Read(cpu.state.sp-1)
                )};

                Error code { cpu.board.ram.Write(
                    cpu.state.sp-1,
                    amount + stack
                )};

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc++;

                return code;
            }

            default:
                return Error::InvalidSpecifier;        
        }

        return System::ErrorCode::Ok;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::IncrementReg(CPU& cpu) noexcept
{
    // incr[type] <value> 
    // register size checks are done at assemble-time
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::incri:
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc+1, 4).data
                )};

                reg += amount;

                cpu.state.pc+=5;

                return System::ErrorCode::Ok;
            }

            case OpCodes::incrf:
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                char* data { BytesFromInteger(reg) };
                float regVal { FloatFromBytes(data)}; 
                delete[] data;

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc+1, 4).data      
                )};

                data = BytesFromFloat(regVal+amount);
                reg = IntegerFromBytes<sysbit_t>(data);
                delete[] data;

                cpu.state.pc+=5;
                return System::ErrorCode::Ok;
            }

            case OpCodes::incrb:
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc+1)
                )};

                reg += amount;

                cpu.state.pc+=2;
                return System::ErrorCode::Ok;
            }

            default:
                return Error::InvalidSpecifier;        
        },

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::IncrementSafe(CPU& cpu) noexcept
{
    // inc[type] <value> 
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::incsi:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data 
                )};

                sysbit_t stack { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                char* data { BytesFromInteger(stack+amount) };
                Error code { cpu.PushSome({
                    data, 
                    4
                })};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::incsf:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )}; 

                float stack { FloatFromBytes(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data      
                )};

                char* data { BytesFromFloat(amount+stack) };
                Error code { cpu.PushSome({
                    data,
                    4
                })};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::incsb:
            {
                if (cpu.state.sp < 1)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                )};
                uchar_t stack { static_cast<uchar_t>(
                    cpu.board.ram.Read(cpu.state.sp-1)
                )};

                Error code { cpu.Push(
                    amount + stack
                )};

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc++;

                return code;
            }

            default:
                return Error::InvalidSpecifier;        
        }

        return System::ErrorCode::Ok;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    ) 
}

OPR CPU::Decrement(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::dcri:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data 
                )};

                sysbit_t stack { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                char* data { BytesFromInteger(stack - amount) };
                Error code { cpu.board.ram.WriteSome(
                    cpu.state.sp-4,
                    {data, 4}
                )};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::dcrf:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )}; 

                float stack { FloatFromBytes(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data      
                )};

                char* data { BytesFromFloat(stack - amount) };
                Error code { cpu.board.ram.WriteSome(
                    cpu.state.sp-4,
                    {data, 4}
                )};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::dcrb:
            {
                if (cpu.state.sp < 1)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                )};
                uchar_t stack { static_cast<uchar_t>(
                    cpu.board.ram.Read(cpu.state.sp-1)
                )};

                Error code { cpu.board.ram.Write(
                    cpu.state.sp-1,
                    stack - amount
                )};

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc++;

                return code;
            }

            default:
                return Error::InvalidSpecifier;        
        }

        return System::ErrorCode::Ok;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::DecrementReg(CPU& cpu) noexcept
{
    // register size checks are done at assemble-time
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::dcrri:
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc+1, 4).data
                )};

                reg -= amount;

                cpu.state.pc+=5;

                return System::ErrorCode::Ok;
            }

            case OpCodes::dcrrf:
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                char* data { BytesFromInteger(reg) };
                float regVal { FloatFromBytes(data)}; 
                delete[] data;

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc+1, 4).data      
                )};

                data = BytesFromFloat(regVal - amount);
                reg = IntegerFromBytes<sysbit_t>(data);
                delete[] data;

                cpu.state.pc+=5;
                return System::ErrorCode::Ok;
            }

            case OpCodes::dcrrb:
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc+1)
                )};

                reg -= amount;

                cpu.state.pc+=2;
                return System::ErrorCode::Ok;
            }

            default:
                return Error::InvalidSpecifier;        
        },

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::DecrementSafe(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::dcrsi:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                sysbit_t amount { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data 
                )};

                sysbit_t stack { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                )} ;

                char* data { BytesFromInteger(stack - amount) };
                Error code { cpu.PushSome({
                    data, 
                    4
                })};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::dcrsf:
            {
                if (cpu.state.sp < 4)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                float amount { FloatFromBytes(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )}; 

                float stack { FloatFromBytes(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data      
                )};

                char* data { BytesFromFloat(stack - amount) };
                Error code { cpu.PushSome({
                    data,
                    4
                })};
                delete[] data;

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc+=4;

                return code;
            }

            case OpCodes::dcrsb:
            {
                if (cpu.state.sp < 1)
                {
                    LOGE(
                        System::LogLevel::Medium,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );
                    return Error::Bad;
                }

                uchar_t amount { static_cast<uchar_t>(
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                )};
                uchar_t stack { static_cast<uchar_t>(
                    cpu.board.ram.Read(cpu.state.sp-1)
                )};

                Error code { cpu.Push(
                    stack - amount 
                )};

                if (code == System::ErrorCode::Ok)
                    cpu.state.pc++;

                return code;
            }

            default:
                return Error::InvalidSpecifier;        
        }

        return System::ErrorCode::Ok;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    ) 
}
#undef OPR
