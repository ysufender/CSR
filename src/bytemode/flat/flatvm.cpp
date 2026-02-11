#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <ffi.h>
#include <fstream>
#include <functional>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "bytemode/syscall.hpp"
#include "extensions/streamextensions.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/assemblyinfo.hpp"
#include "bytemode/flat/flatram.hpp"
#include "bytemode/flat/flatvm.hpp"
#include "extensions/converters.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/jit.hpp"
#include "CSRConfig.hpp"
#include "slice.hpp"
#include "system.hpp"

template<typename T>
using Res = System::Result<T>;

#define None(res) \
    if (System::None(res)) [[unlikely]] \
        return System::GetErr(res)

#define Get(res) \
    System::Get(std::move(res))

#define OPR const System::ErrorCode
#define NOT_IMP(name) \
        LOGE(System::LogLevel::Low, "Implement ", #name); \
        return System::ErrorCode::Ok;

#define Enumc(regn) static_cast<char>(regn)
#define Is8BitReg(reg) (Enumc(reg) >= Enumc(RegisterModeFlags::al)) && (Enumc(reg) <= Enumc(RegisterModeFlags::flg))
#define RomSafetyCheck(addr) \
        if (address < 12 || address > rom.Size()) [[unlikely]] \
            return System::ErrorCode::ROMAccessError;

#define block(expr) \
        { \
            expr \
            return System::ErrorCode::Ok; \
        }

using Numo = NumericModeFlags;

static Res<sysbit_t*> GetRegister32Bit(RegisterModeFlags reg, FlatCPU::State& state)
{
    static sysbit_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::eax: return &state.eax;
        case RegisterModeFlags::ebx: return &state.ebx;
        case RegisterModeFlags::ecx: return &state.ecx;
        case RegisterModeFlags::edx: return &state.edx;
        case RegisterModeFlags::esi: return &state.esi;
        case RegisterModeFlags::edi: return &state.edi;
        case RegisterModeFlags::pc: return &state.pc;
        case RegisterModeFlags::sp: return &state.sp;
        case RegisterModeFlags::bp: return &state.bp;
        default: [[unlikely]]
            LOGE(System::LogLevel::High, std::to_string((int)reg), " is not a 32bit register.");
            return System::ErrorCode::InvalidSpecifier;
    }
}

static Res<uchar_t*> GetRegister8Bit(RegisterModeFlags reg, FlatCPU::State& state)
{
    static uchar_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::al: return &state.al;
        case RegisterModeFlags::bl: return &state.bl;
        case RegisterModeFlags::cl: return &state.cl;
        case RegisterModeFlags::dl: return &state.dl;
        case RegisterModeFlags::flg: return &state.flg;
        default: [[unlikely]]
            LOGE(System::LogLevel::High, std::to_string((int)reg), " is not an 8bit register.");
            return System::ErrorCode::InvalidSpecifier;
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

System::ErrorCode InitStandardLibrary(SysCallHandler& handler);

System::ErrorCode FlatVM::SetUpCommon()
{
#ifdef ENABLE_JIT
    if (settings.jit)
    {
        for (const auto& [symbol, addr] : assembly.Symbols())
        {
            Res<uchar_t> res { rom[addr] };

            None(res);

            if (Get(res) <= OpCodesMax)
                blocks.Add(addr);
        }

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

    if (InitStandardLibrary(handler) != System::ErrorCode::Ok) [[unlikely]]
    {
        LOGE(System::LogLevel::High, "Failed to initialize standard library functions.");
        return System::ErrorCode::VMError;
    }

    return System::ErrorCode::Ok;
}

using BytecodePair = std::pair<std::unique_ptr<const char[]>, std::streamoff>;
Res<BytecodePair> FlatVM::ReadBytecode(std::istream& bytecode)
{
    // TODO: All bytecodes must include AssemblyInfo
#ifndef TOOLCHAIN_MODE
    bytecode.seekg(-sizeof(uint64_t), std::ios::end);
    uint64_t size { };
    Extensions::Serialization::DeserializeInteger(size, bytecode);
    bytecode.seekg(-(sizeof(uint64_t)+size), std::ios::end);
    IStreamPos(bytecode, bytecodeEnd, {{
        LOGE(System::LogLevel::High, "Couldn't load executable ", settings.path.c_str());
        return System::ErrorCode::FileIOError;
    }});
    assembly.Deserialize(bytecode);
#else
    bytecode.seekg(0, std::ios::end);
    IStreamPos(bytecode, bytecodeEnd, {
        LOGE(System::LogLevel::High, "Couldn't properly load given bytecode.");
        return System::ErrorCode::VMError;
    });
#endif

    //if (settings.path.extension() != ".jef")
    if (!(assembly.Flags() & AssemblyFlags::Executable)) [[unlikely]]
    {
        LOGE(System::LogLevel::High, "FlatVM does not support given filetype ", settings.path.c_str(), ". It is not marked as an executable.");
        return System::ErrorCode::UnsupportedFileType;
    }

#ifdef ENABLE_JIT
    if (!(assembly.Flags() & AssemblyFlags::SymbolInfo) && settings.jit) [[unlikely]]
        LOGW("Current execution is marked as JIT target however the file ", settings.path.c_str(), " does not contain symbol information.");
#endif

    bytecode.seekg(0, std::ios::beg);
    std::unique_ptr<char[]> data { std::make_unique_for_overwrite<char[]>(bytecodeEnd) };
    bytecode.read(data.get(), bytecodeEnd);

    return Res<BytecodePair>{
        std::in_place_type<BytecodePair>,
            rval(data),
            bytecodeEnd
    };
}

Res<FlatVM> FlatVM::New(VMSettings settings)
{
    if (!std::filesystem::exists(settings.path)) [[unlikely]]
    {
        LOGE(System::LogLevel::High, "Couldn't find executable ", settings.path.string());
        return System::ErrorCode::SourceFileNotFound;
    }

    Res<std::ifstream> streamRes { System::OpenInFile(settings.path) };
    None(streamRes);

    // Create ROM
    std::ifstream bytecode { Get(streamRes) };

    FlatVM vm;
    vm.settings = settings;

    Res<BytecodePair> bytecodeRes { vm.ReadBytecode(bytecode) };
    bytecode.close();

    None(bytecodeRes);

    auto [data, size] = Get(bytecodeRes);

    Res<FlatROM> romRes { FlatROM { std::move(data), static_cast<sysbit_t>(size) } };

    None(romRes);

    vm.rom = Get(romRes);

    // Create RAM
    Res<Slice> stackSizeRes { vm.rom.ReadSome(4, 4) };
    None(stackSizeRes);

    Res<Slice> heapSizeRes { vm.rom.ReadSome(8, 4) };
    None(heapSizeRes);

    Res<FlatRAM> ramRes { FlatRAM::New(
        IntegerFromBytes<sysbit_t>(Get(stackSizeRes).data),
        IntegerFromBytes<sysbit_t>(Get(heapSizeRes).data)
    )};

    None(ramRes);

    vm.ram = Get(ramRes);

    // Create CPU
    Res<Slice> pcRes { vm.rom.ReadSome(0, 4) };
    None(pcRes);

    FlatCPU cpu { vm.ram };
    cpu.state.pc = IntegerFromBytes<sysbit_t>(Get(pcRes).data);
    vm.cpu = std::move(cpu);

    vm.SetUpCommon();

    return vm;
}

#ifdef TOOLCHAIN_MODE
#error "This section of code is not adapted to the result typed structure of the codebase YET"
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
        while (cpu.state.pc < rom.Size() && errcx == System::ErrorCode::Ok) [[likely]]
            errcx = Cycle();
    }
    catch (const CSRException& e)
    {
        [[unlikely]]
        std::cout << e;
        return e.GetCode();
    }
    catch (const std::exception& e)
    {
        [[unlikely]]
        LOGE(System::LogLevel::High, "Unhandled exception.\n\t", e.what());
        return System::ErrorCode::UnhandledException;
    }

    return errcx;
}

