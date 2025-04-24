#include <array>
#include <cstring>
#include <string>
#include <type_traits>

#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/board.hpp"
#include "bytemode/cpu.hpp"
#include "extensions/syntaxextensions.hpp"
#include "system.hpp"

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
            CRASH(
                System::ErrorCode::RAMAccessError,
                "In ", cpu.board.Stringify(), nameof(MemCopy) , " missmatching memory modes and addresses.\n",
                "From Flag: ", MemoryModeFlagsString(from), " From Addr: ", std::to_string(fromAddr),
                "\nTo Flag: ", MemoryModeFlagsString(to), " To Addr: ", std::to_string(toAddr),
                "\n Heap Start: ", std::to_string(cpu.board.ram.StackSize())
            );
        
        if ((toAddr + size) > cpu.board.ram.Size())
            CRASH(
                System::ErrorCode::MemoryOverflow,
                "In ", cpu.board.Stringify(), nameof(MemCopy), " instruction will cause memory overflow.",
                "\nTo Address: ", std::to_string(toAddr), " Size: ", std::to_string(size),
                "\nMemory Size: ", std::to_string(cpu.board.ram.Size())
            );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Increment),
                        "can't increment (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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
                    CRASH(
                        System::ErrorCode::Bad,
                        "In ", cpu.board.Stringify(), nameof(Decrement),
                        "can't decrement (u)int from stack, SP < 4."
                    );

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

#define arr std::array
#define fn std::function<sysbit_t(sysbit_t, sysbit_t)>
OPR CPU::BitLogic(CPU& cpu, arr<OpCodes, 3> op, fn bitwise) noexcept
{
    try_catch(
        OpCodes opc { cpu.board.assembly.Rom().Read(cpu.state.pc-1) };
        if (opc == op.at(0))
        {
            sysbit_t val1 { IntegerFromBytes<sysbit_t>(
                cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data
            )};

            sysbit_t val2 { IntegerFromBytes<sysbit_t>(
                cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
            )}; 
            
            if (Is8BitReg(cpu.board.assembly.Rom().Read(cpu.state.pc)))
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = bitwise(val1, val2);
            }

            cpu.state.pc++; 
            return System::ErrorCode::Ok;
        }
        if (opc == op.at(1))
        {
            uchar_t val1 { 
                static_cast<uchar_t>(cpu.board.ram.Read(cpu.state.sp-2))
            };

            uchar_t val2 {
                static_cast<uchar_t>(cpu.board.ram.Read(cpu.state.sp-1))
            };
        
            if (Is8BitReg(cpu.board.assembly.Rom().Read(cpu.state.pc)))
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = bitwise(val1, val2);
            }

            cpu.state.pc++; 
            return System::ErrorCode::Ok;
        }
        if (opc == op.at(2))
        {
            RegisterModeFlags reg1mode {
                cpu.board.assembly.Rom().Read(cpu.state.pc)
            };

            RegisterModeFlags reg2mode {
                cpu.board.assembly.Rom().Read(cpu.state.pc+1)
            };

            sysbit_t reg1;
            if (Is8BitReg(reg1mode))
                reg1 = static_cast<sysbit_t>(GetRegister8Bit(reg1mode, cpu.state));
            else 
                reg1 = GetRegister32Bit(reg1mode, cpu.state);

            if (Is8BitReg(reg2mode))
                GetRegister8Bit(reg2mode, cpu.state) = 
                    bitwise(GetRegister8Bit(reg2mode, cpu.state), static_cast<uchar_t>(reg1));
            else
                GetRegister32Bit(reg2mode, cpu.state) = 
                    bitwise(GetRegister32Bit(reg2mode, cpu.state), static_cast<uchar_t>(reg1));

            cpu.state.pc+=2; 
            return System::ErrorCode::Ok;
        }
        return System::ErrorCode::InvalidSpecifier;,

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}
#undef arr
#undef fn

OPR CPU::BitAnd(CPU& cpu) noexcept
{
    return BitLogic(
        cpu, 
        {OpCodes::andst, OpCodes::andse, OpCodes::andr},
        [](sysbit_t a, sysbit_t b) -> sysbit_t { return a & b; }
    ); 
}

OPR CPU::BitOr(CPU& cpu) noexcept
{
    return BitLogic(
        cpu, 
        {OpCodes::orst, OpCodes::orse, OpCodes::orr},
        [](sysbit_t a, sysbit_t b) -> sysbit_t { return a | b; }
    );
}

