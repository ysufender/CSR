#include <cstddef>
#include <cstdint>
#include <string>

#include "extensions/converters.hpp"
#include "bytemode/instructions.hpp"
#include "bytemode/jit.hpp"
#include "CSRConfig.hpp"

#define Enumc(regn) static_cast<char>(regn)
#define Is8BitReg(reg) (Enumc(reg) >= Enumc(RegisterModeFlags::al)) && (Enumc(reg) <= Enumc(RegisterModeFlags::flg))

using namespace x86;

x86::Gp GetRegister(RegisterModeFlags reg)
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
            CRASH(
                System::ErrorCode::InvalidSpecifier,
                RegisterModeFlagsString(reg), " is not an 8bit register."
            );
            return T3;
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

    if (rom[pc] >= std::size(instructions))
        return JITError::CompilationError;

    try {
        goto *instructions[rom[pc++]];

        op_NoOperation: { asml.nop(); return JITError::Ok; }
        op_StoreThirtyTwo: {
            sysbit_t imm { IntegerFromBytes<sysbit_t>(rom.ReadSome(pc, 4).data) };
            asml.mov(T2, dword_ptr(rreg32, rsp));
            asml.mov(dword_ptr(rram, T2), imm);
            asml.add(dword_ptr(rreg32, rsp), 4);
            //asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, ram)));
            //asml.mov(dword_ptr(T1, SP), imm);
            //asml.add(SP, 4);
            pc+=4;
            return JITError::Ok;
        }
        op_StoreEight: { return JITError::UnsupportedInstruction; }
        op_StoreFromSymbol: { return JITError::UnsupportedInstruction; }
        op_LoadFromStack: { return JITError::UnsupportedInstruction; }
        op_ReadFromHeap: { return JITError::UnsupportedInstruction; }
        op_ReadFromRegister: { return JITError::UnsupportedInstruction; }
        op_Move: { 
            return JITError::UnsupportedInstruction;
            OpCodes op { rom.Read(pc-1) };
            RegisterModeFlags regF { rom.Read(pc) };  
            int size { Is8BitReg(regF) ? 1 : 4 };

            if (op == OpCodes::movc)
            {
                if (size == 1)
                    asml.mov(GetRegister(regF), rom.Read(pc+1));
                else
                    asml.mov(GetRegister(regF), IntegerFromBytes<uint32_t>(rom.ReadSome(pc+1, 4).data));
                pc += size+1;
            }
            else
                return JITError::UnsupportedInstruction;
            return JITError::Ok;
        }
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
        op_Jump: {
            return JITError::UnsupportedInstruction;
            if (rom.Read(pc-1) == (uchar_t)OpCodes::jmpr) [[unlikely]]
            {
                return JITError::UnsupportedInstruction;
            }
            else [[likely]]
            {
                const uint32_t symbol { IntegerFromBytes<uint32_t>(rom.ReadSome(pc, 4).data) };
                if (!(start <= symbol && symbol <= pc-1))
                {
                    Label lbl { asml.newLabel() };
                    asml.mov(T2, firstParamReg);
                    asml.push(secondParamReg);
                    asml.push(x86::rax);
                    asml.mov(firstParamReg, qword_ptr(x86::rdi, offsetof(JITContext, vm)));
                    asml.mov(secondParamReg, symbol);
                    asml.call(qword_ptr(T2, offsetof(JITContext, IsCompiled)));
                    asml.test(x86::al, x86::al); 
                    asml.jz(lbl);

                    asml.call(qword_ptr(T2, offsetof(JITContext, GetEntry)));
                    asml.mov(T3, x86::rax);
                    asml.pop(x86::rax);
                    asml.pop(secondParamReg);
                    asml.mov(firstParamReg, T2);
                    restore()
                    asml.jmp(T3);
                    
                    asml.bind(lbl);
                    restore()
                    asml.ret();
                    pc += 4;
                    return JITError::Ok;
                }

                pc--; 
                return JITError::Finish;
            }
        }
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

        op_IncrementLocal: {
            return JITError::UnsupportedInstruction;
            const sysbit_t index { IntegerFromBytes<sysbit_t>(rom.ReadSome(pc, 4).data) };  

            switch (OpCodes(rom[pc-1]))
            {
                case OpCodes::incli: {
                    const sysbit_t inc { IntegerFromBytes<sysbit_t>(rom.ReadSome(pc+4, 4).data) };
                    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, ram)));
                    if (inc == 1)
                        asml.inc(dword_ptr(T1, BP, 0, index));
                    else
                        asml.add(dword_ptr(T1, BP, 0, index), inc);
                    pc += 8;
                    return JITError::Ok;
                }

                case OpCodes::inclf: {
                    const float inc { FloatFromBytes(rom.ReadSome(pc+4, 4).data) };
                    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, ram)));
                    asml.movss(RF1, dword_ptr(T1, BP, 0, index));
                    asml.mov(T2, reinterpret_cast<const float*>(rom&(pc+4)));
                    asml.movss(RF2, x86::dword_ptr(T2));
                    asml.addss(RF1, RF2);
                    asml.movss(dword_ptr(T1), RF1);
                    pc += 8;
                    return JITError::Ok;
                }

                case OpCodes::inclb: {
                    uint8_t inc { IntegerFromBytes<uint8_t>(rom.ReadSome(pc+4, 1).data) };
                    asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, ram)));
                    asml.add(byte_ptr(T1, BP, 0, index), inc);
                    pc += 5;
                    return JITError::Ok;
                }

                default: return JITError::UnsupportedInstruction;
            }
        }

        op_ReadLocal: {
            sysbit_t index { IntegerFromBytes<sysbit_t>(rom.ReadSome(pc, 4).data) };
            //asml.mov(T1, qword_ptr(firstParamReg, offsetof(JITContext, ram)));
            if (rom.Read(pc-1) == uchar_t(OpCodes::rdlt))
            {
                //asml.mov(T2, dword_ptr(T1, BP, 0, index));
                //asml.mov(dword_ptr(T1, SP), T2);
                //asml.add(SP, 4);
                asml.mov(T1, dword_ptr(rreg32, rsp));
                asml.mov(T2, dword_ptr(rreg32, rbp));
                asml.mov(T2, dword_ptr(rram, T2, 0, index));
                asml.mov(dword_ptr(rram, T1), T2);
                asml.add(dword_ptr(rreg32, rsp), 4);
            }
            else
            {
                asml.mov(T1, dword_ptr(rreg32, rsp));
                asml.mov(T2, dword_ptr(rreg32, rbp));
                asml.mov(T2, dword_ptr(rram, T2, 0, index));
                asml.mov(dword_ptr(rram, T1), T2);
                asml.inc(dword_ptr(rreg32, rsp));
                //asml.mov(T2, byte_ptr(T1, BP, 0, index));
                //asml.mov(dword_ptr(T1, SP), T2);
                //asml.inc(SP);
            }
            pc += 4;
            return JITError::Ok;
        }

        op_CompareJump: { 
            return JITError::UnsupportedInstruction;
        }
    }
    catch (const std::exception& e)
    { return JITError::CompilationError; }

    return JITError::CompilationError;
}