const System::ErrorCode FlatVM::Cycle()
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
        if (code != System::ErrorCode::Ok) [[unlikely]]
        {
            LOGE(System::LogLevel::High, "ROM read error: ", System::ErrorCodeString(code));
            return code;
        }

        if (op >= std::size(jumpTable)) [[unlikely]]
        {
            LOGE(System::LogLevel::High, "Invalid opcode '", std::to_string(op), "' at PC=", std::to_string(cpu.state.pc));
            return System::ErrorCode::InvalidInstruction;
        }

        cpu.state.pc++;
        LOGD("TEST");
        goto *jumpTable[op];

        op_NoOperation: block()

        op_StoreThirtyTwo: block(
            Res<Slice> romRes { rom.ReadSome(cpu.state.pc, 4) };

            None(romRes);

            const System::ErrorCode err { cpu.PushSome(Get(romRes)) };

            if (err == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc+=4;

            return err;
        )

        op_StoreEight: block(
            Res<uchar_t> romRes { rom.Read(cpu.state.pc) };

            None(romRes);

            const System::ErrorCode code { cpu.Push(Get(romRes)) };

            if (code == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc++;

            return code;
        )

        op_StoreFromSymbol: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (op == (char)OpCodes::stes ? 1 : 4)
            };

            Res<Slice> symbolDataRes { rom.ReadSome(cpu.state.pc, 4) };
            None(symbolDataRes);

            sysbit_t symbol { IntegerFromBytes<sysbit_t>(Get(symbolDataRes).data) };

            Res<Slice> valueDataRes { rom.ReadSome(symbol, size) };
            None(valueDataRes);

            const System::ErrorCode err { cpu.PushSome(Get(valueDataRes)) };

            if (err == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc+=4;

            return err;
        )

        op_LoadFromStack: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (op == (char)OpCodes::ldt ? 4 : 1)
            };

            Res<Slice> valuesRes { ram.ReadSome(cpu.state.sp-size, size) };
            None(valuesRes);

            const System::ErrorCode errc { ram.WriteSome(cpu.state.ebx, Get(valuesRes)) };

            if (errc != System::ErrorCode::Ok) [[likely]]
                return ram.Deallocate(cpu.state.ebx, size);

            return errc;

        )

        op_ReadFromHeap: block(
            sysbit_t size {
                static_cast<sysbit_t>
                (op == (char)OpCodes::rdt ? 4 : 1)
            };

            Res<Slice> valuesRes { ram.ReadSome(cpu.state.ebx, size) };
            None(valuesRes);

            const System::ErrorCode errc { cpu.PushSome(Get(valuesRes)) };
            return errc;
        )

        op_ReadFromRegister: block(
            Res<uchar_t> regRes { rom.Read(cpu.state.pc) };
            None(regRes);

            RegisterModeFlags reg { Get(regRes) };
            sysbit_t size { Is8BitReg(reg) ? sysbit_t{1} : sysbit_t{4} };
            System::ErrorCode err;

            if (Is8BitReg(reg))
            {
                char data[1];
                Res<uchar_t*> regRes { GetRegister8Bit(reg, cpu.state) };
                None(regRes);

                BytesFromInteger<uchar_t>(*Get(regRes), data);

                err = cpu.PushSome(Get(Slice::New(data, size)));
            }
            else
            {
                char data[4];
                Res<sysbit_t*> regRes { GetRegister32Bit(reg, cpu.state) };
                None(regRes);

                BytesFromInteger<sysbit_t>(*Get(regRes), data);
                
                err = cpu.PushSome(Get(Slice::New(data, size)));
            }

            if (err == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc++;

            return err;
        )

        op_Move: block(
            System::ErrorCode err;

            Res<uchar_t> regRes { rom.Read(cpu.state.pc) };
            None(regRes);

            RegisterModeFlags regFlag { Get(regRes) };
            sysbit_t size { Is8BitReg(regFlag) ? sysbit_t{1} : sysbit_t{4} };

            switch (OpCodes(op))
            {
                case OpCodes::movc:
                {
                    if (size == 1)
                    {
                        Res<uchar_t*> lhsRes { GetRegister8Bit(regFlag, cpu.state) };
                        None(lhsRes);

                        Res<uchar_t> rhsRes { rom.Read(cpu.state.pc+1) };
                        None(rhsRes);

                        *Get(lhsRes) = Get(rhsRes);

                        cpu.state.pc+=2;
                    }
                    else
                    {
                        Res<sysbit_t*> lhsRes { GetRegister32Bit(regFlag, cpu.state) };
                        None(lhsRes);

                        Res<Slice> rhsRes { rom.ReadSome(cpu.state.pc+1, 4) };
                        None(rhsRes);

                        *Get(lhsRes) = IntegerFromBytes<sysbit_t>(Get(rhsRes).data);
                        cpu.state.pc+=5;
                    }
                    return System::ErrorCode::Ok;
                }

                case OpCodes::movs:
                {
                    if (size == 1)
                    {
                        Res<uchar_t*> lhsRes { GetRegister8Bit(regFlag, cpu.state) };
                        None(lhsRes);

                        Res<Slice> rhsRes { ram.ReadSome(cpu.state.sp-1, 1) };
                        None(rhsRes);

                        *Get(lhsRes) = IntegerFromBytes<uchar_t>(
                            Get(rhsRes).data
                        );
                    }
                    else
                    {
                        Res<sysbit_t*> lhsRes { GetRegister32Bit(regFlag, cpu.state) };
                        None(lhsRes);

                        Res<Slice> rhsRes { ram.ReadSome(cpu.state.sp-4, 4) };
                        None(rhsRes);

                        *Get(lhsRes) = IntegerFromBytes<sysbit_t>(
                            Get(rhsRes).data
                        );
                    }

                    cpu.state.pc++;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::movr:
                {
                    regRes = rom.Read(cpu.state.pc+1);
                    None(regRes);

                    RegisterModeFlags reg2Flag { Get(regRes) };
                    sysbit_t size2 { Is8BitReg(reg2Flag) ? sysbit_t{1} : sysbit_t{4} };
                    sysbit_t val;

                    if (size == 1)
                    {
                        Res<uchar_t*> rhsRes { GetRegister8Bit(regFlag, cpu.state) };
                        None(rhsRes);

                        val = static_cast<sysbit_t>(*Get(rhsRes));
                    }
                    else
                    {
                        Res<sysbit_t*> rhsRes { GetRegister32Bit(regFlag, cpu.state) };
                        None(rhsRes);

                        val = *Get(rhsRes);
                    }

                    if (size2 == 1)
                    {
                        Res<uchar_t*> lhsRes { GetRegister8Bit(reg2Flag, cpu.state) };
                        None(lhsRes);

                        *Get(lhsRes) = val;
                    }
                    else
                    {
                        Res<sysbit_t*> lhsRes { GetRegister32Bit(reg2Flag, cpu.state) };
                        None(lhsRes);

                        *Get(lhsRes) = val;
                    }

                    cpu.state.pc+=2;
                    return System::ErrorCode::Ok;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Add32: block(
            sysbit_t int1;
            sysbit_t int2;

            Res<Slice> sliceRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes);

            int1 = IntegerFromBytes<sysbit_t>(Get(sliceRes).data);
            cpu.PopSome(4);

            Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes2);

            int2 = IntegerFromBytes<sysbit_t>(Get(sliceRes2).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromInteger(int1+int2, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_AddFloat: block(
            float float1;
            float float2;

            Res<Slice> sliceRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes);

            float1 = FloatFromBytes(Get(sliceRes).data);
            cpu.PopSome(4);

            Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes2);

            float2 = FloatFromBytes(Get(sliceRes2).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromFloat<char>(float1+float2, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
    )

        op_Add8: block(
            uchar_t byte1;
            uchar_t byte2;

            Res<char> res { ram.Read(cpu.state.sp-1) };
            None(res);
            byte1 = Get(res);
            cpu.Pop();

            res = ram.Read(cpu.state.sp-1);
            None(res);
            byte2 = Get(res);
            cpu.Pop();

            return cpu.Push(byte1+byte2);
        )

        op_AddReg: block(
            Res<uchar_t> res { rom.Read(cpu.state.pc-1) };
            None(res);
            OpCodes op { Get(res) };

            res = rom.Read(cpu.state.pc);
            None(res);
            RegisterModeFlags reg1 { Get(res) };

            res = rom.Read(cpu.state.pc+1);
            None(res);
            RegisterModeFlags reg2 { Get(res) };

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
            ) [[unlikely]]
            {
                LOGE(System::LogLevel::High,
                    "PC: ",std::to_string(cpu.state.pc-1),
                    " ", OpCodesString(op),
                    " ", std::to_string((int)reg1),
                    " ", std::to_string((int)reg2),
                    " Given registers are not compatible with given numeric type."
                );
                return System::ErrorCode::InvalidSpecifier;
            }

            cpu.state.pc+=2;

            if (Is8BitReg(reg1))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(reg1, cpu.state) };
                None(regRes);
                uchar_t reg1 { *Get(regRes) };

                regRes = GetRegister8Bit(reg2, cpu.state);
                None(regRes);
                uchar_t& reg2ref { *Get(regRes) };

                reg2ref += reg1;
            }
            else if (op == OpCodes::addrf)
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                None(regRes);
                sysbit_t reg1 { *Get(regRes) };

                regRes = GetRegister32Bit(reg2, cpu.state);
                None(regRes);
                sysbit_t& reg2ref { *Get(regRes) };

                char data[4];
                BytesFromInteger(reg1, data);
                float float1 { FloatFromBytes( data)};

                BytesFromInteger(reg2ref, data);
                float float2 { FloatFromBytes( data)};

                BytesFromFloat(float1+float2, data);
                reg2ref = IntegerFromBytes<sysbit_t>( data);
            }
            else
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                None(regRes);
                sysbit_t reg1 { *Get(regRes) };

                regRes = GetRegister32Bit(reg2, cpu.state);
                None(regRes);
                sysbit_t& reg2ref { *Get(regRes) };

                reg2ref += reg1;
            }
            return System::ErrorCode::Ok;
        )

        op_AddSafe32: block(
            sysbit_t int1;
            sysbit_t int2;

            Res<Slice> sliceRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes);
            int1 = IntegerFromBytes<sysbit_t>(Get(sliceRes).data);

            Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-8, 4) };
            None(sliceRes2);
            int2 = IntegerFromBytes<sysbit_t>(Get(sliceRes2).data);

            char data[4];
            BytesFromInteger(int1+int2, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_AddSafeFloat: block(
            float float1;
            float float2;

            Res<Slice> sliceRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sliceRes);
            float1 = FloatFromBytes(Get(sliceRes).data);

            Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-8, 4) };
            None(sliceRes2);
            float2 = FloatFromBytes(Get(sliceRes2).data);

            char data[4];
            BytesFromFloat<char>(float1+float2, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_AddSafe8: block(
            uchar_t byte1;
            uchar_t byte2;

            Res<char> resRes { ram.Read(cpu.state.sp-1) };
            None(resRes);
            byte1 = Get(resRes);

            resRes = ram.Read(cpu.state.sp-2);
            None(resRes);
            byte2 = Get(resRes);

            return cpu.Push(byte1+byte2);
        )

        op_MemCopy: block(
            sysbit_t fromAddr { *Get(GetRegister32Bit(RegisterModeFlags::eax, cpu.state)) };
            sysbit_t toAddr { *Get(GetRegister32Bit(RegisterModeFlags::ebx, cpu.state)) };
            sysbit_t size { *Get(GetRegister32Bit(RegisterModeFlags::ecx, cpu.state)) };

            if ((toAddr + size) > ram.Size()) [[unlikely]]
            {
                LOGE(
                    System::LogLevel::High,
                    "In ", nameof(MemCopy), " instruction will cause memory overflow.",
                    "\nTo Address: ", std::to_string(toAddr), " Size: ", std::to_string(size),
                    "\nMemory Size: ", std::to_string(ram.Size())
                );
                return System::ErrorCode::MemoryOverflow;
            }

            Res<Slice> dataToCopy { ram.ReadSome(fromAddr, size) };
            None(dataToCopy);
            const System::ErrorCode code { ram.WriteSome(toAddr, Get(dataToCopy)) };

            if (code == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc++;

            return code;
        )

        op_Increment: block(
            switch (OpCodes(op))
            {
                case OpCodes::inci:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sliceRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(Get(sliceRes).data) };

                    Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sliceRes2);
                    sysbit_t stack { IntegerFromBytes<sysbit_t>(Get(sliceRes2).data) };

                    char data[4];
                    BytesFromInteger(stack+amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::incf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sliceRes);
                    float amount { FloatFromBytes(
                        Get(sliceRes).data
                    )};

                    Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sliceRes);
                    float stack { FloatFromBytes(
                        Get(sliceRes2).data
                    )};

                    char data[4];
                    BytesFromFloat(amount+stack, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::incb:
                {
                    if (cpu.state.sp < 1) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    uchar_t amount { static_cast<uchar_t>(Get(res)) };

                    Res<char> cRes = ram.Read(cpu.state.sp-1);
                    None(cRes);
                    uchar_t stack { static_cast<uchar_t>(Get(cRes)) };

                    const System::ErrorCode code { ram.Write(
                        cpu.state.sp-1,
                        amount + stack
                    )};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc++;

                    return code;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_IncrementReg: block(
            switch (OpCodes(op))
            {
                case OpCodes::incri:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<sysbit_t*> regRes { GetRegister32Bit(RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    sysbit_t& reg { *Get(regRes) };

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc+1, 4) };
                    None(sliceRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        Get(sliceRes).data
                    )};

                    reg += amount;

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::incrf:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<sysbit_t*> regRes { GetRegister32Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    sysbit_t& reg { *Get(regRes) };

                    char data[4];
                    BytesFromInteger(reg, data);
                    float regVal { FloatFromBytes(data)};

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc+1, 4) };
                    None(sliceRes);
                    float amount { FloatFromBytes( Get(sliceRes).data) };

                    BytesFromFloat(regVal+amount, data);
                    reg = IntegerFromBytes<sysbit_t>(data);

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::incrb:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<uchar_t*> regRes { GetRegister8Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    uchar_t& reg { *Get(regRes) };

                    modeRes = rom.Read(cpu.state.pc+1);
                    None(modeRes);
                    uchar_t amount { static_cast<uchar_t>(Get(modeRes)) };

                    reg += amount;

                    cpu.state.pc+=2;
                    return System::ErrorCode::Ok;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_IncrementSafe: block(
            switch (OpCodes(op))
            {
                case OpCodes::incsi:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sliceRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        Get(sliceRes).data
                    )};

                    Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sliceRes2);
                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        Get(sliceRes2).data
                    )};

                    char data[4];
                    BytesFromInteger(stack+amount, data);
                    const System::ErrorCode code { cpu.PushSome(Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::incsf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sliceRes);
                    float amount { FloatFromBytes(
                        Get(sliceRes).data
                    )};

                    Res<Slice> sliceRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sliceRes2);
                    float stack { FloatFromBytes(
                        Get(sliceRes2).data
                    )};

                    char data[4];
                    BytesFromFloat(amount+stack, data);
                    const System::ErrorCode code { cpu.PushSome(Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::incsb:
                {
                    if (cpu.state.sp < 1) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Increment),
                            "can't increment (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    uchar_t amount { Get(res) };

                    Res<char> cRes { ram.Read(cpu.state.sp-1) };
                    None(cRes);
                    uchar_t stack { static_cast<uchar_t>(Get(cRes)) };

                    const System::ErrorCode code { cpu.Push( amount + stack) };

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc++;

                    return code;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Decrement: block(
            switch (OpCodes(op))
            {
                case OpCodes::dcri:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    char data[4];
                    BytesFromInteger(stack - amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::dcrf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement float from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    float amount { FloatFromBytes(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float stack { FloatFromBytes(
                        Get(sRes2).data
                    )};

                    char data[4];
                    BytesFromFloat(stack - amount, data);
                    const System::ErrorCode code { ram.WriteSome( cpu.state.sp-4, Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::dcrb:
                {
                    if (cpu.state.sp < 1) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement (u)byte from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    uchar_t amount { Get(res) };

                    Res<char> cRes { ram.Read(cpu.state.sp-1) };
                    None(cRes);
                    uchar_t stack { static_cast<uchar_t>(Get(cRes)) };

                    const System::ErrorCode code { ram.Write(
                        cpu.state.sp-1,
                        stack - amount
                    )};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc++;

                    return code;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DecrementReg: block(
            switch (OpCodes(op))
            {
                case OpCodes::dcrri:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);
                    Res<sysbit_t*> regRes { GetRegister32Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    sysbit_t& reg { *Get(regRes) };

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc+1, 4) };
                    None(sliceRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        Get(sliceRes).data
                    )};

                    reg -= amount;

                    cpu.state.pc+=5;

                    return System::ErrorCode::Ok;
                }

                case OpCodes::dcrrf:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<sysbit_t*> regRes { GetRegister32Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    sysbit_t& reg { *Get(regRes) };

                    char data[4];
                    BytesFromInteger(reg, data);
                    float regVal { FloatFromBytes(data)};

                    Res<Slice> sliceRes { rom.ReadSome(cpu.state.pc+1, 4) };
                    None(sliceRes);
                    float amount { FloatFromBytes(
                        Get(sliceRes).data
                    )};

                    BytesFromFloat(regVal - amount, data);
                    reg = IntegerFromBytes<sysbit_t>(data);

                    cpu.state.pc+=5;
                    return System::ErrorCode::Ok;
                }

                case OpCodes::dcrrb:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<uchar_t*> regRes { GetRegister8Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    uchar_t& reg { *Get(regRes) };

                    modeRes = rom.Read(cpu.state.pc+1);
                    None(modeRes);
                    uchar_t amount { Get(modeRes) };
                    reg -= amount;

                    cpu.state.pc+=2;
                    return System::ErrorCode::Ok;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DecrementSafe: block(
            switch (OpCodes(op))
            {
                case OpCodes::dcrsi:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement (u)int from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    sysbit_t amount { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t stack { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )} ;

                    char data[4];
                    BytesFromInteger(stack - amount, data);
                    const System::ErrorCode code { cpu.PushSome(Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::dcrsf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement float from stack, SP < 4."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    float amount { FloatFromBytes(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float stack { FloatFromBytes(
                        Get(sRes2).data
                    )};

                    char data[4];
                    BytesFromFloat(stack - amount, data);
                    const System::ErrorCode code { cpu.PushSome(Get(Slice::New(data, 4)))};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc+=4;

                    return code;
                }

                case OpCodes::dcrsb:
                {
                    if (cpu.state.sp < 1) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "In ", nameof(Decrement),
                            "can't decrement (u)byte from stack, SP < 1."
                        );
                        return System::ErrorCode::Bad;
                    }

                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    uchar_t amount { Get(res) };

                    Res<char> cRes { ram.Read(cpu.state.sp-1) };
                    None(cRes);
                    uchar_t stack { static_cast<uchar_t>(Get(cRes)) };

                    const System::ErrorCode code { cpu.Push(
                        stack - amount
                    )};

                    if (code == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc++;

                    return code;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }


        )

        op_BitAnd: block(
            return BitLogic(
                OpCodes(op),
                {OpCodes::andst, OpCodes::andse, OpCodes::andr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a & b; }
            );
        )

        op_BitOr: block(
            return BitLogic(
                OpCodes(op),
                {OpCodes::orst, OpCodes::orse, OpCodes::orr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a | b; }
            );
        )

        op_BitNor: block(
            // LOGW("This operation ", nameof(BitNor), " is stupid as hell. Why does it exist?");
            return BitLogic(
                OpCodes(op),
                {OpCodes::norst, OpCodes::norse, OpCodes::norr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return ~(a | b);}
            );
        )

        op_SwapTop: block(
            switch (OpCodes(op))
            {
                case OpCodes::swpt:
                {
                    if (cpu.state.sp < 8) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "Can't swap 32-bits on stack, SP < 8"
                        );
                        return System::ErrorCode::RAMAccessError;
                    }

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    sysbit_t bottom { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t top { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    {
                        char data[4];
                        BytesFromInteger(top, data);
                        const System::ErrorCode err { ram.WriteSome( cpu.state.sp-8, Get(Slice::New(data, 4)))};

                        if (err != System::ErrorCode::Ok) [[unlikely]]
                            return err;
                    }

                    char data[4];
                    BytesFromInteger(bottom, data);
                    return ram.WriteSome( cpu.state.sp-4, Get(Slice::New(data, 4)));
                }

                case OpCodes::swpe:
                {
                    if (cpu.state.sp < 2) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "Can't swap 8-bits on stack, SP < 2"
                        );
                        return System::ErrorCode::RAMAccessError;
                    }

                    Res<char> bottom { ram.Read(cpu.state.sp-2) };
                    None(bottom);

                    Res<char> top { ram.Read(cpu.state.sp-1) };
                    None(top);

                    {
                        const System::ErrorCode err { ram.Write(
                            cpu.state.sp-2,
                            Get(top)
                        )};

                        if (err != System::ErrorCode::Ok)
                            return err;
                    }

                    return ram.Write(
                        cpu.state.sp-1,
                        Get(bottom)
                    );
                }

                case OpCodes::swpr:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);
                    RegisterModeFlags reg1flag { Get(modeRes) };

                    modeRes = rom.Read(cpu.state.pc+1);
                    None(modeRes);
                    RegisterModeFlags reg2flag { Get(modeRes) };

                    sysbit_t reg1;
                    sysbit_t reg2;

                    if (Is8BitReg(reg1flag))
                    {
                        Res<uchar_t*> regRes { GetRegister8Bit(reg1flag, cpu.state) };
                        None(regRes);
                        reg1 = static_cast<sysbit_t>(*Get(regRes));
                    }
                    else
                    {
                        Res<sysbit_t*> regRes { GetRegister32Bit(reg1flag, cpu.state) };
                        None(regRes);
                        reg1 = *Get(regRes);
                    }

                    if (Is8BitReg(reg2flag))
                    {
                        Res<uchar_t*> regRes { GetRegister8Bit(reg2flag, cpu.state) };
                        None(regRes);
                        reg2 = static_cast<sysbit_t>(*Get(regRes));
                    }
                    else
                    {
                        Res<sysbit_t*> regRes { GetRegister32Bit(reg2flag, cpu.state) };
                        None(regRes);
                        reg2 = *Get(regRes);
                    }

                    if (Is8BitReg(reg1flag))
                    {
                        Res<uchar_t*> lhsRes { GetRegister8Bit(reg1flag, cpu.state) };
                        None(lhsRes);
                        *Get(lhsRes) = static_cast<uchar_t>(reg2);
                    }
                    else
                    {
                        Res<sysbit_t*> lhsRes { GetRegister32Bit(reg1flag, cpu.state) };
                        None(lhsRes);
                        *Get(lhsRes) = reg2;
                    }

                    if (Is8BitReg(reg2flag))
                    {
                        Res<uchar_t*> lhsRes { GetRegister8Bit(reg2flag, cpu.state) };
                        None(lhsRes);
                        *Get(lhsRes) = static_cast<uchar_t>(reg1);
                    }
                    else
                    {
                        Res<sysbit_t*> lhsRes { GetRegister32Bit(reg2flag, cpu.state) };
                        None(lhsRes);
                        *Get(lhsRes) = reg1;
                    }

                    return System::ErrorCode::Ok;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DuplicateTop: block(
            switch (OpCodes(op))
            {
                case OpCodes::dupt:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "Can't duplicate 32-bits on stack. SP < 4"
                        );
                        return System::ErrorCode::RAMAccessError;
                    }

                    Res<Slice> data { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(data);
                    return cpu.PushSome(Get(data));
                }

                case OpCodes::dupe:
                {
                    if (cpu.state.sp < 1) [[unlikely]]
                    {
                        LOGE(
                            System::LogLevel::High,
                            "Can't duplicate 8-bits on stack. SP < 1"
                        );
                        return System::ErrorCode::RAMAccessError;
                    }

                    Res<char> data { ram.Read(cpu.state.sp-1) };
                    None(data);
                    return cpu.Push(Get(data));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_RawDataStack: block(
            LOGD("TEST");
            switch (OpCodes(op))
            {
                case OpCodes::raw:
                {
                    // raw <size> <..data..>
                    LOGD("TEST");
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    // TODO: size is 19 for some reason, fix it
                    sysbit_t size { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};
                    cpu.state.pc += 4;

                    LOGD("TEST");
                    LOGD("Size ", std::to_string(size));
                    System::ErrorCode err { System::ErrorCode::Ok };
                    for (; err == System::ErrorCode::Ok && size > 0; size--)
                    {
                        LOGD("TEST", std::to_string(size));
                        Res<uchar_t> res { rom.Read(cpu.state.pc++) };
                        None(res);
                        err = cpu.Push(Get(res));
                    }

                    return err;
                }

                case OpCodes::raws:
                {
                    // raw <address> <size>
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    sysbit_t addr { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};
                    cpu.state.pc += 4;

                    Res<Slice> sRes2 { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes2);
                    sysbit_t size { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};
                    cpu.state.pc += 4;

                    Res<Slice> res { rom.ReadSome(addr, size) };
                    None(res);
                    return cpu.PushSome(Get(res));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Invert: block(
            switch (OpCodes(op))
            {
                case OpCodes::invt:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes);
                    sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};
                    System::ErrorCode err { cpu.PopSome(4) };

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    top32 = ~top32;

                    char data[4];
                    BytesFromInteger(top32, data);
                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::inve:
                {
                    Res<char> res { ram.Read(cpu.state.sp-1) };
                    None(res);
                    uchar_t byte { static_cast<uchar_t>(Get(res)) };
                    System::ErrorCode err { cpu.Pop() };

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    byte = ~byte;
                    return cpu.Push(byte);
                }

                case OpCodes::invr:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);
                    RegisterModeFlags regMode { Get(modeRes) };

                    if (Is8BitReg(regMode))
                    {
                        Res<uchar_t*> regRes { GetRegister8Bit(regMode, cpu.state) };
                        None(regRes);
                        uchar_t& reg { *Get(regRes) };
                        reg = ~reg;
                    }
                    else
                    {
                        Res<sysbit_t*> regRes { GetRegister32Bit(regMode, cpu.state) };
                        None(regRes);
                        sysbit_t& reg { *Get(regRes) };
                        reg = ~reg;
                    }

                    return System::ErrorCode::Ok;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_InvertSafe: block(
            switch (OpCodes(op))
            {
                case OpCodes::invst:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes);
                    sysbit_t top32 { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    top32 = ~top32;

                    char data[4];
                    BytesFromInteger(top32, data);
                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::invse:
                {
                    Res<char> res { ram.Read(cpu.state.sp-1) };
                    None(res);
                    uchar_t byte { static_cast<uchar_t>(Get(res)) };

                    byte = ~byte;
                    return cpu.Push(byte);
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Compare: block(
            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            const uchar_t compressedModes { Get(modeRes) };

            cpu.state.pc++;

            Numo numMode {
                static_cast<uchar_t>(compressedModes >> 5)
            };
            const uchar_t compareMode {
                static_cast<const uchar_t>(compressedModes & 0x1F)
            };

            switch (OpCodes(op))
            {
                case OpCodes::cmp:
                {
                    if (numMode == Numo::UInt)
                    {
                        Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                        None(sRes);
                        sysbit_t int1 { IntegerFromBytes<sysbit_t>(
                            Get(sRes).data
                        )};

                        Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                        None(sRes2);
                        sysbit_t int2 { IntegerFromBytes<sysbit_t>(
                            Get(sRes2).data
                        )};

                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(int1, int2, compareMode);
                    }
                    else if (numMode == Numo::Float)
                    {
                        Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                        None(sRes);
                        float float1 { FloatFromBytes(
                            Get(sRes).data
                        )};

                        Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                        None(sRes2);
                        float float2 { FloatFromBytes(
                            Get(sRes2).data
                        )};

                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(float1, float2, compareMode);
                    }
                    else if (numMode == Numo::Int)
                    {
                        Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                        None(sRes);
                        int int1 { IntegerFromBytes<int32_t>(
                            Get(sRes).data
                        )};

                        Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                        None(sRes2);
                        int int2 { IntegerFromBytes<int32_t>(
                            Get(sRes2).data
                        )};

                        cpu.state.sp -= 8;
                        cpu.state.bl = CompareVarious(int1, int2, compareMode);
                    }
                    else if (numMode == Numo::UByte)
                    {
                        Res<char> res { ram.Read(cpu.state.sp-2) };
                        None(res);
                        uchar_t byte1 { static_cast<uchar_t>(Get(res)) };

                        res = ram.Read(cpu.state.sp-1);
                        None(res);
                        uchar_t byte2 { static_cast<uchar_t>(Get(res)) };

                        cpu.state.sp -= 2;
                        cpu.state.bl = CompareVarious(byte1, byte2, compareMode);
                    }
                    else
                    {
                        Res<char> byte1 { ram.Read(cpu.state.sp-2) };
                        None(byte1);

                        Res<char> byte2 { ram.Read(cpu.state.sp-1) };
                        None(byte2);

                        cpu.state.sp -= 2;
                        cpu.state.bl = CompareVarious(Get(byte1), Get(byte2), compareMode);
                    }
                    return System::ErrorCode::Ok;
                }

                case OpCodes::cmpr:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc++) };
                    None(modeRes);
                    RegisterModeFlags reg1mode { Get(modeRes) };

                    modeRes = rom.Read(cpu.state.pc++);
                    None(modeRes);
                    RegisterModeFlags reg2mode { Get(modeRes) };

                    sysbit_t reg1;
                    if (Is8BitReg(reg1mode))
                    {
                        Res<uchar_t*> regRes { GetRegister8Bit(reg1mode, cpu.state) }; 
                        None(regRes);
                        reg1 = static_cast<sysbit_t>(*Get(regRes));
                    }
                    else
                    {
                        Res<sysbit_t*> regRes { GetRegister32Bit(reg1mode, cpu.state) }; 
                        None(regRes);
                        reg1 = *Get(regRes);
                    }

                    sysbit_t reg2;
                    if (Is8BitReg(reg2mode))
                    {
                        Res<uchar_t*> regRes { GetRegister8Bit(reg2mode, cpu.state) }; 
                        None(regRes);
                        reg1 = static_cast<sysbit_t>(*Get(regRes));
                    }
                    else
                    {
                        Res<sysbit_t*> regRes { GetRegister32Bit(reg2mode, cpu.state) }; 
                        None(regRes);
                        reg1 = *Get(regRes);
                    }

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

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PopInstruction: block(
            switch (OpCodes(op))
            {
                case OpCodes::pope:
                    return cpu.Pop();
                case OpCodes::popt:
                    return cpu.PopSome(4);
                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Jump: block(
            switch (OpCodes(op))
            {
                case OpCodes::jmpr:
                {
                    Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
                    None(modeRes);

                    Res<sysbit_t*> regRes { GetRegister32Bit( RegisterModeFlags(Get(modeRes)), cpu.state) };
                    None(regRes);
                    sysbit_t address { *Get(regRes) };


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
                }

                case OpCodes::jmp:
                {
                    Res<Slice> addressRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(addressRes);
                    sysbit_t address { IntegerFromBytes<sysbit_t>(
                        Get(addressRes).data
                    )};

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
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SwapRange: block(
            // swr <size: sysbit>
            Res<Slice> sizeRes { rom.ReadSome(cpu.state.pc, 4) };
            None(sizeRes);
            sysbit_t size { IntegerFromBytes<sysbit_t>(
                Get(sizeRes).data
            )};

            System::ErrorCode err { System::ErrorCode::Ok };
            for (
                sysbit_t midpoint = cpu.state.sp-size;
                err == System::ErrorCode::Ok && size > 0;
                size--
            )
            {
                Res<char> tmp { ram.Read(cpu.state.sp-size) };
                None(tmp);

                Res<char> res { ram.Read(midpoint-size) };
                None(res);

                err = err == System::ErrorCode::Ok ? ram.Write(
                    cpu.state.sp-size,
                    Get(res)
                ) : err;

                err = err == System::ErrorCode::Ok ?
                    ram.Write(midpoint-size, Get(tmp))
                    : err;
            }

            cpu.state.pc += 4;
        )

        op_DuplicateRange: block(
            // dur <size: sysbit>
            Res<Slice> res { rom.ReadSome(cpu.state.pc, 4) };
            None(res);
            sysbit_t size { IntegerFromBytes<sysbit_t>(
                Get(res).data
            )};

            System::ErrorCode err { System::ErrorCode::Ok };
            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-size, size) };
            None(sRes);
            cpu.PushSome(Get(sRes));

            cpu.state.pc += 4;
        )

        op_Repeat: block(
            // rep <compressed(mem/num)> <count> <val>
            MemoryModeFlags memMode;
            NumericModeFlags numMode;

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            const uchar_t compressed { Get(modeRes) };

            memMode = MemoryModeFlags(compressed >> 4);
            numMode = NumericModeFlags(compressed & 0b00001111);

            cpu.state.pc++;
            Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
            None(sRes);
            const sysbit_t count { IntegerFromBytes<sysbit_t>(
                Get(sRes).data
            )};
            cpu.state.pc += 4;

            Res<Slice> valueDataRes {
                rom.ReadSome(cpu.state.pc, ByteSize(numMode))
            };
            None(valueDataRes);
            cpu.state.pc += ByteSize(numMode);

            sysbit_t address;
            if (memMode == MemoryModeFlags::Heap)
                address = cpu.state.ebx;
            else
            {
                if (cpu.state.sp + count*ByteSize(numMode) > ram.StackSize()) [[unlikely]]
                {
                    LOGE(
                        System::LogLevel::High,
                        "In "" instruction rep. Can't push onto stack, it's full"
                    );
                    return System::ErrorCode::StackOverflow;
                }

                address = cpu.state.sp;
                cpu.state.sp += count*ByteSize(numMode);
            }

            System::ErrorCode err { System::ErrorCode::Ok };
            Slice valueData { Get(valueDataRes) };
            for (sysbit_t i = 0; (i < count) && (err == System::ErrorCode::Ok); i++, address += ByteSize(numMode))
                err = err == System::ErrorCode::Ok ?
                    ram.WriteSome(address, valueData) :
                    err;

            return err;
        )

        op_Allocate: block(
            Res<sysbit_t> address { ram.Allocate(cpu.state.ecx) };
            None(address);
            cpu.state.ebx = Get(address);
        )

        op_PowRegister: block(
            float base;
            float power;
            OpCodes op { op };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc++) };
            None(modeRes);
            RegisterModeFlags reg1 { Get(modeRes) };

            modeRes = rom.Read(cpu.state.pc++);
            None(modeRes);
            RegisterModeFlags reg2 { Get(modeRes) };

            switch (op)
            {
                case OpCodes::powri:
                {
                    Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                    None(regRes);
                    base = static_cast<float>(*Get(regRes));

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    power = static_cast<float>(*Get(regRes));

                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) = res;
                    break;
                }

                case OpCodes::powrf:
                {
                    char data[4];

                    Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                    None(regRes);
                    BytesFromInteger<sysbit_t>(*Get(regRes), data);
                    base = FloatFromBytes(data);

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    BytesFromInteger<sysbit_t>(*Get(regRes), data);
                    power = FloatFromBytes(data);

                    float res { std::pow(base, power) };
                    BytesFromFloat(res, data);

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) = IntegerFromBytes<sysbit_t>(data);
                    break;
                }

                case OpCodes::powrb:
                {
                    Res<uchar_t*> regRes { GetRegister8Bit(reg1, cpu.state) };
                    None(regRes);
                    base = static_cast<float>(*Get(regRes));

                    regRes = GetRegister8Bit(reg2, cpu.state);
                    None(regRes);
                    power = static_cast<float>(*Get(regRes));

                    uchar_t res { static_cast<uchar_t>(std::pow(base, power)) };

                    regRes = GetRegister8Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) = res;
                    break;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PowStack: block(
            float base;
            float power;
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::powsi:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    base = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    ));

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    power = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    ));

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::powsf:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    base = FloatFromBytes(Get(sRes).data);

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    power = FloatFromBytes(Get(sRes2).data);

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    float res { std::pow(base, power) };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::powsb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-2) };
                    None(res);
                    base = static_cast<float>(Get(res));

                    res = ram.Read(cpu.state.sp-1);
                    None(res);
                    power = static_cast<float>(Get(res));

                    err = cpu.PopSome(2);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    return cpu.Push(static_cast<uchar_t>(std::pow(base, power)));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_PowConst: block(
            float base;
            float power;
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::powi:
                {
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    base = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    ));

                    Res<Slice> sRes2 { rom.ReadSome(cpu.state.pc+4, 4) };
                    None(sRes2);
                    power = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    ));
                    cpu.state.pc += 8;

                    sysbit_t res { static_cast<sysbit_t>(std::pow(base, power)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::powf:
                {
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    base = FloatFromBytes(Get(sRes).data);

                    Res<Slice> sRes2 { rom.ReadSome(cpu.state.pc+4, 4) };
                    None(sRes2);
                    power = FloatFromBytes(Get(sRes2).data);

                    cpu.state.pc += 8;

                    float res { std::pow(base, power) };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::powb:
                {
                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    base = static_cast<float>(Get(res));

                    res = rom.Read(cpu.state.pc+1);
                    None(res);
                    power = static_cast<float>(Get(res));
                    cpu.state.pc += 2;

                    return cpu.Push(static_cast<uchar_t>(std::pow(base, power)));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtConst: block(
            float num;
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::sqri:
                {
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    num = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    ));
                    cpu.state.pc += 4;

                    sysbit_t res { static_cast<sysbit_t>(std::sqrt(num)) };
                    char data[4];
                    BytesFromInteger(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::sqrf:
                {
                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
                    None(sRes);
                    num = FloatFromBytes(
                        Get(sRes).data
                    );
                    cpu.state.pc += 4;

                    float res { std::sqrt(num) };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::sqrb:
                {
                    Res<uchar_t> res { rom.Read(cpu.state.pc) };
                    None(res);
                    num = static_cast<float>(Get(res));
                    cpu.state.pc++;

                    return cpu.Push(static_cast<uchar_t>(std::sqrt(num)));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtRegister: block(
            float num;
            OpCodes op { op };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc++) };
            None(modeRes);
            RegisterModeFlags reg { Get(modeRes) };

            switch (op)
            {
                case OpCodes::sqrri:
                {
                    Res<sysbit_t*> regRes { GetRegister32Bit(reg, cpu.state) };
                    None(regRes);
                    num = static_cast<float>(*Get(regRes));
                    cpu.state.eax = static_cast<sysbit_t>(std::sqrt(num));
                    break;
                }

                case OpCodes::sqrrf:
                {
                    char data[4];

                    Res<sysbit_t*> regRes { GetRegister32Bit(reg, cpu.state) };
                    None(regRes);
                    BytesFromInteger<sysbit_t>(*Get(regRes), data);
                    num = FloatFromBytes(data);

                    float res { std::sqrt(num) };
                    BytesFromFloat(res, data);
                    cpu.state.eax = IntegerFromBytes<sysbit_t>(data);
                    break;
                }

                case OpCodes::sqrrb:
                {
                    Res<uchar_t*> regRes { GetRegister8Bit(reg, cpu.state) };
                    None(regRes);
                    num = static_cast<float>(*Get(regRes));
                    cpu.state.al = static_cast<uchar_t>(std::sqrt(num));
                    break;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_SqrtStack: block(
            float num;
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::sqrsi:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes);
                    num = static_cast<float>(IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    ));

                    err = cpu.PopSome(4);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    sysbit_t res { static_cast<sysbit_t>(std::sqrt(num)) };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::sqrsf:
                {
                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes);
                    num = FloatFromBytes(Get(sRes).data);

                    err = cpu.PopSome(4);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    float res { std::sqrt(num) };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::sqrsb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-1) };
                    None(res);
                    num = static_cast<float>(Get(res));

                    err = cpu.Pop();

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    return cpu.Push(static_cast<uchar_t>(std::sqrt(num)));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_ConditionalJump: block(
            OpCodes op { op };
            sysbit_t address;

            if (cpu.state.bl == 0)
                address = cpu.state.pc + ((op == OpCodes::cnd) ? 4 : 1);
            else if (op == OpCodes::cnd)
            {
                Res<Slice> res { rom.ReadSome(cpu.state.pc, 4) };
                None(res);
                address = IntegerFromBytes<sysbit_t>(
                    Get(res).data
                );
            }
            else if (op == OpCodes::cndr)
            {
                Res<uchar_t> res { rom.Read(cpu.state.pc) };
                None(res);
                
                Res<sysbit_t*> regRes { GetRegister32Bit(RegisterModeFlags(Get(res)), cpu.state) };
                None(regRes);
                address = *Get(regRes);
            }
            else [[unlikely]]
                return System::ErrorCode::InvalidInstruction;



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


        op_CallFunc: block(
            if (cpu.state.sp < cpu.state.bl) [[unlikely]]
                return System::ErrorCode::RAMAccessError;

            OpCodes op { op };
            sysbit_t address;
            if (op == OpCodes::cal)
            {
                Res<Slice> res { rom.ReadSome(cpu.state.pc, 4) };
                None(res);
                address = IntegerFromBytes<sysbit_t>(
                    Get(res).data
                );
            }
            if (op == OpCodes::calr)
            {
                Res<uchar_t> res { rom.Read(cpu.state.pc) };
                None(res);

                Res<sysbit_t*> regRes { GetRegister32Bit( RegisterModeFlags(Get(res)), cpu.state) };
                None(regRes);
                address = *Get(regRes);
            }

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

            if (cpu.state.sp+8+cpu.state.bl > ram.StackSize()) [[unlikely]]
            {
                LOGE( System::LogLevel::High, "Can't push parameters.");
                return System::ErrorCode::StackOverflow;
            }

            //const Slice params { ram.ReadSome(cpu.state.sp-cpu.state.bl, cpu.state.bl) };
            //Slice params (&ram+cpu.state.sp-cpu.state.bl, cpu.state.bl);
            //cpu.state.sp += 8 - cpu.state.bl;
            //ram.WriteSome(cpu.state.sp, params);
            //cpu.state.sp -= cpu.state.bl + 8;
            cpu.state.sp -= cpu.state.bl;
            std::memmove(ram&(cpu.state.sp+8), ram&(cpu.state.sp), cpu.state.bl);

            // Store bp
            char data[4];
            BytesFromInteger(cpu.state.bp, data);
            System::ErrorCode err = cpu.PushSome(Get(Slice::New(data, 4)));

            if (err != System::ErrorCode::Ok) [[unlikely]]
                return err;

            // Store pc
            BytesFromInteger(cpu.state.pc + (op == OpCodes::cal ? 4 : 1), data);
            err = cpu.PushSome(Get(Slice::New(data, 4)));

            if (err != System::ErrorCode::Ok) [[unlikely]]
                return err;

            // Change pc and bp
            cpu.state.pc = address;
            cpu.state.bp = sysbit_t{cpu.state.sp};

            cpu.state.sp += cpu.state.bl;
        )

        op_MulStack: block(
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::muli:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    sysbit_t res { lhs * rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::mulf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    float lhs { FloatFromBytes(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float rhs { FloatFromBytes(Get(sRes2).data) };

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    float res { lhs * rhs };
                    char data[4];
                    BytesFromFloat(res, data);
                    
                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::mulb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-2) };
                    None(res);
                    uchar_t lhs { static_cast<uchar_t>(Get(res)) };

                    res = ram.Read(cpu.state.sp-1);
                    None(res);
                    uchar_t rhs { static_cast<uchar_t>(Get(res)) };

                    err = cpu.PopSome(2);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    return cpu.Push(static_cast<uchar_t>(lhs * rhs));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_MulRegister: block(
            OpCodes op { op };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc++) };
            None(modeRes);
            RegisterModeFlags reg1 { Get(modeRes) };

            modeRes = rom.Read(cpu.state.pc++);
            None(modeRes);
            RegisterModeFlags reg2 { Get(modeRes) };

            switch (op)
            {
                case OpCodes::mulri:
                {
                    Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                    None(regRes);
                    sysbit_t lhs { *Get(regRes) };

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) *= lhs;

                    break;
                }

                case OpCodes::mulrf:
                {
                    char data[4];

                    Res<sysbit_t*> regRes { GetRegister32Bit(reg1, cpu.state) };
                    None(regRes);
                    BytesFromInteger<sysbit_t>(*Get(regRes), data);
                    float lhs { FloatFromBytes(data) };

                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    BytesFromInteger<sysbit_t>(*Get(regRes), data);
                    float rhs { FloatFromBytes(data) };

                    BytesFromFloat(lhs * rhs, data);
                    
                    regRes = GetRegister32Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) = IntegerFromBytes<sysbit_t>(data);

                    break;
                }

                case OpCodes::mulrb:
                {
                    Res<uchar_t*> regRes { GetRegister8Bit(reg1, cpu.state) };
                    None(regRes);
                    uchar_t lhs { *Get(regRes) };

                    regRes = GetRegister8Bit(reg2, cpu.state);
                    None(regRes);
                    *Get(regRes) *= lhs;

                    break;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_MulSafe: block(
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::mulsi:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    sysbit_t res { lhs * rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::mulsf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    float lhs { FloatFromBytes(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float rhs { FloatFromBytes(Get(sRes2).data) };

                    float res { lhs * rhs };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::mulsb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-2) };
                    None(res);
                    uchar_t lhs { static_cast<uchar_t>(Get(res)) };

                    res = ram.Read(cpu.state.sp-1);
                    None(res);
                    uchar_t rhs { static_cast<uchar_t>(Get(res)) };

                    return cpu.Push(static_cast<uchar_t>(lhs * rhs));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivStack: block(
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::divi:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    sysbit_t res { lhs / rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::divf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    float lhs { FloatFromBytes(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float rhs { FloatFromBytes(Get(sRes2).data) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(8);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    float res { lhs / rhs };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::divb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-2) };
                    None(res);
                    uchar_t lhs { static_cast<uchar_t>(Get(res)) };

                    res = ram.Read(cpu.state.sp-1);
                    None(res);
                    uchar_t rhs { static_cast<uchar_t>(Get(res)) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    err = cpu.PopSome(2);

                    if (err != System::ErrorCode::Ok) [[unlikely]]
                        return err;

                    return cpu.Push(static_cast<uchar_t>(lhs / rhs));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivRegister: block(
            OpCodes op { op };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc++) };
            None(modeRes);
            RegisterModeFlags reg1 { Get(modeRes) };

            modeRes = rom.Read(cpu.state.pc++);
            None(modeRes);
            RegisterModeFlags reg2 { Get(modeRes) };

            switch (op)
            {
                case OpCodes::divri:
                {
                    Res<sysbit_t*> res { GetRegister32Bit(reg1, cpu.state) };
                    None(res);
                    sysbit_t lhs { *Get(res) };

                    res = GetRegister32Bit(reg2, cpu.state) ;
                    None(res);
                    sysbit_t& rhs { *Get(res) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    rhs = lhs / rhs;
                    break;
                }

                case OpCodes::divrf:
                {
                    char data[4];

                    Res<sysbit_t*> res { GetRegister32Bit(reg1, cpu.state) };
                    None(res);
                    BytesFromInteger<sysbit_t>(*Get(res), data);
                    float lhs { FloatFromBytes(data) };

                    res = GetRegister32Bit(reg2, cpu.state);
                    None(res);
                    BytesFromInteger<sysbit_t>(*Get(res), data);
                    float rhs { FloatFromBytes(data) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    BytesFromFloat(lhs / rhs, data);

                    res = GetRegister32Bit(reg2, cpu.state);
                    None(res);
                    *Get(res) = IntegerFromBytes<sysbit_t>(data);

                    break;
                }

                case OpCodes::divrb:
                {
                    Res<uchar_t*> res { GetRegister8Bit(reg1, cpu.state) };
                    None(res);
                    uchar_t lhs { *Get(res) };

                    res = GetRegister8Bit(reg2, cpu.state);
                    None(res);
                    uchar_t& rhs { *Get(res) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    rhs = lhs / rhs;
                    break;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_DivSafe: block(
            System::ErrorCode err;

            switch (OpCodes(op))
            {
                case OpCodes::divsi:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    sysbit_t lhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes).data
                    )};

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    sysbit_t rhs { IntegerFromBytes<sysbit_t>(
                        Get(sRes2).data
                    )};

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    sysbit_t res { lhs / rhs };
                    char data[4];
                    BytesFromInteger<sysbit_t>(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::divsf:
                {
                    if (cpu.state.sp < 4) [[unlikely]]
                        return System::ErrorCode::RAMAccessError;

                    Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                    None(sRes);
                    float lhs { FloatFromBytes(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                    None(sRes2);
                    float rhs { FloatFromBytes(Get(sRes2).data) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    float res { lhs / rhs };
                    char data[4];
                    BytesFromFloat(res, data);

                    return cpu.PushSome(Get(Slice::New(data, 4)));
                }

                case OpCodes::divsb:
                {
                    Res<char> res { ram.Read(cpu.state.sp-2) };
                    None(res);
                    uchar_t lhs { static_cast<uchar_t>(Get(res)) };

                    res = ram.Read(cpu.state.sp-1);
                    None(res);
                    uchar_t rhs { static_cast<uchar_t>(Get(res)) };

                    if (rhs == 0) [[unlikely]]
                        return System::ErrorCode::DivideByZero;

                    return cpu.Push(static_cast<uchar_t>(lhs / rhs));
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )

        op_Return: block(
            // callstack is:
            //  bp 4bytes
            //  pc 4bytes
            // current bp is AFTER the callstack

            if (cpu.state.sp - cpu.state.bp < cpu.state.bl) [[unlikely]]
                return System::ErrorCode::StackUnderflow;

            Res<Slice> sRes { ram.ReadSome(cpu.state.bp - 8, 4) };
            None(sRes);
            sysbit_t bpToReturnTo { IntegerFromBytes<sysbit_t>(
                Get(sRes).data
            )};

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp - 4, 4) };
            None(sRes2);
            sysbit_t pcToReturnTo { IntegerFromBytes<sysbit_t>(
                Get(sRes2).data
            )};

            System::ErrorCode err;
            
            if (cpu.state.bl != 0)
            {
                Res<Slice> returnValues { ram.ReadSome(
                    cpu.state.sp - cpu.state.bl,
                    cpu.state.bl
                )};
                None(returnValues);

                err = cpu.PopSome(cpu.state.sp - cpu.state.bp + 8);

                if (err != System::ErrorCode::Ok) [[unlikely]]
                    return err;

                err = cpu.PushSome(Get(returnValues));
            }
            else
                err = cpu.PopSome(cpu.state.sp - cpu.state.bp + 8);

            cpu.state.bp = bpToReturnTo;
            cpu.state.pc = pcToReturnTo;
            
            return err;
        )

        op_Deallocate: block(
            if (cpu.state.ebx < 0 || ram.Size() <= cpu.state.ebx) [[unlikely]]
                return System::ErrorCode::RAMAccessError;

            return ram.Deallocate(cpu.state.ebx, cpu.state.ecx);
        )

        op_Sub32: block(
            sysbit_t rhs;
            sysbit_t lhs;

            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes);
            rhs = IntegerFromBytes<sysbit_t>(Get(sRes).data);
            cpu.PopSome(4);

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes2);
            lhs = IntegerFromBytes<sysbit_t>(Get(sRes2).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromInteger(lhs-rhs, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_SubFloat: {
            float rhs;
            float lhs;

            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes);
            rhs = FloatFromBytes(Get(sRes).data);
            cpu.PopSome(4);

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes2);
            lhs = FloatFromBytes(Get(sRes2).data);
            cpu.PopSome(4);

            char data[4];
            BytesFromFloat<char>(lhs-rhs, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        }

        op_Sub8: {
            Res<char> rhs { ram.Read(cpu.state.sp-1) };
            None(rhs);
            cpu.Pop();

            Res<char> lhs { ram.Read(cpu.state.sp-1) };
            None(lhs);
            cpu.Pop();

            return cpu.Push(static_cast<uchar_t>(Get(lhs)-static_cast<uchar_t>(Get(rhs))));
        }

        op_SubReg: block(
            OpCodes op { op };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            RegisterModeFlags regLhs { Get(modeRes) };

            modeRes = rom.Read(cpu.state.pc+1);
            None(modeRes);
            RegisterModeFlags regRhs { Get(modeRes) };

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
            ) [[unlikely]]
            {
                LOGE(System::LogLevel::High,
                    "PC: ", std::to_string(cpu.state.pc-1),
                    " ", OpCodesString(op),
                    " ", std::to_string((int)regLhs),
                    " ", std::to_string((int)regRhs),
                    " Given registers are not compatible with given numeric type."
                );
                return System::ErrorCode::InvalidSpecifier;
            }

            cpu.state.pc+=2;

            if (Is8BitReg(regLhs))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(regLhs, cpu.state) };
                None(regRes);
                uchar_t regLhsRef { *Get(regRes) };

                regRes = GetRegister8Bit(regRhs, cpu.state);
                None(regRes);
                uchar_t& regRhsRef { *Get(regRes) };

                regRhsRef = regLhsRef - regRhsRef;
            }
            else if (op == OpCodes::subrf)
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(regLhs, cpu.state) };
                None(regRes);
                sysbit_t regLhsRef { *Get(regRes) };

                regRes = GetRegister32Bit(regRhs, cpu.state);
                None(regRes);
                sysbit_t& regRhsRef { *Get(regRes) };

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
                Res<sysbit_t*> regRes { GetRegister32Bit(regLhs, cpu.state) };
                None(regRes);
                sysbit_t regLhsRef { *Get(regRes) };

                regRes = GetRegister32Bit(regRhs, cpu.state);
                None(regRes);
                sysbit_t& regRhsRef { *Get(regRes) };

                regRhsRef = regLhsRef - regRhsRef;
            }
        )

        op_SubSafe32: block(
            sysbit_t rhs;
            sysbit_t lhs;

            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes);
            rhs = IntegerFromBytes<sysbit_t>(Get(sRes).data);

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-8, 4) };
            None(sRes2);
            lhs = IntegerFromBytes<sysbit_t>(Get(sRes2).data);

            char data[4];
            BytesFromInteger(lhs-rhs, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_SubSafeFloat: block(
            float rhs;
            float lhs;

            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes);
            rhs = FloatFromBytes(Get(sRes).data);

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-8, 4) };
            None(sRes2);
            lhs = FloatFromBytes(Get(sRes2).data);

            char data[4];
            BytesFromFloat<char>(lhs-rhs, data);

            return cpu.PushSome(Get(Slice::New(data, 4)));
        )

        op_SubSafe8: block(
            Res<char> rhs { ram.Read(cpu.state.sp-1) };
            None(rhs);

            Res<char> lhs { ram.Read(cpu.state.sp-2) };
            None(lhs);

            return cpu.Push(static_cast<uchar_t>(Get(lhs))-static_cast<uchar_t>(Get(rhs)));
        )

        op_IncrementLocal: block(
            Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
            None(sRes);
            const sysbit_t index { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

            if (index < 0) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Index can't be negative in ", OpCodesString(op));
                return System::ErrorCode::IndexOutOfBounds;
            }
            switch (OpCodes(op))
            {
                case OpCodes::incli:
                {
                    // incli <index> <constant>
                    char data[4];

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc+4, 4) };
                    None(sRes);
                    const sysbit_t constant { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp+index, 4) };
                    None(sRes2);
                    const sysbit_t local { IntegerFromBytes<sysbit_t>(Get(sRes2).data) };

                    BytesFromInteger(constant+local, data);
                    System::ErrorCode err = ram.WriteSome(cpu.state.bp+index, Get(Slice::New(data, 4)));

                    if (err == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc += 8;

                    return err;
                }
                case OpCodes::inclf:
                {
                    // inclf <index> <constant>
                    char data[4];

                    Res<Slice> sRes { rom.ReadSome(cpu.state.pc+4, 4) };
                    None(sRes);
                    const float constant { FloatFromBytes(Get(sRes).data) };

                    Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp+index, 4) };
                    None(sRes2);
                    const float local { FloatFromBytes(Get(sRes2).data) };

                    BytesFromFloat(constant+local, data);
                    System::ErrorCode err = ram.WriteSome(cpu.state.bp+index, Get(Slice::New(data, 4)));

                    if (err == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc += 8;

                    return err;
                }
                case OpCodes::inclb:
                {
                    // inclb <index> <constant>
                    Res<uchar_t> uRes { rom.Read(cpu.state.pc+4) };
                    None(uRes);
                    const uchar_t constant { Get(uRes) };

                    Res<char> cRes { ram.Read(cpu.state.bp+index) };
                    None(cRes);
                    const uchar_t local { static_cast<uchar_t>(Get(cRes)) };

                    System::ErrorCode errcx = ram.Write(cpu.state.bp+index, constant+local);

                    if (errcx == System::ErrorCode::Ok) [[likely]]
                        cpu.state.pc += 5;

                    return errcx;
                }

                default: [[unlikely]]
                    return System::ErrorCode::InvalidInstruction;
            }
        )


    op_ReadLocal: block(
            // rdlt <index>
            // rdle <index>
            sysbit_t size {
                static_cast<sysbit_t>
                (op == (uchar_t)OpCodes::rdlt ? 4 : 1)
            };

            Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
            None(sRes);
            sysbit_t index { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

            Res<Slice> values { ram.ReadSome(cpu.state.bp+index, size) };
            None(values);

            const System::ErrorCode errc { cpu.PushSome(Get(values)) };

            if (errc == System::ErrorCode::Ok) [[likely]]
                cpu.state.pc += 4;

            return errc;
        )


    op_CompareJump: block(
            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            const uchar_t compressedModes { Get(modeRes) };

            Numo numMode { static_cast<uchar_t>(compressedModes >> 5) };
            const uchar_t compareMode { static_cast<const uchar_t>(compressedModes & 0b00011111) };

            if (numMode == Numo::UInt)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                None(sRes);
                sysbit_t int1 { IntegerFromBytes<sysbit_t>( Get(sRes).data)};

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) }; None(sRes2);
                sysbit_t int2 { IntegerFromBytes<sysbit_t>( Get(sRes2).data)};

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::Float)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                None(sRes);
                float float1 { FloatFromBytes( Get(sRes).data)};

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                None(sRes2);
                float float2 { FloatFromBytes( Get(sRes2).data)};

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(float1, float2, compareMode);
            }
            else if (numMode == Numo::Int)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
                None(sRes);
                int int1 { IntegerFromBytes<int32_t>( Get(sRes).data)};

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
                None(sRes2);
                int int2 { IntegerFromBytes<int32_t>( Get(sRes2).data)};

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::UByte)
            {
                Res<char> byte1 { ram.Read(cpu.state.sp-2) };
                None(byte1);

                Res<char> byte2 { ram.Read(cpu.state.sp-1) };
                None(byte2);

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(static_cast<uchar_t>(Get(byte1)), static_cast<uchar_t>(Get(byte2)), compareMode);
            }
            else
            {
                Res<char> byte1 { ram.Read(cpu.state.sp-2) };
                None(byte1);

                Res<char> byte2 { ram.Read(cpu.state.sp-1) };
                None(byte2);

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(Get(byte1), Get(byte2), compareMode);
            }

            sysbit_t address;

            if (cpu.state.bl == 0)
                address = cpu.state.pc + 5;
            else
            {
                Res<Slice> sRes { rom.ReadSome(cpu.state.pc+1, 4) };
                None(sRes);
                address = IntegerFromBytes<sysbit_t>( Get(sRes).data);
            }

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
            Res<uchar_t> readRes { rom.Read(cpu.state.pc) };
            None(readRes);
            const uchar_t compressedModes { Get(readRes) };

            cpu.state.pc++;

            const Numo numMode { static_cast<uchar_t>(compressedModes >> 5) };
            const uchar_t compareMode { static_cast<const uchar_t>(compressedModes & 0x1F) };

            Res<Slice> sRes { rom.ReadSome(cpu.state.pc, sizeof(sysbit_t)) };
            None(sRes);
            const sysbit_t idx1 { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

            Res<Slice> sRes2 { rom.ReadSome(cpu.state.pc+sizeof(sysbit_t), sizeof(sysbit_t)) };
            None(sRes2);
            const sysbit_t idx2 { IntegerFromBytes<sysbit_t>(Get(sRes2).data) };

            cpu.state.pc += 2*sizeof(sysbit_t);

            if (numMode == Numo::UInt)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.bp+idx1, 4) };
                None(sRes);
                sysbit_t int1 { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp+idx2, 4) };
                None(sRes2);
                sysbit_t int2 { IntegerFromBytes<sysbit_t>(Get(sRes2).data) };

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::Float)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.bp+idx1, 4) };
                None(sRes);
                float float1 { FloatFromBytes(Get(sRes).data) };

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp+idx2, 4) };
                None(sRes2);
                float float2 { FloatFromBytes(Get(sRes2).data) };

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(float1, float2, compareMode);
            }
            else if (numMode == Numo::Int)
            {
                Res<Slice> sRes { ram.ReadSome(cpu.state.bp+idx1, 4) };
                None(sRes);
                int int1 { IntegerFromBytes<int32_t>(Get(sRes).data) };

                Res<Slice> sRes2 { ram.ReadSome(cpu.state.bp+idx2, 4) };
                None(sRes2);
                int int2 { IntegerFromBytes<int32_t>(Get(sRes2).data) };

                cpu.state.sp -= 8;
                cpu.state.bl = CompareVarious(int1, int2, compareMode);
            }
            else if (numMode == Numo::UByte)
            {
                Res<char> byte1 { ram.Read(cpu.state.bp+idx1) };
                None(byte1);

                Res<char> byte2 { ram.Read(cpu.state.bp+idx1) };
                None(byte2);

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(static_cast<uchar_t>(Get(byte1)), static_cast<uchar_t>(Get(byte2)), compareMode);
            }
            else
            {
                Res<char> byte1 { ram.Read(cpu.state.bp+idx1) };
                None(byte1);

                Res<char> byte2 { ram.Read(cpu.state.sp+idx2) };
                None(byte2);

                cpu.state.sp -= 2;
                cpu.state.bl = CompareVarious(Get(byte1), Get(byte2), compareMode);
            }

            return System::ErrorCode::Ok;
        )

        op_SetFlag: block(
            Res<uchar_t> compressedRes { rom.Read(cpu.state.pc) };
            None(compressedRes);
            uchar_t compressed { Get(compressedRes) };

            uchar_t flagToSet { static_cast<uchar_t>(compressed >> 4) };
            uchar_t value { static_cast<uchar_t>(compressed & 0x0F) };
            cpu.state.pc++;

            if (value == 1)
                cpu.state.flg |= (1 << flagToSet);
            else
                cpu.state.flg &= ~(1 << flagToSet);
        )

        op_SysCall: block(
            // &bl is still set,
            // values are pushed to stack then syscall is made.
            // values are eaten.
            // sys <size:string>
            Res<const char*> addrRes { rom&cpu.state.pc };
            None(addrRes);
            const char* strptr { Get(addrRes)+sizeof(sysbit_t) };

            Res<Slice> sRes { rom.ReadSome(cpu.state.pc, 4) };
            None(sRes);
            sysbit_t size { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

            std::string_view signatureStr(strptr, size);

            if (signatureStr == "CSR_Println")
            {
                cpu.state.pc += size + sizeof(sysbit_t);

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                None(sRes);
                sysbit_t vmAddr { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                Res<Slice> sRes2 { ram.ReadSome(vmAddr, 4) };
                None(sRes2);
                size = { IntegerFromBytes<sysbit_t>(Get(sRes2).data) };

                strptr = static_cast<const char*>(GetRealAddress(vmAddr+4));
                std::cout.write(strptr, size);
                std::cout.put('\n');
                cpu.state.sp -= cpu.state.bl;
                cpu.state.bl = 0;
            }
            else if (signatureStr == "CSR_Print")
            {
                cpu.state.pc += size + sizeof(sysbit_t);

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp-4, 4) };
                None(sRes);
                sysbit_t vmAddr { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                Res<Slice> sRes2 { ram.ReadSome(vmAddr, 4) };
                None(sRes2);
                size = { IntegerFromBytes<sysbit_t>(Get(sRes2).data) };

                strptr = static_cast<const char*>(GetRealAddress(vmAddr+4));
                std::cout.write(strptr, size);
                cpu.state.sp -= cpu.state.bl;
                cpu.state.bl = 0;
            }
            else if (signatureStr == "CSR_U32ToFloat")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                cpu.state.sp -= 4;

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp, 4) };
                None(sRes);
                sysbit_t u32 { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                char data[4];
                BytesFromFloat(static_cast<float>(u32), data);
                cpu.PushSome(Get(Slice::New(data, 4)));

                cpu.state.bl = 4;
            }
            else if (signatureStr == "CSR_I32ToFloat")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                cpu.state.sp -= 4;

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp, 4) };
                None(sRes);
                int32_t i32 { IntegerFromBytes<int32_t>(Get(sRes).data) };

                char data[4];
                BytesFromFloat(static_cast<float>(i32), data);
                cpu.PushSome(Get(Slice::New(data, 4)));

                cpu.state.bl = 4;
            }
            else if (signatureStr == "CSR_FloatToU32")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                cpu.state.sp -= 4;

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp, 4) };
                None(sRes);
                float f { FloatFromBytes(Get(sRes).data) };

                char data[4];
                BytesFromInteger(static_cast<sysbit_t>(f), data);
                cpu.PushSome(Get(Slice::New(data, 4)));

                cpu.state.bl = 4;
            }
            else if (signatureStr == "CSR_FloatToU32")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                cpu.state.sp -= 4;

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp, 4) };
                None(sRes);
                float f { FloatFromBytes(Get(sRes).data) };

                char data[4];
                BytesFromInteger(static_cast<int32_t>(f), data);
                cpu.PushSome(Get(Slice::New(data, 4)));

                cpu.state.bl = 4;
            }
            else if (signatureStr == "CSR_PrintU32")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                cpu.state.sp -= 4;

                Res<Slice> sRes { ram.ReadSome(cpu.state.sp, 4) };
                None(sRes);
                sysbit_t u32 { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

                std::string str { std::to_string(u32) };
                std::cout.write(str.data(), str.size());
                std::cout.put('\n');
                cpu.state.bl = 0;
            }
            else if (signatureStr == "CSR_Clock")
            {
                cpu.state.pc += size + sizeof(sysbit_t);
                char bytes[4];
                BytesFromInteger<sysbit_t>((std::chrono::steady_clock::now() - startT).count(), bytes);
                cpu.PushSome(Get(Slice::New(bytes, 4)));
                cpu.state.bl = 4;
            }
            else [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Due to problems with FFI implementation, FFI is not yet supported. Fn: ", signatureStr);
                return System::ErrorCode::NativeCallError;
            }
        )

        op_BitXor: block(
            return BitLogic(
                OpCodes(op),
                {OpCodes::xorst, OpCodes::xorse, OpCodes::xorr},
                [](sysbit_t a, sysbit_t b) -> sysbit_t { return a ^ b; }
            );
        )
    }
    catch (const CSRException& e)
    {
        [[unlikely]]
        std::cout << e;
        return e.GetCode();
    }
    catch (const std::exception& e)
    {
        [[unlikely]]
        LOGE(System::LogLevel::High, "Unhandled exception.\n\t", e.what());
        return System::ErrorCode::UnhandledException;
    }

    [[unlikely]]
    return System::ErrorCode::UnhandledException;
}