OPR CPU::BitNor(CPU& cpu) noexcept
{
    LOGW("This operation ", nameof(BitNor), " is stupid as hell. Why does it exist?");
    return BitLogic(
        cpu,
        {OpCodes::norst, OpCodes::norse, OpCodes::norr},
        [](sysbit_t a, sysbit_t b) -> sysbit_t { return ~(a | b);}
    );
}

OPR CPU::SwapTop(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::swpt:
            {
                if (cpu.state.sp < 8)
                    CRASH(
                        System::ErrorCode::RAMAccessError,
                        "In ", cpu.board.Stringify(),
                        " can't swap 32-bits on stack, SP < 8"
                    );

                sysbit_t bottom { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data
                )};

                sysbit_t top { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                {
                    char* data { BytesFromInteger(top) };
                    Error err { cpu.board.ram.WriteSome(
                        cpu.state.sp-8,
                        {data, 4} 
                    )};
                    delete[] data;

                    if (err != System::ErrorCode::Ok)
                        return err;
                }

                char* data { BytesFromInteger(bottom) };
                Error err { cpu.board.ram.WriteSome(
                    cpu.state.sp-4,
                    {data, 4}
                )};
                delete[] data;

                return err;
            }

            case OpCodes::swpe:
            {
                if (cpu.state.sp < 2)
                    CRASH(
                        System::ErrorCode::RAMAccessError,
                        "In ", cpu.board.Stringify(),
                        " can't swap 32-bits on stack, SP < 2"
                    );

                char bottom { 
                    cpu.board.ram.Read(cpu.state.sp-2)
                };

                char top { 
                    cpu.board.ram.Read(cpu.state.sp-1)
                };

                {
                    Error err { cpu.board.ram.Write(
                        cpu.state.sp-2,
                        top 
                    )};

                    if (err != System::ErrorCode::Ok)
                        return err;
                }

                Error err { cpu.board.ram.Write(
                    cpu.state.sp-1,
                    bottom
                )};

                return err;
            }

            case OpCodes::swpr:
            { 
                RegisterModeFlags reg1flag {
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                };

                RegisterModeFlags reg2flag {
                    cpu.board.assembly.Rom().Read(cpu.state.pc+1)
                };

                sysbit_t reg1;
                sysbit_t reg2;
                if (Is8BitReg(reg1flag))
                    reg1 = static_cast<sysbit_t>(GetRegister8Bit(reg1flag, cpu.state));
                else
                    reg1 = GetRegister32Bit(reg1flag, cpu.state);
                if (Is8BitReg(reg2flag))
                    reg2 = static_cast<sysbit_t>(GetRegister8Bit(reg2flag, cpu.state));
                else
                    reg2 = GetRegister32Bit(reg2flag, cpu.state);

                if (Is8BitReg(reg1flag))
                    GetRegister8Bit(reg1flag, cpu.state) = static_cast<uchar_t>(reg2);
                else
                    GetRegister32Bit(reg1flag, cpu.state) = reg2;
                if (Is8BitReg(reg2flag))
                    GetRegister8Bit(reg2flag, cpu.state) = static_cast<uchar_t>(reg1);
                else
                    GetRegister32Bit(reg2flag, cpu.state) = reg1;

                return System::ErrorCode::Ok;
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        },

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::DuplicateTop(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::dupt:
            {
                if (cpu.state.sp < 4)
                    CRASH(
                        Error::RAMAccessError,
                        "In ", cpu.board.Stringify(),
                        " can't duplicate 32-bits on stack. SP < 4"
                    );

                Slice data { cpu.board.ram.ReadSome(cpu.state.sp-4, 4) };
                Error code { cpu.PushSome(data) };
                
                return code;
            }

            case OpCodes::dupe:
            {
                if (cpu.state.sp < 1)
                    CRASH(
                        Error::RAMAccessError,
                        "In ", cpu.board.Stringify(),
                        " can't duplicate 8-bits on stack. SP < 1"
                    );

                const char data { cpu.board.ram.Read(cpu.state.sp-1) };
                Error code { cpu.Push(data) };

                return code;
            }
            break;

            default:
                return System::ErrorCode::InvalidSpecifier;
        },

        return exc.GetCode();,
        return Error::UnhandledException;
    );
}

