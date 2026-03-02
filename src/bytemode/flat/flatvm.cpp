#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "bytemode/syscall.hpp"
#include "extensions/streamextensions.hpp"
#include "extensions/stringextensions.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/assemblyinfo.hpp"
#include "bytemode/flat/flatram.hpp"
#include "bytemode/flat/flatvm.hpp"
#include "extensions/converters.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/jit.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

#define OPR const System::ErrorCode
#define NOT_IMP(name) \
        LOGE(System::LogLevel::Low, "Implement ", #name); \
        return System::ErrorCode::Ok;

#define Enumc(regn) static_cast<char>(regn)
#define Is8BitReg(reg) (Enumc(reg) >= Enumc(RegisterModeFlags::al)) && (Enumc(reg) <= Enumc(RegisterModeFlags::flg))
#define RomSafetyCheck(addr) \
        if (address < 12 || address > rom.Size()) \
            return System::ErrorCode::ROMAccessError;

#define block(expr) \
        { \
            expr \
            return System::ErrorCode::Ok; \
        }

using Numo = NumericModeFlags;

static sysbit_t& GetRegister32Bit(RegisterModeFlags reg, FlatCPU::State& state)
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
        case RegisterModeFlags::bp: return state.bp;
        default: [[unlikely]]
            CRASH(
                System::ErrorCode::InvalidSpecifier,
                std::to_string((int)reg), " is not a 32bit register."
            );
            return dummy;
    }
}

static uchar_t& GetRegister8Bit(RegisterModeFlags reg, FlatCPU::State& state)
{
    static uchar_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::al: return state.al;
        case RegisterModeFlags::bl: return state.bl;
        case RegisterModeFlags::cl: return state.cl;
        case RegisterModeFlags::dl: return state.dl;
        case RegisterModeFlags::flg: return state.flg;
        default: [[unlikely]]
            CRASH(
                System::ErrorCode::InvalidSpecifier,
                std::to_string((int)reg), " is not an 8bit register."
            );
            return dummy;
    }
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

const System::ErrorCode InitStandardLibrary(SysCallHandler& handler);

void FlatVM::SetUpCommon()
{
#ifdef ENABLE_JIT
    if (settings.jit)
    {
        for (const auto& [symbol, addr] : assembly.Symbols())
            if (rom[addr] <= OpCodesMax)
                blocks.Add(addr);

        jitContext = JITContext {
            .reg32 = {
                &cpu.state.eax,
                &cpu.state.ebx,
                &cpu.state.ecx,
                &cpu.state.edx,
                &cpu.state.esi,
                &cpu.state.edi,
                &cpu.state.pc,
                &cpu.state.sp,
                &cpu.state.bp
            },
            .reg8 = {
                &cpu.state.al,
                &cpu.state.bl,
                &cpu.state.cl,
                &cpu.state.dl,
                &cpu.state.flg
            },
            .ram = &ram,
            .vm = this,
            .IsCompiled = IsCompiled,
            .GetEntry = GetEntry
        };
    }
#endif

    if (InitStandardLibrary(handler) != System::ErrorCode::Ok)
        CRASH(System::ErrorCode::VMError, "Failed to initialize standard library functions.");
}

std::pair<std::unique_ptr<const char[]>, std::streamoff> FlatVM::ReadBytecode(std::istream& bytecode)
{
#ifndef TOOLCHAIN_MODE
    bytecode.seekg(-sizeof(uint64_t), std::ios::end);
    uint64_t size { };
    Extensions::Serialization::DeserializeInteger(size, bytecode);
    bytecode.seekg(-(sizeof(uint64_t)+size), std::ios::end);
    IStreamPos(bytecode, bytecodeEnd, {
        CRASH(System::ErrorCode::FileIOError, "Couldn't load executable ", settings.path.c_str());
    });
    assembly.Deserialize(bytecode);
#else
    bytecode.seekg(0, std::ios::end);
    IStreamPos(bytecode, bytecodeEnd, {
        CRASH(System::ErrorCode::VMError, "Couldn't properly load given bytecode.");
    });
#endif

    if (!(assembly.Flags() & AssemblyFlags::Executable))
        CRASH(System::ErrorCode::UnsupportedFileType,
            "FlatVM does not support given filetype ", settings.path.c_str(), ". It is not marked as an executable.");

#ifdef ENABLE_JIT
    if (!(assembly.Flags() & AssemblyFlags::SymbolInfo) && settings.jit) [[unlikely]]
        LOGW("Current execution is marked as JIT target however the file ", settings.path.c_str(), " does not contain symbol information.");
#endif

    bytecode.seekg(0, std::ios::beg);
    std::unique_ptr<char[]> data { std::make_unique_for_overwrite<char[]>(bytecodeEnd) };
    bytecode.read(data.get(), bytecodeEnd);
    return {rval(data), bytecodeEnd};
}

FlatVM::FlatVM(FlatVM::VMSettings settings) :
    settings(settings),
    ram(),
    rom(),
    cpu(ram),
    handler(),
#ifdef ENABLE_JIT
    blocks(),
    jitContext(),
#endif
    assembly()
{
    if (!std::filesystem::exists(settings.path))
        CRASH(System::ErrorCode::SourceFileNotFound,
            "Couldn't find executable ", settings.path.string());
    std::ifstream bytecode { System::OpenInFile(settings.path) };
    auto [data, size] { ReadBytecode(bytecode) };
    bytecode.close();

    rom = FlatROM { rval(data), static_cast<sysbit_t>(size) };
    cpu.state.pc = IntegerFromBytes<sysbit_t>(rom.ReadSome(0, 4).data);
    ram = FlatRAM {
        IntegerFromBytes<sysbit_t>(rom.ReadSome(4, 4).data),
        IntegerFromBytes<sysbit_t>(rom.ReadSome(8, 4).data)
    };

    SetUpCommon();
}

#ifdef TOOLCHAIN_MODE
FlatVM::FlatVM(FlatVM::VMSettings settings, AssemblyInfo info, std::istream& bytecode) :
    settings(settings),
    ram(),
    rom(),
    cpu(ram),
    handler(),
#ifdef ENABLE_JIT
    blocks(),
    jitContext(),
#endif
    assembly(info)
{
    auto [data, size] { ReadBytecode(bytecode) };

    rom = FlatROM { rval(data), static_cast<sysbit_t>(size) };
    cpu.state.pc = IntegerFromBytes<sysbit_t>(rom.ReadSome(0, 4).data);
    ram = FlatRAM {
        IntegerFromBytes<sysbit_t>(rom.ReadSome(4, 4).data),
        IntegerFromBytes<sysbit_t>(rom.ReadSome(8, 4).data)
    };

    SetUpCommon();
}

FlatVM::FlatVM(FlatVM::VMSettings settings, AssemblyInfo info, char* const buf, const sysbit_t bufsize) :
    settings(settings),
    ram(),
    rom(),
    cpu(ram),
    handler(),
#ifdef ENABLE_JIT
    blocks(),
    jitContext(),
#endif
    assembly(info)
{
    std::stringstream bytecode;
    bytecode.rdbuf()->pubsetbuf(buf, bufsize);
    auto [data, size] { ReadBytecode(bytecode) };

    rom = FlatROM { rval(data), static_cast<sysbit_t>(size) };
    cpu.state.pc = IntegerFromBytes<sysbit_t>(rom.ReadSome(0, 4).data);
    ram = FlatRAM {
        IntegerFromBytes<sysbit_t>(rom.ReadSome(4, 4).data),
        IntegerFromBytes<sysbit_t>(rom.ReadSome(8, 4).data)
    };

    SetUpCommon();
}
#endif

const System::ErrorCode FlatVM::Run() noexcept
{
    startT = std::chrono::steady_clock::now();
    System::ErrorCode errcx { System::ErrorCode::Ok };

    try
    {
        while (cpu.state.pc < rom.Size() && errcx == System::ErrorCode::Ok)
            errcx = Cycle();
    }
    catch (const CSRException& e)
    {
        std::cout << e;
        return e.GetCode();
    }
    catch (const std::exception& e)
    {
        LOGE(System::LogLevel::Medium, "Unhandled exception.\n\t", e.what());
        return System::ErrorCode::UnhandledException;
    }

    return errcx;
}