#define arr std::array
#define fn std::function<sysbit_t(sysbit_t, sysbit_t)>
OPR FlatVM::BitLogic(OpCodes opc, arr<OpCodes, 3> op, fn bitwise) noexcept
{
    try_catch(
        if (opc == op.at(0))
        {
            Res<Slice> sRes { ram.ReadSome(cpu.state.sp-8, 4) };
            None(sRes);
            sysbit_t val1 { IntegerFromBytes<sysbit_t>(Get(sRes).data) };

            Res<Slice> sRes2 { ram.ReadSome(cpu.state.sp-4, 4) };
            None(sRes2);
            sysbit_t val2 { IntegerFromBytes<sysbit_t>(
                Get(sRes2).data
            )};

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            uchar_t mode { Get(modeRes) };

            if (Is8BitReg(mode))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(RegisterModeFlags(mode), cpu.state) };
                None(regRes);
                uchar_t& reg { *Get(regRes) };

                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(RegisterModeFlags(mode), cpu.state) };
                None(regRes);
                sysbit_t& reg { *Get(regRes) };

                reg = bitwise(val1, val2);
            }

            cpu.state.pc++;
            cpu.state.sp -= 8;
            return System::ErrorCode::Ok;
        }
        if (opc == op.at(1))
        {
            Res<char> valRes { ram.Read(cpu.state.sp-2) };
            None(valRes);
            uchar_t val1 { static_cast<uchar_t>(Get(valRes)) };

            valRes = ram.Read(cpu.state.sp-1);
            None(valRes);
            uchar_t val2 { static_cast<uchar_t>(Get(valRes)) };

            Res<uchar_t> modeRes { rom.Read(cpu.state.pc) };
            None(modeRes);
            uchar_t mode { Get(modeRes) };

            if (Is8BitReg(mode))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(RegisterModeFlags(mode), cpu.state) };
                None(regRes);
                uchar_t& reg { *Get(regRes) };

                reg = static_cast<uchar_t>(bitwise(val1, val2));
            }
            else
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(RegisterModeFlags(mode), cpu.state) };
                None(regRes);
                sysbit_t& reg { *Get(regRes) };

                reg = bitwise(val1, val2);
            }

            cpu.state.pc++;
            cpu.state.sp -= 2;
            return System::ErrorCode::Ok;
        }
        if (opc == op.at(2))
        {
            Res<uchar_t> modeType { rom.Read(cpu.state.pc) };
            None(modeType);
            RegisterModeFlags reg1mode { Get(modeType) };

            modeType = rom.Read(cpu.state.pc+1);
            None(modeType);
            RegisterModeFlags reg2mode { Get(modeType) };

            sysbit_t reg1;
            if (Is8BitReg(reg1mode))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(reg1mode, cpu.state) };
                None(regRes);
                reg1 = static_cast<sysbit_t>(*Get(regRes));
            }
            else
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(reg1mode, cpu.state) };
                None(regRes);
                reg1 = *Get(regRes);
            }

            if (Is8BitReg(reg2mode))
            {
                Res<uchar_t*> regRes { GetRegister8Bit(reg2mode, cpu.state) };
                None(regRes);
                uchar_t& reg2 { *Get(regRes) };
                reg2 = bitwise(reg2, static_cast<uchar_t>(reg1));
            }
            else
            {
                Res<sysbit_t*> regRes { GetRegister32Bit(reg2mode, cpu.state) };
                None(regRes);
                sysbit_t& reg2 { *Get(regRes) };
                reg2 = bitwise(reg2, static_cast<uchar_t>(reg1));
            }

            cpu.state.pc+=2;
            return System::ErrorCode::Ok;
        }

        [[unlikely]]
        return System::ErrorCode::InvalidSpecifier;,

        [[unlikely]]
        return exc.GetCode();,

        [[unlikely]]
        return System::ErrorCode::UnhandledException;
    )
}
#undef arr
#undef fn