OPR CPU::RawDataStack(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::raw:
            {
                // raw <size> <..data..>
                sysbit_t size { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )};
                cpu.state.pc += 4;

                System::ErrorCode err;
                for (; size > 0; size--)
                {
                    err = cpu.Push(
                        cpu.board.assembly.Rom().Read(cpu.state.pc++)
                    );

                    if (err != System::ErrorCode::Ok)
                        break;
                }

                return err;
            }

            case OpCodes::raws:
            {
                // raw <address> <size> 
                sysbit_t addr { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )};
                
                cpu.state.pc += 4;

                sysbit_t size { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data
                )};

                System::ErrorCode err;
                for (; size > 0; size--)
                {
                    err = cpu.Push(
                        cpu.board.assembly.Rom().Read(addr++)
                    );

                    if (err != System::ErrorCode::Ok)
                        break;
                }

                if (err == System::ErrorCode::Ok)
                    cpu.state.pc += 4;

                return err;
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        }, 

        return exc.GetCode();, 
        return Error::UnhandledException;
    )
}

OPR CPU::Invert(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::invt:
            {
                sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data 
                )};
                System::ErrorCode err { cpu.PopSome(4) };

                if (err != System::ErrorCode::Ok)
                    return err;

                top32 = ~top32;

                char* data { BytesFromInteger(
                    top32
                )};

                err = cpu.PushSome({
                    data,
                    4
                });
                delete[] data;

                return err;
            }

            case OpCodes::inve:
            {
                uchar_t byte { static_cast<uchar_t>(cpu.board.ram.Read(cpu.state.sp-1)) };
                System::ErrorCode err { cpu.Pop() };

                if (err != System::ErrorCode::Ok)
                    return err;

                byte = ~byte;
                err = cpu.Push(byte);
                return err;
            }

            case OpCodes::invr:
            {
                RegisterModeFlags regMode { 
                    cpu.board.assembly.Rom().Read(cpu.state.pc)
                };

                if (Is8BitReg(regMode))
                {
                    uchar_t& reg { GetRegister8Bit(regMode, cpu.state) };
                    reg = ~reg;
                }
                else 
                {
                    sysbit_t& reg { GetRegister32Bit(regMode, cpu.state) };
                    reg = ~reg;
                }

                return System::ErrorCode::Ok;
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        }, 
        
        return exc.GetCode();, 
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::InvertSafe(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::invst:
            {
                sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                    cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data 
                )};

                top32 = ~top32;

                char* data { BytesFromInteger(
                    top32
                )};

                Error err { cpu.PushSome({
                    data,
                    4
                })};
                delete[] data;

                return err;
            }

            case OpCodes::invse:
            {
                uchar_t byte { static_cast<uchar_t>(cpu.board.ram.Read(cpu.state.sp-1)) };

                byte = ~byte;
                Error err { cpu.Push(byte) };
                return err;
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        }, 
        
        return exc.GetCode();, 
        return System::ErrorCode::UnhandledException;
    )
}

template<typename T>
    requires (
        std::is_integral_v<T> || 
        std::is_floating_point_v<T> && 
        !std::is_same_v<double, T>
    )
static bool CompareVarious(T lhs, T rhs, uchar_t mode)
{
    switch (CompareModeFlags(mode))
    {
        case CompareModeFlags::les:
            return lhs < rhs;
        case CompareModeFlags::gre:
            return lhs > rhs;
        case CompareModeFlags::equ:
            return lhs == rhs;
        case CompareModeFlags::leq:
            return lhs <= rhs;
        case CompareModeFlags::geq:
            return lhs >= rhs;
        case CompareModeFlags::neq:
            return lhs != rhs;
    }

    return false;
}