const System::ErrorCode FlatVM::Cycle() noexcept
{
    System::ErrorCode code = System::ErrorCode::Ok;
    static constexpr void* jumpTable[] = {
        &&op_NoOperation,
        &&op_StoreThirtyTwo, &&op_StoreEight, &&op_StoreFromSymbol, &&op_StoreFromSymbol,
        &&op_LoadFromStack, &&op_LoadFromStack, &&op_ReadFromHeap, &&op_ReadFromHeap, &&op_ReadFromRegister,
        &&op_Move, &&op_Move, &&op_Move,
        &&op_Add32, &&op_AddFloat, &&op_Add8, &&op_AddReg, &&op_AddReg, &&op_AddReg,
        &&op_AddSafe32, &&op_AddSafeFloat, &&op_AddSafe8,
        &&op_MemCopy,
        &&op_Increment, &&op_Increment, &&op_Increment, &&op_IncrementReg, &&op_IncrementReg, &&op_IncrementReg,
        &&op_IncrementSafe, &&op_IncrementSafe, &&op_IncrementSafe,
        &&op_Decrement, &&op_Decrement, &&op_Decrement, &&op_DecrementReg, &&op_DecrementReg, &&op_DecrementReg,
        &&op_DecrementSafe, &&op_DecrementSafe, &&op_DecrementSafe,
        &&op_BitAnd, &&op_BitAnd, &&op_BitAnd,
        &&op_BitOr, &&op_BitOr, &&op_BitOr,
        &&op_BitNor, &&op_BitNor, &&op_BitNor,
        &&op_SwapTop, &&op_SwapTop, &&op_SwapTop,
        &&op_DuplicateTop, &&op_DuplicateTop,
        &&op_RawDataStack, &&op_RawDataStack,
        &&op_Invert, &&op_Invert, &&op_Invert, &&op_InvertSafe, &&op_InvertSafe,
        &&op_Compare, &&op_Compare,
        &&op_PopInstruction, &&op_PopInstruction,
        &&op_Jump, &&op_Jump,
        &&op_SwapRange, &&op_DuplicateRange,
        &&op_Repeat, &&op_Allocate,
        &&op_PowRegister, &&op_PowRegister, &&op_PowRegister,
        &&op_PowStack, &&op_PowStack, &&op_PowStack,
        &&op_PowConst, &&op_PowConst, &&op_PowConst,
        &&op_SqrtConst, &&op_SqrtConst, &&op_SqrtConst,
        &&op_SqrtRegister, &&op_SqrtRegister, &&op_SqrtRegister,
        &&op_SqrtStack, &&op_SqrtStack, &&op_SqrtStack,
        &&op_ConditionalJump, &&op_ConditionalJump,
        &&op_CallFunc, &&op_CallFunc,
        &&op_MulStack, &&op_MulStack, &&op_MulStack,
        &&op_MulRegister, &&op_MulRegister, &&op_MulRegister,
        &&op_MulSafe, &&op_MulSafe, &&op_MulSafe,
        &&op_DivStack, &&op_DivStack, &&op_DivStack,
        &&op_DivRegister, &&op_DivRegister, &&op_DivRegister,
        &&op_DivSafe, &&op_DivSafe, &&op_DivSafe,
        &&op_Return, &&op_Deallocate,
        &&op_Sub32, &&op_SubFloat, &&op_Sub8, &&op_SubReg, &&op_SubReg, &&op_SubReg,
        &&op_SubSafe32, &&op_SubSafeFloat, &&op_SubSafe8,
        &&op_IncrementLocal, &&op_IncrementLocal, &&op_IncrementLocal,
        &&op_ReadLocal, &&op_ReadLocal,
        &&op_CompareJump,
        &&op_CompareLocal,
        // &&op_PushStackFrame,
        &&op_SetFlag,
        &&op_SysCall,
        &&op_BitXor, &&op_BitXor, &&op_BitXor,
    };

    try
    {
#ifndef NDEBUG
        if (this->settings.step)
        {
            int c = std::getchar();
            if (c == 'r')
                this->settings.step = false;
        }
#endif
        uchar_t op;
        System::ErrorCode code = rom.TryRead(cpu.state.pc, op);
        if (code != System::ErrorCode::Ok) [[unlikely]] {
            LOGE(System::LogLevel::Medium, "ROM read error: ", System::ErrorCodeString(code));
            return code;
        }

        if (op >= std::size(jumpTable)) [[unlikely]] {
            LOGE(System::LogLevel::Medium, "Invalid opcode '", std::to_string(op), "' at PC=", std::to_string(cpu.state.pc));
            return System::ErrorCode::InvalidInstruction;
        }

        cpu.state.pc++;
        goto *jumpTable[op];

        op_NoOperation: block()

        op_StoreThirtyTwo: block(
            const System::ErrorCode err { cpu.PushSome(rom.ReadSome(cpu.state.pc, 4)) };
            if (err == System::ErrorCode::Ok)
                cpu.state.pc+=4;
            return err;
        )

        op_StoreEight: block(
            const System::ErrorCode code { cpu.Push(rom.Read(cpu.state.pc)) };
            if (code == System::ErrorCode::Ok)
                cpu.state.pc++;
            return code;
        )

        op_StoreFromSymbol: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (rom.Read(cpu.state.pc-1) == (char)OpCodes::stes ? 1 : 4)
            };

            const Slice symbolData { rom.ReadSome(cpu.state.pc, 4) };
            const sysbit_t symbol { IntegerFromBytes<sysbit_t>(symbolData.data) };
            const Slice valueData { rom.ReadSome(symbol, size) };

            const System::ErrorCode err { cpu.PushSome(valueData) };
            if (err == System::ErrorCode::Ok)
                cpu.state.pc+=4;
            return err;
        )

        op_LoadFromStack: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (rom.Read(cpu.state.pc-1) == (char)OpCodes::ldt ? 4 : 1)
            };

            const Slice values { ram.ReadSome(cpu.state.sp-size, size) };
            // ldc no longer allocates memory itself.
            // it should be allocated and address must be put on &ebx beforehand
            //const sysbit_t alloc { ram.Allocate(size) };

            const System::ErrorCode errc { ram.WriteSome(cpu.state.ebx, values) };
            if (errc != System::ErrorCode::Ok)
                // even though we didn't allocate here, we free in case of an error.
                return ram.Deallocate(cpu.state.ebx, size);
            return errc;

        )

        op_ReadFromHeap: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (rom.Read(cpu.state.pc-1) == (char)OpCodes::rdt ? 4 : 1)
            };

            const Slice values { ram.ReadSome(cpu.state.ebx, size) };

            const System::ErrorCode errc { cpu.PushSome(values) };
            return errc;
        )

        op_ReadFromRegister: block(
            RegisterModeFlags reg { rom.Read(cpu.state.pc) };
            sysbit_t size { Is8BitReg(reg) ? sysbit_t{1} : sysbit_t{4} };
            System::ErrorCode err;

            if (Is8BitReg(reg))
            {
                char data[1];
                BytesFromInteger<uchar_t>(GetRegister8Bit(reg, cpu.state), data);
                err = cpu.PushSome({ data, size });
            }
            else
            {
                char data[4];
                BytesFromInteger<sysbit_t>(GetRegister32Bit(reg, cpu.state), data);
                err = cpu.PushSome({ data, size });
            }

            if (err == System::ErrorCode::Ok)
                cpu.state.pc++;
            return err;
        )

        op_Move: block(
            System::ErrorCode err;
            RegisterModeFlags regFlag { rom.Read(cpu.state.pc) };
            sysbit_t size { Is8BitReg(regFlag) ? sysbit_t{1} : sysbit_t{4} };

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::movc:
                {
                    if (size == 1)
                    {
                        GetRegister8Bit(regFlag, cpu.state) = rom.Read(cpu.state.pc+1);
                        cpu.state.pc+=2;
                    }
                    else
                    {
                        GetRegister32Bit(regFlag, cpu.state) = IntegerFromBytes<sysbit_t>(
                            rom.ReadSome(cpu.state.pc+1, 4).data
                        );
                        cpu.state.pc+=5;
                    }
                    return System::ErrorCode::Ok;
                }

                case OpCodes::movs:
                {
                    if (size == 1)
                        GetRegister8Bit(regFlag, cpu.state) = IntegerFromBytes<uchar_t>(
                            ram.ReadSome(cpu.state.sp-1, 1).data
                        );
                    else
                        GetRegister32Bit(regFlag, cpu.state) = IntegerFromBytes<sysbit_t>(
                            ram.ReadSome(cpu.state.sp-4, 4).data
                        );

                    cpu.state.pc++;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::movr:
                {
                    RegisterModeFlags reg2Flag { rom.Read(cpu.state.pc+1)};
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
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_Add32: block(
            sysbit_t int1;
            sysbit_t int2;
            int1 = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);
            int2 = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromInteger(int1+int2, data);
            return cpu.PushSome({ data, 4 });
        )

        op_AddFloat: block(
            float float1;
            float float2;
            float1 = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);
            float2 = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromFloat<char>(float1+float2, data);
            return cpu.PushSome({ data, 4 });
    )

        op_Add8: block(
            uchar_t byte1;
            uchar_t byte2;
            byte1 = ram.Read(cpu.state.sp-1);
            cpu.Pop();
            byte2 = ram.Read(cpu.state.sp-1);
            cpu.Pop();

            return cpu.Push(byte1+byte2);
        )

        op_AddReg: block(
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags reg1 { rom.Read(cpu.state.pc) };
            RegisterModeFlags reg2 { rom.Read(cpu.state.pc+1) };

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
                    "PC: ", std::to_string(cpu.state.pc-1),
                    " ", OpCodesString(op),
                    " ", std::to_string((int)reg1),
                    " ", std::to_string((int)reg2),
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

                char data[4];
                BytesFromInteger(reg1ref, data);
                float float1 { FloatFromBytes( data)};

                BytesFromInteger(reg2ref, data);
                float float2 { FloatFromBytes( data)};

                BytesFromFloat(float1+float2, data);
                reg2ref = IntegerFromBytes<sysbit_t>( data);
            }
            else
            {
                sysbit_t reg1ref { GetRegister32Bit(reg1, cpu.state) };
                sysbit_t& reg2ref { GetRegister32Bit(reg2, cpu.state) };
                reg2ref += reg1ref;
            }
            return System::ErrorCode::Ok;
        )

        op_AddSafe32: block(
            sysbit_t int1;
            sysbit_t int2;
            int1 = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            int2 = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-8, 4).data);

            char data[4];
            BytesFromInteger(int1+int2, data);
            return cpu.PushSome({ data, 4 });
        )

        op_AddSafeFloat: block(
            float float1;
            float float2;
            float1 = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            float2 = FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data);

            char data[4];
            BytesFromFloat<char>(float1+float2, data);
            return cpu.PushSome({ data, 4 });
        )

        op_AddSafe8: block(
            uchar_t byte1;
            uchar_t byte2;
            byte1 = ram.Read(cpu.state.sp-1);
            byte2 = ram.Read(cpu.state.sp-2);

            return cpu.Push(byte1+byte2);
        )

        op_MemCopy: block(
            uchar_t compressedModes { static_cast<uchar_t>(rom.Read(cpu.state.pc)) };
            MemoryModeFlags from { MemoryModeFlags(compressedModes & 0xF0) };
            MemoryModeFlags to { MemoryModeFlags(compressedModes & 0x0F) };

            sysbit_t fromAddr { GetRegister32Bit(RegisterModeFlags::eax, cpu.state) };
            sysbit_t toAddr { GetRegister32Bit(RegisterModeFlags::ebx, cpu.state) };
            sysbit_t size { GetRegister32Bit(RegisterModeFlags::ecx, cpu.state) };

            if ((toAddr + size) > ram.Size())
                CRASH(
                    System::ErrorCode::MemoryOverflow,
                    "In ", nameof(MemCopy), " instruction will cause memory overflow.",
                    "\nTo Address: ", std::to_string(toAddr), " Size: ", std::to_string(size),
                    "\nMemory Size: ", std::to_string(ram.Size())
                );

            Slice dataToCopy { ram.ReadSome(fromAddr, size) };
            const System::ErrorCode code { ram.WriteSome(toAddr, dataToCopy) };

            if (code == System::ErrorCode::Ok)
                cpu.state.pc++;
            return code;
        )

        op_Increment: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::inci:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromInteger(stack+amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, {data, 4})};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::incf:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    float stack { FloatFromBytes(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromFloat(amount+stack, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, {data, 4})};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::incb:
                {
                    if (cpu.state.sp < 1)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc)
                    )};
                    uchar_t stack { static_cast<uchar_t>(
                        ram.Read(cpu.state.sp-1)
                    )};

                    const System::ErrorCode code { ram.Write(
                        cpu.state.sp-1,
                        amount + stack
                    )};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc++;
                    return code;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_IncrementReg: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::incri:
                {
                    sysbit_t& reg { GetRegister32Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc+1, 4).data
                    )};

                    reg += amount;

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::incrf:
                {
                    sysbit_t& reg { GetRegister32Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    char data[4];
                    BytesFromInteger(reg, data);
                    float regVal { FloatFromBytes(data)};

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc+1, 4).data
                    )};

                    BytesFromFloat(regVal+amount, data);
                    reg = IntegerFromBytes<sysbit_t>(data);

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::incrb:
                {
                    uchar_t& reg { GetRegister8Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc+1)
                    )};

                    reg += amount;

                    cpu.state.pc+=2;
                    return System::ErrorCode::Ok;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_IncrementSafe: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::incsi:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromInteger(stack+amount, data);
                    const System::ErrorCode code { cpu.PushSome({ data, 4 })};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::incsf:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    float stack { FloatFromBytes(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromFloat(amount+stack, data);
                    const System::ErrorCode code { cpu.PushSome({ data, 4 })};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::incsb:
                {
                    if (cpu.state.sp < 1)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc)
                    )};
                    uchar_t stack { static_cast<uchar_t>(
                        ram.Read(cpu.state.sp-1)
                    )};

                    const System::ErrorCode code { cpu.Push(
                        amount + stack
                    )};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc++;
                    return code;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Decrement: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::dcri:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromInteger(stack - amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, {data, 4})};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::dcrf:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement float from stack, SP < 4."
                        );

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    float stack { FloatFromBytes(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromFloat(stack - amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, {data, 4})};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::dcrb:
                {
                    if (cpu.state.sp < 1)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement (u)byte from stack, SP < 4."
                        );

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc)
                    )};
                    uchar_t stack { static_cast<uchar_t>(
                        ram.Read(cpu.state.sp-1)
                    )};

                    const System::ErrorCode code { ram.Write(
                        cpu.state.sp-1,
                        stack - amount
                    )};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc++;
                    return code;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DecrementReg: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::dcrri:
                {
                    sysbit_t& reg { GetRegister32Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc+1, 4).data
                    )};

                    reg -= amount;

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::dcrrf:
                {
                    sysbit_t& reg { GetRegister32Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    char data[4];
                    BytesFromInteger(reg, data);
                    float regVal { FloatFromBytes(data)};

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc+1, 4).data
                    )};

                    BytesFromFloat(regVal - amount, data);
                    reg = IntegerFromBytes<sysbit_t>(data);

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::dcrrb:
                {
                    uchar_t& reg { GetRegister8Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc+1)
                    )};

                    reg -= amount;

                    cpu.state.pc+=2;
                    return System::ErrorCode::Ok;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_DecrementSafe: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::dcrsi:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );

                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )} ;

                    char data[4];
                    BytesFromInteger(stack - amount, data);
                    const System::ErrorCode code { cpu.PushSome({ data, 4 })};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::dcrsf:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );

                    float amount { FloatFromBytes(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};

                    float stack { FloatFromBytes(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    char data[4];
                    BytesFromFloat(stack - amount, data);
                    const System::ErrorCode code { cpu.PushSome({ data, 4 })};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc+=4;
                    return code;
                }

                case OpCodes::dcrsb:
                {
                    if (cpu.state.sp < 1)
                        CRASH(
                            System::ErrorCode::Bad,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );

                    uchar_t amount { static_cast<uchar_t>(
                        rom.Read(cpu.state.pc)
                    )};
                    uchar_t stack { static_cast<uchar_t>(
                        ram.Read(cpu.state.sp-1)
                    )};

                    const System::ErrorCode code { cpu.Push(
                        stack - amount
                    )};

                    if (code == System::ErrorCode::Ok)
                        cpu.state.pc++;
                    return code;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_BitAnd: block(
            return BitLogic(
                {OpCodes::andst, OpCodes::andse, OpCodes::andr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a & b; }
            );
        )

        op_BitOr: block(
            return BitLogic(
                {OpCodes::orst, OpCodes::orse, OpCodes::orr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a | b; }
            );
        )

        op_BitNor: block(
            LOGW("This operation ", nameof(BitNor), " is stupid as hell. Why does it exist?");
            return BitLogic(
                {OpCodes::norst, OpCodes::norse, OpCodes::norr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return ~(a | b);}
            );
        )

        op_SwapTop: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::swpt:
                {
                    if (cpu.state.sp < 8)
                        CRASH(
                            System::ErrorCode::RAMAccessError,
                            "Can't swap 32-bits on stack, SP < 8"
                        );

                    sysbit_t bottom { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    )};

                    sysbit_t top { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    {
                        char data[4];
                        BytesFromInteger(top, data);
                        const System::ErrorCode err { ram.WriteSome( cpu.state.sp-8, {data, 4} )};

                        if (err != System::ErrorCode::Ok)
                            return err;
                    }

                    char data[4];
                    BytesFromInteger(bottom, data);
                    return ram.WriteSome( cpu.state.sp-4, {data, 4});
                }

                case OpCodes::swpe:
                {
                    if (cpu.state.sp < 2)
                        CRASH(
                            System::ErrorCode::RAMAccessError,
                            "Can't swap 8-bits on stack, SP < 2"
                        );

                    char bottom {
                        ram.Read(cpu.state.sp-2)
                    };

                    char top {
                        ram.Read(cpu.state.sp-1)
                    };

                    {
                        const System::ErrorCode err { ram.Write(
                            cpu.state.sp-2,
                            top
                        )};

                        if (err != System::ErrorCode::Ok)
                            return err;
                    }

                    return ram.Write(
                        cpu.state.sp-1,
                        bottom
                    );
                }

                case OpCodes::swpr:
                {
                    RegisterModeFlags reg1flag {
                        rom.Read(cpu.state.pc)
                    };

                    RegisterModeFlags reg2flag {
                        rom.Read(cpu.state.pc+1)
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
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DuplicateTop: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::dupt:
                {
                    if (cpu.state.sp < 4)
                        CRASH(
                            System::ErrorCode::RAMAccessError,
                            "Can't duplicate 32-bits on stack. SP < 4"
                        );

                    Slice data { ram.ReadSome(cpu.state.sp-4, 4) };
                    return cpu.PushSome(data);
                }

                case OpCodes::dupe:
                {
                    if (cpu.state.sp < 1)
                        CRASH(
                            System::ErrorCode::RAMAccessError,
                            "Can't duplicate 8-bits on stack. SP < 1"
                        );

                    const char data { ram.Read(cpu.state.sp-1) };
                    return cpu.Push(data);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_RawDataStack: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::raw:
                {
                    // raw <size> <..data..>
                    sysbit_t size { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};
                    cpu.state.pc += 4;

                    System::ErrorCode err;
                    for (; size > 0; size--)
                    {
                        err = cpu.Push(
                            rom.Read(cpu.state.pc++)
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
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};
                    cpu.state.pc += 4;

                    sysbit_t size { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};
                    cpu.state.pc += 4;

                    return cpu.PushSome(
                        rom.ReadSome(addr, size)
                    );
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Invert: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::invt:
                {
                    sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};
                    System::ErrorCode err { cpu.PopSome(4) };

                    if (err != System::ErrorCode::Ok)
                        return err;

                    top32 = ~top32;

                    char data[4];
                    BytesFromInteger(top32, data);
                    return cpu.PushSome({ data, 4 });
                }

                case OpCodes::inve:
                {
                    uchar_t byte { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };
                    System::ErrorCode err { cpu.Pop() };

                    if (err != System::ErrorCode::Ok)
                        return err;

                    byte = ~byte;
                    return cpu.Push(byte);
                }

                case OpCodes::invr:
                {
                    RegisterModeFlags regMode {
                        rom.Read(cpu.state.pc)
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
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_InvertSafe: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::invst:
                {
                    sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    top32 = ~top32;

                    char data[4];
                    BytesFromInteger(top32, data);
                    return cpu.PushSome({ data, 4 });
                }

                case OpCodes::invse:
                {
                    uchar_t byte { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };

                    byte = ~byte;
                    return cpu.Push(byte);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Compare: block(
            const uchar_t compressedModes { static_cast<const uchar_t>(
                rom.Read(cpu.state.pc)
            )};
            cpu.state.pc++;

            Numo numMode {
                static_cast<uchar_t>(compressedModes >> 5)
            };
            const uchar_t compareMode {
                static_cast<const uchar_t>(compressedModes & 0x1F)
            };

            switch (OpCodes(rom.Read(cpu.state.pc-2)))
            {
                case OpCodes::cmp:
                {
                    if (numMode == Numo::UInt)
                    {
                        sysbit_t int1 { IntegerFromBytes<sysbit_t>(
                            ram.ReadSome(cpu.state.sp-8, 4).data
                        )};
                        sysbit_t int2 { IntegerFromBytes<sysbit_t>(
                            ram.ReadSome(cpu.state.sp-4, 4).data
                        )};
                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(int1, int2, compareMode);
                    }
                    else if (numMode == Numo::Float)
                    {
                        float float1 { FloatFromBytes(
                            ram.ReadSome(cpu.state.sp-8, 4).data
                        )};
                        float float2 { FloatFromBytes(
                            ram.ReadSome(cpu.state.sp-4, 4).data
                        )};

                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(float1, float2, compareMode);
                    }
                    else if (numMode == Numo::Int)
                    {
                        int int1 { IntegerFromBytes<int32_t>(
                            ram.ReadSome(cpu.state.sp-8, 4).data
                        )};
                        int int2 { IntegerFromBytes<int32_t>(
                            ram.ReadSome(cpu.state.sp-4, 4).data
                        )};

                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(int1, int2, compareMode);
                    }
                    else if (numMode == Numo::UByte)
                    {
                        uchar_t byte1 { static_cast<uchar_t>(
                            ram.Read(cpu.state.sp-2)
                        )};
                        uchar_t byte2 { static_cast<uchar_t>(
                            ram.Read(cpu.state.sp-1)
                        )};

                        cpu.state.sp -= 2;
                        cpu.state.bl = CompareVarious(byte1, byte2, compareMode);
                    }
                    else
                    {
                        char byte1 { ram.Read(cpu.state.sp-2) };
                        char byte2 { ram.Read(cpu.state.sp-1) };

                        cpu.state.sp -= 2;
                        cpu.state.bl = CompareVarious(byte1, byte2, compareMode);
                    }
                    return System::ErrorCode::Ok;
                }

                case OpCodes::cmpr:
                {
                    RegisterModeFlags reg1mode {
                        rom.Read(cpu.state.pc++)
                    };
                    RegisterModeFlags reg2mode {
                        rom.Read(cpu.state.pc++)
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
                        cpu.state.bl = CompareVarious(reg1, reg2, compareMode);
                    else if (numMode == Numo::Float)
                        cpu.state.bl = CompareVarious(
                            static_cast<float>(reg1),
                            static_cast<float>(reg2),
                            compareMode
                        );
                    else if (numMode == Numo::Int)
                        cpu.state.bl = CompareVarious(
                            static_cast<int>(reg1),
                            static_cast<int>(reg2),
                            compareMode
                        );
                    else if (numMode == Numo::UByte)
                        cpu.state.bl = CompareVarious(
                            static_cast<uchar_t>(reg1),
                            static_cast<uchar_t>(reg2),
                            compareMode
                        );
                    else
                        cpu.state.bl = CompareVarious(
                            static_cast<char>(reg1),
                            static_cast<char>(reg2),
                            compareMode
                        );
                    return System::ErrorCode::Ok;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PopInstruction: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::pope:
                    return cpu.Pop();
                case OpCodes::popt:
                    return cpu.PopSome(4);
                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Jump: block(
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::jmpr:
                {
                    sysbit_t address { GetRegister32Bit(
                        RegisterModeFlags(rom.Read(cpu.state.pc)),
                        cpu.state
                    )};


#ifdef ENABLE_JIT
                    JITError err { BranchIncrease(blocks, address, &jitContext, rom, settings.jit) };

                    if (err == JITError::Ok)
                        return System::ErrorCode::Ok;
                    else
                    {
#endif
                        // Safety test, address must be in bounds of rom
                        RomSafetyCheck(address);
                        cpu.state.pc = address;
                        return System::ErrorCode::Ok;
#ifdef ENABLE_JIT
                    }
#endif
                }

                case OpCodes::jmp:
                {
                    sysbit_t address { IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    )};


#ifdef ENABLE_JIT
                    JITError err { BranchIncrease(blocks, address, &jitContext, rom, settings.jit) };

                    if (err == JITError::Ok)
                        return System::ErrorCode::Ok;
                    else
                    {
#endif
                        // Safety test, address must be in bounds of rom
                        RomSafetyCheck(address);
                        cpu.state.pc = address;
                        return System::ErrorCode::Ok;
#ifdef ENABLE_JIT
                    }
#endif
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SwapRange: block(
            // swr <size: sysbit>
            sysbit_t size { IntegerFromBytes<sysbit_t>(
                rom.ReadSome(cpu.state.pc, 4).data
            )};

            System::ErrorCode err { System::ErrorCode::Ok };
            for (sysbit_t midpoint = cpu.state.sp-size; size > 0; size--)
            {
                char tmp { ram.Read(cpu.state.sp-size) };
                err = err == System::ErrorCode::Ok ? ram.Write(
                    cpu.state.sp-size,
                    ram.Read(midpoint-size)
                ) : err;
                err = err == System::ErrorCode::Ok ?
                    ram.Write(midpoint-size, tmp)
                    : err;

                if (err != System::ErrorCode::Ok)
                    return err;
            }

            cpu.state.pc += 4;
        )

    op_DuplicateRange: block(
            // dur <size: sysbit>
            sysbit_t size { IntegerFromBytes<sysbit_t>(
                rom.ReadSome(cpu.state.pc, 4).data
            )};

            System::ErrorCode err { System::ErrorCode::Ok };
            cpu.PushSome(
                ram.ReadSome(cpu.state.sp-size, size)
            );

            cpu.state.pc += 4;
        )

        op_Repeat: block(
            // rep <compressed(mem/num)> <count> <val>
            MemoryModeFlags memMode;
            NumericModeFlags numMode;
            const uchar_t compressed { static_cast<uchar_t>(
                rom.Read(cpu.state.pc)
            )};

            memMode = MemoryModeFlags(compressed >> 4);
            numMode = NumericModeFlags(compressed & 0b00001111);

            cpu.state.pc++;
            const sysbit_t count { IntegerFromBytes<sysbit_t>(
                rom.ReadSome(cpu.state.pc, 4).data
            )};
            cpu.state.pc += 4;

            const Slice valueData {
                rom.ReadSome(cpu.state.pc, ByteSize(numMode))
            };
            cpu.state.pc += ByteSize(numMode);

            sysbit_t address;
            if (memMode == MemoryModeFlags::Heap)
                address = cpu.state.ebx;
            else
            {
                if (cpu.state.sp + count*ByteSize(numMode) > ram.StackSize())
                    CRASH(
                        System::ErrorCode::StackOverflow,
                        "In "" instruction rep. Can't push onto stack, it's full"
                    );
                address = cpu.state.sp;
                cpu.state.sp += count*ByteSize(numMode);
            }

            System::ErrorCode err { System::ErrorCode::Ok };
            for (sysbit_t i = 0; (i < count) && (err == System::ErrorCode::Ok); i++, address += ByteSize(numMode))
                err = err == System::ErrorCode::Ok ?
                    ram.WriteSome(address, valueData) :
                    err;
            return err;
        )

        op_Allocate: block(
            const sysbit_t address { ram.Allocate(cpu.state.ecx) };
            cpu.state.ebx = address;
        )

        op_PowRegister: block(
            float base;
            float power;
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags reg1 { rom.Read(cpu.state.pc++)};
            RegisterModeFlags reg2 { rom.Read(cpu.state.pc++)};

            switch (op)
            {
                case OpCodes::powri:
                {
                    base = static_cast<float>(GetRegister32Bit(reg1, cpu.state));
                    power = static_cast<float>(GetRegister32Bit(reg2, cpu.state));
                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };
                    GetRegister32Bit(reg2, cpu.state) = res;
                    break;
                }

                case OpCodes::powrf:
                {
                    char data[4];
                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg1, cpu.state), data);
                    base = FloatFromBytes(data);

                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg2, cpu.state), data);
                    power = FloatFromBytes(data);

                    float res { std::pow(base, power) };
                    BytesFromFloat(res, data);
                    GetRegister32Bit(reg2, cpu.state) = IntegerFromBytes<sysbit_t>(data);
                    break;
                }

                case OpCodes::powrb:
                {
                    base = static_cast<float>(GetRegister8Bit(reg1, cpu.state));
                    power = static_cast<float>(GetRegister8Bit(reg2, cpu.state));
                    uchar_t res { static_cast<uchar_t>(std::pow(base, power)) };
                    GetRegister8Bit(reg2, cpu.state) = res;
                    break;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PowStack: block(
            float base;
            float power;
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::powsi:
                {
                    base = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    ));
                    power = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    ));

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                    break;
                }

                case OpCodes::powsf:
                {
                    base = FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data);
                    power = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    float res { std::pow(base, power) };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::powsb:
                {
                    base = static_cast<float>(ram.Read(cpu.state.sp-2));
                    power = static_cast<float>(ram.Read(cpu.state.sp-1));

                    err = cpu.PopSome(2);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    uchar_t res { static_cast<uchar_t>(std::pow(base, power)) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PowConst: block(
            float base;
            float power;
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::powi:
                {
                    base = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    ));
                    power = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc+4, 4).data
                    ));
                    cpu.state.pc += 8;

                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::powf:
                {
                    base = FloatFromBytes(rom.ReadSome(cpu.state.pc, 4).data);
                    power = FloatFromBytes(rom.ReadSome(cpu.state.pc+4, 4).data);
                    cpu.state.pc += 8;

                    float res { std::pow(base, power) };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::powb:
                {
                    base = static_cast<float>(rom.Read(cpu.state.pc));
                    power = static_cast<float>(rom.Read(cpu.state.pc+1));
                    cpu.state.pc += 2;

                    uchar_t res { static_cast<uchar_t>(std::pow(base, power)) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtConst: block(
            float num;
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::sqri:
                {
                    num = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        rom.ReadSome(cpu.state.pc, 4).data
                    ));
                    cpu.state.pc += 4;

                    sysbit_t res { static_cast<sysbit_t>(std::sqrt(num)) };
                    char data[4];
                    BytesFromInteger(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::sqrf:
                {
                    num = FloatFromBytes(
                        rom.ReadSome(cpu.state.pc, 4).data
                    );
                    cpu.state.pc += 4;

                    float res { std::sqrt(num) };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::sqrb:
                {
                    num = static_cast<float>(rom.Read(cpu.state.pc));
                    cpu.state.pc++;

                    uchar_t res { static_cast<uchar_t>(std::sqrt(num)) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtRegister: block(
            float num;
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags reg { rom.Read(cpu.state.pc++)};

            switch (op)
            {
                case OpCodes::sqrri:
                {
                    num = static_cast<float>(GetRegister32Bit(reg, cpu.state));
                    sysbit_t res { static_cast<sysbit_t>(std::sqrt(num)) };
                    cpu.state.eax = res;
                    break;
                }

                case OpCodes::sqrrf:
                {
                    char data[4];

                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg, cpu.state), data);
                    num = FloatFromBytes(data);

                    float res { std::sqrt(num) };
                    BytesFromFloat(res, data);
                    cpu.state.eax = IntegerFromBytes<sysbit_t>(data);
                    break;
                }

                case OpCodes::sqrrb:
                {
                    num = static_cast<float>(GetRegister8Bit(reg, cpu.state));
                    uchar_t res { static_cast<uchar_t>(std::sqrt(num)) };
                    cpu.state.al = res;
                    break;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtStack: block(
            float num;
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::sqrsi:
                {
                    num = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    ));

                    err = cpu.PopSome(4);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    sysbit_t res { static_cast<sysbit_t>(std::sqrt(num)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::sqrsf:
                {
                    num = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);

                    err = cpu.PopSome(4);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    float res { std::sqrt(num) };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::sqrsb:
                {
                    num = static_cast<float>(ram.Read(cpu.state.sp-1));

                    err = cpu.PopSome(1);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    uchar_t res { static_cast<uchar_t>(std::sqrt(num)) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_ConditionalJump: block(
            OpCodes op { OpCodes(rom.Read(cpu.state.pc-1)) };
            sysbit_t address;

            if (cpu.state.bl == 0)
                address = cpu.state.pc + ((op == OpCodes::cnd) ? 4 : 1);
            else if (op == OpCodes::cnd)
                address = IntegerFromBytes<sysbit_t>(
                    rom.ReadSome(cpu.state.pc, 4).data
                );
            else if (op == OpCodes::cndr)
                address = GetRegister32Bit(
                    RegisterModeFlags(rom.Read(cpu.state.pc)),
                    cpu.state
                );
            else
                return System::ErrorCode::InvalidInstruction;



#ifdef ENABLE_JIT
            JITError err { BranchIncrease(blocks, address, &jitContext, rom, settings.jit) };

            if (err == JITError::Ok)
                return System::ErrorCode::Ok;
            else
            {
#endif
                // Safety test, address must be in bounds of rom
                RomSafetyCheck(address);
                cpu.state.pc = address;
                return System::ErrorCode::Ok;
#ifdef ENABLE_JIT
            }
#endif
        )


        op_CallFunc: block(
            if (cpu.state.sp < cpu.state.bl)
                return System::ErrorCode::RAMAccessError;

            OpCodes op { OpCodes(rom.Read(cpu.state.pc-1)) };
            sysbit_t address;
            if (op == OpCodes::cal)
                address = IntegerFromBytes<sysbit_t>(
                    rom.ReadSome(cpu.state.pc, 4).data
                );
            if (op == OpCodes::calr)
                address = GetRegister32Bit(
                    RegisterModeFlags(
                        rom.Read(cpu.state.pc)
                    ),
                    cpu.state
                );

            // normal call
            // Copy params beforehand
            // Create callstack
            //  - Store bp
            //  - Store pc
            //  - Change bp

#ifdef ENABLE_JIT
            JITError jiterr { BranchIncrease(blocks, address, &jitContext, rom, settings.jit) };

            if (jiterr == JITError::Ok)
                return System::ErrorCode::Ok;
#endif

            if (cpu.state.sp+8+cpu.state.bl > ram.StackSize())
                CRASH(System::ErrorCode::StackOverflow, "Can't push parameters.");

            //const Slice params { ram.ReadSome(cpu.state.sp-cpu.state.bl, cpu.state.bl) };
            //Slice params (&ram+cpu.state.sp-cpu.state.bl, cpu.state.bl);
            //cpu.state.sp += 8 - cpu.state.bl;
            //ram.WriteSome(cpu.state.sp, params);
            //cpu.state.sp -= cpu.state.bl + 8;
            std::memmove(&ram+cpu.state.sp+8, &ram+cpu.state.sp-cpu.state.bl, cpu.state.bl);

            // Store bp
            char data[4];
            BytesFromInteger(cpu.state.bp, data);
            System::ErrorCode err = cpu.PushSome({data, 4});

            if (err != System::ErrorCode::Ok)
                return err;

            // Store pc
            BytesFromInteger(cpu.state.pc + (op == OpCodes::cal ? 4 : 1), data);
            err = cpu.PushSome({data, 4});

            if (err != System::ErrorCode::Ok)
                return err;

            // Change pc and bp
            cpu.state.pc = address;
            cpu.state.bp = cpu.state.sp;

            // Copy params
    //        System::ErrorCode err;
    //        Slice params(cpu.paramBuf.get(), cpu.state.bl);
    //        err = ram.WriteSome(cpu.state.sp, params);
    //        if (err != System::ErrorCode::Ok)
    //            errcx = err;
    //            break;
            cpu.state.sp += cpu.state.bl;
        )

        op_MulStack: block(
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::muli:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    sysbit_t res { lhs * rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::mulf:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    float lhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data) };
                    float rhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data) };

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    float res { lhs * rhs };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::mulb:
                {
                    uchar_t lhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-2)) };
                    uchar_t rhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };

                    err = cpu.PopSome(2);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    uchar_t res { static_cast<uchar_t>(lhs * rhs) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_MulRegister: block(
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags reg1 { rom.Read(cpu.state.pc++)};
            RegisterModeFlags reg2 { rom.Read(cpu.state.pc++)};

            switch (op)
            {
                case OpCodes::mulri:
                {
                    sysbit_t lhs { GetRegister32Bit(reg1, cpu.state) };
                    sysbit_t& rhs { GetRegister32Bit(reg2, cpu.state) };
                    rhs *= lhs;
                    break;
                }

                case OpCodes::mulrf:
                {
                    char data[4];
                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg1, cpu.state), data);
                    float lhs { FloatFromBytes(data) };

                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg2, cpu.state), data);
                    float rhs { FloatFromBytes(data) };

                    BytesFromFloat(lhs * rhs, data);
                    GetRegister32Bit(reg2, cpu.state) = IntegerFromBytes<sysbit_t>(data);

                    break;
                }

                case OpCodes::mulrb:
                {
                    uchar_t lhs { GetRegister8Bit(reg1, cpu.state) };
                    uchar_t& rhs { GetRegister8Bit(reg2, cpu.state) };
                    rhs *= lhs;
                    break;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_MulSafe: block(
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::mulsi:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    sysbit_t res { lhs * rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::mulsf:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    float lhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data) };
                    float rhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data) };

                    float res { lhs * rhs };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::mulsb:
                {
                    uchar_t lhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-2)) };
                    uchar_t rhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };

                    uchar_t res { static_cast<uchar_t>(lhs * rhs) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivStack: block(
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::divi:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    sysbit_t res { lhs / rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::divf:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    float lhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data) };
                    float rhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(8);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    float res { lhs / rhs };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::divb:
                {
                    uchar_t lhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-2)) };
                    uchar_t rhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(2);
                    if (err != System::ErrorCode::Ok)
                        return err;

                    uchar_t res { static_cast<uchar_t>(lhs / rhs) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivRegister: block(
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags reg1 { rom.Read(cpu.state.pc++)};
            RegisterModeFlags reg2 { rom.Read(cpu.state.pc++)};

            switch (op)
            {
                case OpCodes::divri:
                {
                    sysbit_t lhs { GetRegister32Bit(reg1, cpu.state) };
                    sysbit_t& rhs { GetRegister32Bit(reg2, cpu.state) };
                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;
                    rhs = lhs / rhs;
                    break;
                }

                case OpCodes::divrf:
                {
                    char data[4];
                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg1, cpu.state), data);
                    float lhs { FloatFromBytes(data) };

                    BytesFromInteger<sysbit_t>(GetRegister32Bit(reg2, cpu.state), data);
                    float rhs { FloatFromBytes(data) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    BytesFromFloat(lhs / rhs, data);
                    GetRegister32Bit(reg2, cpu.state) = IntegerFromBytes<sysbit_t>(data);

                    break;
                }

                case OpCodes::divrb:
                {
                    uchar_t lhs { GetRegister8Bit(reg1, cpu.state) };
                    uchar_t& rhs { GetRegister8Bit(reg2, cpu.state) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    rhs = lhs / rhs;
                    break;
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivSafe: block(
            System::ErrorCode err;

            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::divsi:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-8, 4).data
                    )};
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        ram.ReadSome(cpu.state.sp-4, 4).data
                    )};

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    sysbit_t res { lhs / rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::divsf:
                {
                    if (cpu.state.sp < 4)
                        return System::ErrorCode::RAMAccessError;

                    float lhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data) };
                    float rhs { FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    float res { lhs / rhs };
                    char data[4];
                    BytesFromFloat(res, data);
                    return cpu.PushSome({data, 4});
                }

                case OpCodes::divsb:
                {
                    uchar_t lhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-2)) };
                    uchar_t rhs { static_cast<uchar_t>(ram.Read(cpu.state.sp-1)) };

                    if (rhs == 0)
                        return System::ErrorCode::DivideByZero;

                    uchar_t res { static_cast<uchar_t>(lhs / rhs) };
                    return cpu.Push(res);
                }

                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Return: block(
            // callstack is:
            //  bp 4bytes
            //  pc 4bytes
            // current bp is AFTER the callstack

            if (cpu.state.sp - cpu.state.bp < cpu.state.bl)
                return System::ErrorCode::StackUnderflow;

            sysbit_t bpToReturnTo { IntegerFromBytes<sysbit_t>(
                ram.ReadSome(cpu.state.bp - 8, 4).data
            )};
            sysbit_t pcToReturnTo { IntegerFromBytes<sysbit_t>(
                ram.ReadSome(cpu.state.bp - 4, 4).data
            )};


            System::ErrorCode err;
            
            if (cpu.state.bl != 0)
            {
                Slice returnValues { ram.ReadSome(
                    cpu.state.sp - cpu.state.bl,
                    cpu.state.bl
                )};
                err = cpu.PopSome(cpu.state.sp - cpu.state.bp + 8);
                if (err != System::ErrorCode::Ok)
                    return err;
                err = cpu.PushSome(returnValues);
            }
            else
                err = cpu.PopSome(cpu.state.sp - cpu.state.bp + 8);

            cpu.state.bp = bpToReturnTo;
            cpu.state.pc = pcToReturnTo;
            
            return err;
        )

        op_Deallocate: block(
            if (cpu.state.ebx < 0 || ram.Size() <= cpu.state.ebx)
                return System::ErrorCode::RAMAccessError;

            return ram.Deallocate(cpu.state.ebx, cpu.state.ecx);
        )

        op_Sub32: block(
            sysbit_t rhs;
            sysbit_t lhs;
            rhs = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);
            lhs = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromInteger(lhs-rhs, data);
            return cpu.PushSome({data, 4 });
        )

        op_SubFloat: block(
            float rhs;
            float lhs;
            rhs = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);
            lhs = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromFloat<char>(lhs-rhs, data);
            return cpu.PushSome({data, 4 });
        )

        op_Sub8: block(
            uchar_t rhs;
            uchar_t lhs;
            rhs = ram.Read(cpu.state.sp-1);
            cpu.Pop();
            lhs = ram.Read(cpu.state.sp-1);
            cpu.Pop();

            return cpu.Push(lhs-rhs);
        )

        op_SubReg: block(
            OpCodes op { rom.Read(cpu.state.pc-1) };
            RegisterModeFlags regLhs { rom.Read(cpu.state.pc) };
            RegisterModeFlags regRhs { rom.Read(cpu.state.pc+1) };

            if (
                /*case 1*/
                ((op == OpCodes::subrb)
                &&
                (!Is8BitReg(regLhs) || !Is8BitReg(regRhs)))
                ||

                /*case 2*/
                ((op != OpCodes::subrb)
                &&
                (Is8BitReg(regLhs) || Is8BitReg(regRhs)))
            )
                CRASH(System::ErrorCode::InvalidSpecifier,
                    "PC: ", std::to_string(cpu.state.pc-1),
                    " ", OpCodesString(op),
                    " ", std::to_string((int)regLhs),
                    " ", std::to_string((int)regRhs),
                    " Given registers are not compatible with given numeric type."
                );

            cpu.state.pc+=2;

            if (Is8BitReg(regLhs))
            {
                uchar_t regLhsRef { GetRegister8Bit(regLhs, cpu.state) };
                uchar_t& regRhsRef { GetRegister8Bit(regRhs, cpu.state) };
                regRhsRef = regLhsRef - regRhsRef;
            }
            else if (op == OpCodes::subrf)
            {
                sysbit_t regLhsRef { GetRegister32Bit(regLhs, cpu.state) };
                sysbit_t& regRhsRef { GetRegister32Bit(regRhs, cpu.state) };

                char data[4];
                BytesFromInteger(regLhsRef, data);
                float floatLhs { FloatFromBytes( data)};

                BytesFromInteger(regRhsRef, data);
                float floatRhs { FloatFromBytes(data)};

                BytesFromFloat(floatLhs-floatRhs, data);
                regRhsRef = IntegerFromBytes<sysbit_t>(data);
            }
            else
            {
                sysbit_t regLhsRef { GetRegister32Bit(regLhs, cpu.state) };
                sysbit_t& regRhsRef { GetRegister32Bit(regRhs, cpu.state) };
                regRhsRef = regLhsRef - regRhsRef;
            }
        )

        op_SubSafe32: block(
            sysbit_t rhs;
            sysbit_t lhs;
            rhs = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-4, 4).data);
            lhs = IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp-8, 4).data);

            char data[4];
            BytesFromInteger(lhs-rhs, data);
            return cpu.PushSome({ data, 4 });
        )

        op_SubSafeFloat: block(
            float rhs;
            float lhs;
            rhs = FloatFromBytes(ram.ReadSome(cpu.state.sp-4, 4).data);
            lhs = FloatFromBytes(ram.ReadSome(cpu.state.sp-8, 4).data);

            char data[4];
            BytesFromFloat<char>(lhs-rhs, data);
            return cpu.PushSome({ data, 4 });
        )

        op_SubSafe8: block(
            uchar_t rhs;
            uchar_t lhs;
            rhs = ram.Read(cpu.state.sp-1);
            lhs = ram.Read(cpu.state.sp-2);

            return cpu.Push(lhs-rhs);
        )

        op_IncrementLocal: block(
            const sysbit_t index { IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc, 4).data) };
            if (index < 0)
                CRASH(System::ErrorCode::IndexOutOfBounds, "Index can't be negative in ", OpCodesString(rom.Read(cpu.state.pc-1)));
            switch (OpCodes(rom.Read(cpu.state.pc-1)))
            {
                case OpCodes::incli:
                {
                    // incli <index> <constant>
                    char data[4];
                    const sysbit_t constant { IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc+4, 4).data) };
                    const sysbit_t local { IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.bp+index, 4).data) };
                    BytesFromInteger(constant+local, data);
                    System::ErrorCode err = ram.WriteSome(cpu.state.bp+index, {data, 4});
                    if (err == System::ErrorCode::Ok)
                        cpu.state.pc += 8;
                    return err;
                }
                case OpCodes::inclf:
                {
                    // inclf <index> <constant>
                    char data[4];
                    const float constant { FloatFromBytes(rom.ReadSome(cpu.state.pc+4, 4).data) };
                    const float local { FloatFromBytes(ram.ReadSome(cpu.state.bp+index, 4).data) };
                    BytesFromFloat(constant+local, data);
                    System::ErrorCode err = ram.WriteSome(cpu.state.bp+index, {data, 4});
                    cpu.state.pc += 8;
                    return err;
                }
                case OpCodes::inclb:
                {
                    // inclb <index> <constant>
                    const uchar_t constant { rom.Read(cpu.state.pc+4) };
                    const uchar_t local { static_cast<uchar_t>(ram.Read(cpu.state.bp+index)) };
                    System::ErrorCode errcx = ram.Write(cpu.state.bp+index, constant+local);
                    cpu.state.pc += 5;
                    return errcx;
                }
                default:
                    return System::ErrorCode::InvalidInstruction;
            }
        )


    op_ReadLocal: block(
            // rdlt <index>
            // rdle <index>
            sysbit_t size {
                static_cast<sysbit_t>
                (rom.Read(cpu.state.pc-1) == (uchar_t)OpCodes::rdlt ? 4 : 1)
            };
            sysbit_t index {
                IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc, 4).data)
            };

            const Slice values { ram.ReadSome(cpu.state.bp+index, size) };

            const System::ErrorCode errc { cpu.PushSome(values) };
            if (errc != System::ErrorCode::Ok)
                return errc;
            cpu.state.pc += 4;
        )


    op_CompareJump: block(
            const uchar_t compressedModes { static_cast<const uchar_t>(
                rom.Read(cpu.state.pc)
            )};

            Numo numMode {
                static_cast<uchar_t>(compressedModes >> 5)
            };
            const uchar_t compareMode {
                static_cast<const uchar_t>(compressedModes & 0b00011111)
            };

            bool status;

            if (numMode == Numo::UInt)
            {
                sysbit_t int1 { IntegerFromBytes<sysbit_t>(
                    ram.ReadSome(cpu.state.sp-8, 4).data
                )};
                sysbit_t int2 { IntegerFromBytes<sysbit_t>(
                    ram.ReadSome(cpu.state.sp-4, 4).data
                )};
                cpu.state.sp -= 8;
                status = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::Float)
            {
                float float1 { FloatFromBytes(
                    ram.ReadSome(cpu.state.sp-8, 4).data
                )};
                float float2 { FloatFromBytes(
                    ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                cpu.state.sp -= 8;
                status = CompareVarious(float1, float2, compareMode);
            }
            else if (numMode == Numo::Int)
            {
                int int1 { IntegerFromBytes<int32_t>(
                    ram.ReadSome(cpu.state.sp-8, 4).data
                )};
                int int2 { IntegerFromBytes<int32_t>(
                    ram.ReadSome(cpu.state.sp-4, 4).data
                )};

                cpu.state.sp -= 8;
                status = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::UByte)
            {
                uchar_t byte1 { static_cast<uchar_t>(
                    ram.Read(cpu.state.sp-2)
                )};
                uchar_t byte2 { static_cast<uchar_t>(
                    ram.Read(cpu.state.sp-1)
                )};

                cpu.state.sp -= 2;
                status = CompareVarious(byte1, byte2, compareMode);
            }
            else
            {
                char byte1 { ram.Read(cpu.state.sp-2) };
                char byte2 { ram.Read(cpu.state.sp-1) };

                cpu.state.sp -= 2;
                status = CompareVarious(byte1, byte2, compareMode);
            }

            sysbit_t address;

            if (status)
                address = IntegerFromBytes<sysbit_t>(
                    rom.ReadSome(cpu.state.pc+1, 4).data
                );
            else
                address = cpu.state.pc + 5;

#ifdef ENABLE_JIT
            JITError err { BranchIncrease(blocks, address, &jitContext, rom, settings.jit) };
            if (err == JITError::Ok)
                return System::ErrorCode::Ok;
            else if (err != JITError::VMLevelError)
            {
#endif
                // Safety test, address must be in bounds of rom
                RomSafetyCheck(address);
                cpu.state.pc = address;
                return System::ErrorCode::Ok;
#ifdef ENABLE_JIT
            }
            else
                return System::ErrorCode::JITError;
#endif
        )

        op_CompareLocal: block(
            const uchar_t compressedModes { static_cast<const uchar_t>(
                rom.Read(cpu.state.pc)
            )};
            cpu.state.pc++;

            const Numo numMode { static_cast<uchar_t>(compressedModes >> 5) };
            const uchar_t compareMode { static_cast<const uchar_t>(compressedModes & 0x1F) };

            const sysbit_t idx1 { IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc, sizeof(sysbit_t)).data) };
            const sysbit_t idx2 { IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc+sizeof(sysbit_t), sizeof(sysbit_t)).data) };
            cpu.state.pc += 2*sizeof(sysbit_t);

            if (numMode == Numo::UInt)
            {
                sysbit_t int1 { IntegerFromBytes<sysbit_t>(
                    ram.ReadSome(cpu.state.bp+idx1, 4).data
                )};
                sysbit_t int2 { IntegerFromBytes<sysbit_t>(
                    ram.ReadSome(cpu.state.bp+idx2, 4).data
                )};
                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::Float)
            {
                float float1 { FloatFromBytes(
                    ram.ReadSome(cpu.state.bp+idx1, 4).data
                )};
                float float2 { FloatFromBytes(
                    ram.ReadSome(cpu.state.bp+idx2, 4).data
                )};

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(float1, float2, compareMode);
            }
            else if (numMode == Numo::Int)
            {
                int int1 { IntegerFromBytes<int32_t>(
                    ram.ReadSome(cpu.state.bp+idx1, 4).data
                )};
                int int2 { IntegerFromBytes<int32_t>(
                    ram.ReadSome(cpu.state.bp+idx2, 4).data
                )};

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::UByte)
            {
                uchar_t byte1 { static_cast<uchar_t>(
                    ram.Read(cpu.state.bp+idx1)
                )};
                uchar_t byte2 { static_cast<uchar_t>(
                    ram.Read(cpu.state.bp+idx1)
                )};

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(byte1, byte2, compareMode);
            }
            else
            {
                char byte1 { ram.Read(cpu.state.bp+idx1) };
                char byte2 { ram.Read(cpu.state.sp+idx2) };

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(byte1, byte2, compareMode);
            }
            return System::ErrorCode::Ok;
        )

        op_SetFlag: block(
            uchar_t compressed { rom.Read(cpu.state.pc) };
            uchar_t flagToSet { static_cast<uchar_t>(compressed >> 4) };
            uchar_t value { static_cast<uchar_t>(compressed & 0x0F) };
            cpu.state.pc++;

            if (value == 1)
                cpu.state.flg |= (1 << flagToSet);
            else
                cpu.state.flg &= ~(1 << flagToSet);
        )
        
        op_SysCall:
        {
            static_assert(sizeof(void*) == sizeof(uintptr_t), "Native calls assume that the sizeof(uintptr_t) == sizeof(void*), otherwise they are not supported.");
            static_assert(sizeof(uintptr_t) >= sizeof(uint32_t), "Native calls assume that the sizeof(uintptr_t) >= sizeof(uint32_t), otherwise they are not supported.");

            // &bl is still set,
            // values are pushed to stack then syscall is made.
            // values are eaten.
            // sys <size:string>
            // TODO: Pass pointer to signature instead of signature
            const char* strptr { rom&(cpu.state.pc+sizeof(sysbit_t)) };
            sysbit_t size { IntegerFromBytes<sysbit_t>(rom.ReadSome(cpu.state.pc, 4).data) };
            std::string_view signatureStr(strptr, size);

            cpu.state.pc += size + 4;

            SysFunctionHandle& handle { handler.MakeFunctionHandle(signatureStr) };

            uintptr_t* args { static_cast<uintptr_t*>(alloca(handle.ArgCount() * sizeof(uintptr_t))) };
            void** argptrs { static_cast<void**>(alloca(handle.ArgCount() * sizeof(void**))) };

            uintptr_t returnValue;

            cpu.state.sp -= cpu.state.bl;
            for (size_t i = 0; i < handle.ArgCount(); i++)
            {
                argptrs[i] = args + i;
                switch (handle.argTypes[i])
                {
                    case FFIType::Int:
                    case FFIType::UInt:
                    {
                        sysbit_t val { IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp, 4).data) };
                        std::memcpy(args + i, &val, 4);
                        cpu.state.sp += 4;
                        continue;
                    }

                    case FFIType::VMPointer:
                    {
                        sysbit_t vmPtr { IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp, 4).data) };
                        void* realPtr { GetRealAddress(vmPtr) };
                        std::memcpy(args + i, &realPtr, sizeof(realPtr));
                        cpu.state.sp += 4;
                        continue;
                    }

                    case FFIType::NativePointer:
                    {
                        sysbit_t vmptr { IntegerFromBytes<sysbit_t>(ram.ReadSome(cpu.state.sp, 4).data) };
                        void* realPtr { reinterpret_cast<void*>(IntegerFromBytes<uintptr_t>(ram.ReadSome(vmptr, sizeof(uintptr_t)).data)) };
                        std::memcpy(args + i, &realPtr, sizeof(void*));
                        cpu.state.sp += 4;
                        continue;
                    }

                    case FFIType::Byte:
                    case FFIType::UByte:
                    case FFIType::Bool:
                    {
                        uchar_t val { static_cast<uchar_t>(ram.Read(cpu.state.sp)) };
                        std::memcpy(args + i, &val, sizeof(uchar_t));
                        cpu.state.sp++;
                        continue;
                    }

                    case FFIType::Float:
                    {
                        float vmptr { FloatFromBytes(ram.ReadSome(cpu.state.sp, 4).data) };
                        std::memcpy(args + i, &vmptr, sizeof(float));
                        cpu.state.sp += 4;
                        continue;
                    }

                    case FFIType::Void:
                        __builtin_unreachable();
                }
            }

            cpu.state.sp -= cpu.state.bl;
            handler(handle, argptrs, static_cast<void*>(&returnValue));
            cpu.state.bl = GetTypeSize(handle.returnType);

            switch (handle.returnType)
            {
                case FFIType::Int:
                case FFIType::UInt:
                {
                    char buf[4];
                    sysbit_t val;
                    std::memcpy(&val, &returnValue, sizeof(sysbit_t));
                    BytesFromInteger(val, buf);
                    return cpu.PushSome({ buf, 4 });
                }

                case FFIType::VMPointer:
                {
                    char buf[4];
                    sysbit_t val { GetVMAddress(reinterpret_cast<void*>(returnValue)) };
                    BytesFromInteger(val, buf);
                    return cpu.PushSome({ buf, 4 });
                }

                case FFIType::NativePointer:
                {
                    sysbit_t vmAddr { ram.Allocate(sizeof(uintptr_t)) };

                    char* addressBuf { static_cast<char*>(alloca(sizeof(uintptr_t))) };
                    BytesFromInteger(returnValue, addressBuf);
                    System::ErrorCode err { ram.WriteSome(vmAddr, { addressBuf, sizeof(uintptr_t) }) };

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                    {
                        LOGE(System::LogLevel::Medium, "Error while returning native pointer from native call.");
                        return err;
                    }

                    char returnBuf[4];
                    BytesFromInteger(vmAddr, returnBuf);
                    err = cpu.PushSome({ returnBuf, 4 });

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        LOGE(System::LogLevel::Medium, "Error while returning native pointer from native call.");

                    return err;
                }

                case FFIType::Byte:
                case FFIType::UByte:
                case FFIType::Bool:
                {
                    uchar_t val;
                    std::memcpy(&val, &returnValue, 1);
                    return cpu.Push(val);
                }

                case FFIType::Float:
                {
                    char buf[4];
                    float val;
                    std::memcpy(&val, &returnValue, 4);
                    BytesFromFloat(val, buf);
                    return cpu.PushSome({ buf, 4 });
                }

                case FFIType::Void:
                    return System::ErrorCode::Ok;
            }
        }

        op_BitXor: block(
            return BitLogic(
                {OpCodes::xorst, OpCodes::xorse, OpCodes::xorr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a ^ b; }
            );
        )
    }
    catch (const CSRException& e)
    {
        std::cout << e;
        return e.GetCode();
    }
    catch (const std::exception& e)
    {
        LOGE(System::LogLevel::Medium, "Unhandled exception.\n\t", e.what());
        return System::ErrorCode::UnhandledException;
    }

    return System::ErrorCode::UnhandledException;
}

