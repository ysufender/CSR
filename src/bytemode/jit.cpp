#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

#include "extensions/converters.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/jit.hpp"
#include "CSRConfig.hpp"

#define Enumc(regn) static_cast<char>(regn)
#define Is8BitReg(reg) (Enumc(reg) >= Enumc(RegisterModeFlags::al)) && (Enumc(reg) <= Enumc(RegisterModeFlags::flg))

using namespace x86;

System::Result<x86::Gp> GetRegister(RegisterModeFlags reg)
{
    static sysbit_t dummy { 0 };
    switch (reg)
    {
        case RegisterModeFlags::eax: return EAX;
        case RegisterModeFlags::ebx: return EBX;
        case RegisterModeFlags::ecx: return ECX;
        case RegisterModeFlags::edx: return EDX;
        case RegisterModeFlags::pc: return PC;
        case RegisterModeFlags::sp: return SP;
        case RegisterModeFlags::bp: return BP;
        case RegisterModeFlags::al: return AL;
        case RegisterModeFlags::bl: return BL;
        case RegisterModeFlags::cl: return CL;
        case RegisterModeFlags::flg:
        case RegisterModeFlags::dl:
        case RegisterModeFlags::edi:
        case RegisterModeFlags::esi:
        default: [[unlikely]]
        {
            LOGE(System::LogLevel::High, RegisterModeFlagsString(reg), " is not an 8bit register.");
            return System::ErrorCode::InvalidSpecifier;
        }
    }
}

JITError JITInstruction(const BaseROM& rom, uint32_t& pc, x86::Assembler& asml, JITContext& context, const uint32_t start)
{
    static constexpr void* instructions[] {
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
    };

    System::Result<uchar_t> opRes { rom[pc] };
    uchar_t op;
    if (System::None(opRes) || (op = System::Get(std::move(opRes))) >= std::size(instructions))
        return JITError::CompilationError;

    try {
        pc++;
        goto *instructions[op];

        op_NoOperation: { return JITError::UnsupportedInstruction; }
        op_StoreThirtyTwo: { return JITError::UnsupportedInstruction; }
        op_StoreEight: { return JITError::UnsupportedInstruction; }
        op_StoreFromSymbol: { return JITError::UnsupportedInstruction; }
        op_LoadFromStack: { return JITError::UnsupportedInstruction; }
        op_ReadFromHeap: { return JITError::UnsupportedInstruction; }
        op_ReadFromRegister: { return JITError::UnsupportedInstruction; }
        op_Move: { return JITError::UnsupportedInstruction; }
        op_Add32: { return JITError::UnsupportedInstruction; }
        op_AddFloat: { return JITError::UnsupportedInstruction; }
        op_Add8: { return JITError::UnsupportedInstruction; }
        op_AddReg: { return JITError::UnsupportedInstruction; }
        op_AddSafe32: { return JITError::UnsupportedInstruction; }
        op_AddSafeFloat: { return JITError::UnsupportedInstruction; }
        op_AddSafe8: { return JITError::UnsupportedInstruction; }
        op_MemCopy: { return JITError::UnsupportedInstruction; }
        op_Increment: { return JITError::UnsupportedInstruction; }
        op_IncrementReg: { return JITError::UnsupportedInstruction; }
        op_IncrementSafe: { return JITError::UnsupportedInstruction; }
        op_Decrement: { return JITError::UnsupportedInstruction; }
        op_DecrementReg: { return JITError::UnsupportedInstruction; }
        op_DecrementSafe: { return JITError::UnsupportedInstruction; }
        op_BitAnd: { return JITError::UnsupportedInstruction; }
        op_BitOr: { return JITError::UnsupportedInstruction; }
        op_BitNor: { return JITError::UnsupportedInstruction; }
        op_SwapTop: { return JITError::UnsupportedInstruction; }
        op_DuplicateTop: { return JITError::UnsupportedInstruction; }
        op_RawDataStack: { return JITError::UnsupportedInstruction; }
        op_Invert: { return JITError::UnsupportedInstruction; }
        op_InvertSafe: { return JITError::UnsupportedInstruction; }
        op_Compare: { return JITError::UnsupportedInstruction; }
        op_PopInstruction: { return JITError::UnsupportedInstruction; }
        op_Jump: { return JITError::UnsupportedInstruction; }
        op_SwapRange: { return JITError::UnsupportedInstruction; }
        op_DuplicateRange: { return JITError::UnsupportedInstruction; }
        op_Repeat: { return JITError::UnsupportedInstruction; }
        op_Allocate: { return JITError::UnsupportedInstruction; }
        op_PowRegister: { return JITError::UnsupportedInstruction; }
        op_PowStack: { return JITError::UnsupportedInstruction; }
        op_PowConst: { return JITError::UnsupportedInstruction; }
        op_SqrtConst: { return JITError::UnsupportedInstruction; }
        op_SqrtRegister: { return JITError::UnsupportedInstruction; }
        op_SqrtStack: { return JITError::UnsupportedInstruction; }
        op_ConditionalJump: { return JITError::UnsupportedInstruction; }
        op_CallFunc: { return JITError::UnsupportedInstruction; }
        op_MulStack: { return JITError::UnsupportedInstruction; }
        op_MulRegister: { return JITError::UnsupportedInstruction; }
        op_MulSafe: { return JITError::UnsupportedInstruction; }
        op_DivStack: { return JITError::UnsupportedInstruction; }
        op_DivRegister: { return JITError::UnsupportedInstruction; }
        op_DivSafe: { return JITError::UnsupportedInstruction; }
        op_Return: { return JITError::UnsupportedInstruction; }
        op_Deallocate: { return JITError::UnsupportedInstruction; }
        op_Sub32: { return JITError::UnsupportedInstruction; }
        op_SubFloat: { return JITError::UnsupportedInstruction; }
        op_Sub8: { return JITError::UnsupportedInstruction; }
        op_SubReg: { return JITError::UnsupportedInstruction; }
        op_SubSafe32: { return JITError::UnsupportedInstruction; }
        op_SubSafeFloat: { return JITError::UnsupportedInstruction; }
        op_SubSafe8: { return JITError::UnsupportedInstruction; }
        op_IncrementLocal: { return JITError::UnsupportedInstruction; }
        op_ReadLocal: { return JITError::UnsupportedInstruction; }
        op_CompareJump: { return JITError::UnsupportedInstruction; }
    }
    catch (const std::exception& e)
    { return JITError::CompilationError; }

    return JITError::CompilationError;
}