OPR CPU::Compare(CPU& cpu) noexcept
{
    using Numo = NumericModeFlags;

    try_catch(
        const uchar_t compressedModes { static_cast<const uchar_t>(
            cpu.board.assembly.Rom().Read(cpu.state.pc)
        )};
        cpu.state.sp++;
        Numo numMode { 
            static_cast<char>(compressedModes >> 5) 
        };
        const uchar_t compareMode {
            static_cast<const uchar_t>((compressedModes << 3) >> 3)
        };
        uchar_t& bl { GetRegister8Bit(RegisterModeFlags::bl, cpu.state) };

        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::cmp:
            {
                if (numMode == Numo::UInt)
                {
                    sysbit_t int1 { IntegerFromBytes<sysbit_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    sysbit_t int2 { IntegerFromBytes<sysbit_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    bl = CompareVarious(int1, int2, compareMode);
                }
                else if (numMode == Numo::Float)
                {
                    float float1 { FloatFromBytes(
                        cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    float float2 { FloatFromBytes(
                        cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    bl = CompareVarious(float1, float2, compareMode);
                }
                else if (numMode == Numo::Int)
                {
                    int int1 { IntegerFromBytes<int32_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    int int2 { IntegerFromBytes<int32_t>(
                        cpu.board.ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    bl = CompareVarious(int1, int2, compareMode);
                }
                else if (numMode == Numo::UByte)
                {
                    uchar_t byte1 { static_cast<uchar_t>(
                        cpu.board.ram.Read(cpu.state.sp-2)
                    )};
                    uchar_t byte2 { static_cast<uchar_t>(
                        cpu.board.ram.Read(cpu.state.sp-1)
                    )};

                    bl = CompareVarious(byte1, byte2, compareMode);
                }
                else
                {
                    char byte1 { cpu.board.ram.Read(cpu.state.sp-2) };
                    char byte2 { cpu.board.ram.Read(cpu.state.sp-1) };

                    bl = CompareVarious(byte1, byte2, compareMode);
                }
            }

            case OpCodes::cmpr:
            {
                RegisterModeFlags reg1mode {
                    cpu.board.assembly.Rom().Read(cpu.state.pc++)
                };
                RegisterModeFlags reg2mode {
                    cpu.board.assembly.Rom().Read(cpu.state.pc++)
                };

                sysbit_t reg1 { Is8BitReg(reg1mode) ? 
                    GetRegister8Bit(reg1mode, cpu.state) :
                    GetRegister32Bit(reg1mode, cpu.state)
                };
                sysbit_t reg2 { Is8BitReg(reg2mode) ? 
                    GetRegister8Bit(reg2mode, cpu.state) :
                    GetRegister32Bit(reg2mode, cpu.state)
                };

                if (numMode == Numo::UInt)
                    bl = CompareVarious(reg1, reg2, compareMode);
                else if (numMode == Numo::Float)
                    bl = CompareVarious(
                        static_cast<float>(reg1),
                        static_cast<float>(reg2),
                        compareMode
                    );
                else if (numMode == Numo::Int)
                    bl = CompareVarious(
                        static_cast<int>(reg1),
                        static_cast<int>(reg2),
                        compareMode
                    );
                else if (numMode == Numo::UByte)
                    bl = CompareVarious(
                        static_cast<uchar_t>(reg1),
                        static_cast<uchar_t>(reg2),
                        compareMode
                    );
                else
                    bl = CompareVarious(
                        static_cast<char>(reg1),
                        static_cast<char>(reg2),
                        compareMode
                    );
            }

            default:
                return System::ErrorCode::InvalidSpecifier;
        }, 
        
        return exc.GetCode();, 
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::PopInstruction(CPU& cpu) noexcept
{
    try_catch(
        switch (OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::pope:
                return cpu.Pop();
            case OpCodes::popt:
                return cpu.PopSome(4);
            default:
                return System::ErrorCode::InvalidSpecifier;
        }, 
        
        return exc.GetCode();, 
        return System::ErrorCode::UnhandledException;
    )
}

OPR CPU::Jump(CPU& cpu) noexcept
{
    try_catch(
        switch(OpCodes(cpu.board.assembly.Rom().Read(cpu.state.pc-1)))
        {
            case OpCodes::jmpr:
            {
                sysbit_t address { GetRegister32Bit(
                    RegisterModeFlags(cpu.board.assembly.Rom().Read(cpu.state.pc)),
                    cpu.state
                )};

                // Safety test, address must be in bounds of rom
                const sysbit_t romSize { cpu.board.assembly.Rom().Size() };
                if (address >= romSize) // unsigned. no need for negative check
                    return Error::ROMAccessError; 

                cpu.state.pc = address;
                return Error::Ok;
            }

            case OpCodes::jmp:
            {
                sysbit_t address { IntegerFromBytes<sysbit_t>(
                    cpu.board.assembly.Rom().ReadSome(cpu.state.pc, 4).data 
                )};
                
                // Safety test, address must be in bounds of rom
                const sysbit_t romSize { cpu.board.assembly.Rom().Size() };
                if (address >= romSize) // unsigned. no need for negative check
                    return Error::ROMAccessError;

                cpu.state.pc = address;
                return Error::Ok;
            }

            default:
                return Error::InvalidSpecifier;
        },

        return exc.GetCode();,
        return System::ErrorCode::UnhandledException;
    )
}
#undef OPR