#define arr std::array
#define fn std::function<sysbit_t(sysbit_t, sysbit_t)>
OPR FlatVM::BitLogic(arr<OpCodes, 3> op, fn bitwise) noexcept
{
    try_catch(
        OpCodes opc { rom.Read(cpu.state.pc-1) };
        if (opc == op.at(0))
        {
            sysbit_t val1 { IntegerFromBytes<sysbit_t>(
                ram.ReadSome(cpu.state.sp-8, 4).data
            )};

            sysbit_t val2 { IntegerFromBytes<sysbit_t>(
                ram.ReadSome(cpu.state.sp-4, 4).data
            )};

            if (Is8BitReg(rom.Read(cpu.state.pc)))
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(rom.Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(rom.Read(cpu.state.pc)),
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
                static_cast<uchar_t>(ram.Read(cpu.state.sp-2))
            };

            uchar_t val2 {
                static_cast<uchar_t>(ram.Read(cpu.state.sp-1))
            };

            if (Is8BitReg(rom.Read(cpu.state.pc)))
            {
                uchar_t& reg { GetRegister8Bit(
                    RegisterModeFlags(rom.Read(cpu.state.pc)),
                    cpu.state
                )};
                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                sysbit_t& reg { GetRegister32Bit(
                    RegisterModeFlags(rom.Read(cpu.state.pc)),
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
                rom.Read(cpu.state.pc)
            };

            RegisterModeFlags reg2mode {
                rom.Read(cpu.state.pc+1)
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


