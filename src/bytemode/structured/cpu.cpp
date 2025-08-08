#include <cassert>
#include <limits>
#include <memory>
#include <string>

#include "bytemode/structured/instructions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/structured/assembly.hpp"
#include "bytemode/structured/cpu.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

CPU::CPU(Board& board) : 
    board(board),
    state(),
    paramBuf(std::make_unique_for_overwrite<char[]>(std::numeric_limits<uchar_t>::max()))
{
    // Check ROM for stack/heap sizes beforehand.
    uchar_t tmp;
    System::ErrorCode code;
    for (int i = 0; i < 12; i++)
    {
        code = board.Assembly().Rom().TryRead(i, tmp);
        if (code != System::ErrorCode::Ok)
            CRASH(code, "Error while initializing CPU for ", this->board.Stringify());
    }

    this->state.pc = IntegerFromBytes<sysbit_t>(&board.Assembly().Rom());
}


Error CPU::Cycle() noexcept {
    static void* const jumpTable[] = {
        &&op_NoOperation, &&op_StoreThirtyTwo, &&op_StoreEight, &&op_StoreFromSymbol, &&op_StoreFromSymbol,
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
        &&op_CompareJump
    };

    uchar_t op;
    System::ErrorCode code = board.Assembly().Rom().TryRead(state.pc, op);
    if ( code != System::ErrorCode::Ok) [[unlikely]] {
        LOGE(System::LogLevel::Medium, board.Stringify(), " ROM read error: ", System::ErrorCodeString(code));
        return code;
    }

    if (op >= std::size(jumpTable)) [[unlikely]] {
        LOGE(System::LogLevel::Medium, board.Stringify(), " invalid opcode '", OpCodesString(op), "' at PC=", std::to_string(state.pc));
        return System::ErrorCode::InvalidInstruction;
    }

    state.pc++;
    goto *jumpTable[op];

op_NoOperation:         return NoOperation(*this);
op_StoreThirtyTwo:      return StoreThirtyTwo(*this);
op_StoreEight:          return StoreEight(*this);
op_StoreFromSymbol:     return StoreFromSymbol(*this);
op_LoadFromStack:       return LoadFromStack(*this);
op_ReadFromHeap:        return ReadFromHeap(*this);
op_ReadFromRegister:    return ReadFromRegister(*this);
op_Move:                return Move(*this);
op_Add32:               return Add32(*this);
op_AddFloat:            return AddFloat(*this);
op_Add8:                return Add8(*this);
op_AddReg:              return AddReg(*this);
op_AddSafe32:           return AddSafe32(*this);
op_AddSafeFloat:        return AddSafeFloat(*this);
op_AddSafe8:            return AddSafe8(*this);
op_MemCopy:             return MemCopy(*this);
op_Increment:           return Increment(*this);
op_IncrementReg:        return IncrementReg(*this);
op_IncrementSafe:       return IncrementSafe(*this);
op_Decrement:           return Decrement(*this);
op_DecrementReg:        return DecrementReg(*this);
op_DecrementSafe:       return DecrementSafe(*this);
op_BitAnd:              return BitAnd(*this);
op_BitOr:               return BitOr(*this);
op_BitNor:              return BitNor(*this);
op_SwapTop:             return SwapTop(*this);
op_DuplicateTop:        return DuplicateTop(*this);
op_RawDataStack:        return RawDataStack(*this);
op_Invert:              return Invert(*this);
op_InvertSafe:          return InvertSafe(*this);
op_Compare:             return Compare(*this);
op_PopInstruction:      return PopInstruction(*this);
op_Jump:                return Jump(*this);
op_SwapRange:           return SwapRange(*this);
op_DuplicateRange:      return DuplicateRange(*this);
op_Repeat:              return Repeat(*this);
op_Allocate:            return Allocate(*this);
op_PowRegister:         return PowRegister(*this);
op_PowStack:            return PowStack(*this);
op_PowConst:            return PowConst(*this);
op_SqrtConst:           return SqrtConst(*this);
op_SqrtRegister:        return SqrtRegister(*this);
op_SqrtStack:           return SqrtStack(*this);
op_ConditionalJump:     return ConditionalJump(*this);
op_CallFunc:            return CallFunc(*this);
op_MulStack:            return MulStack(*this);
op_MulRegister:         return MulRegister(*this);
op_MulSafe:             return MulSafe(*this);
op_DivStack:            return DivStack(*this);
op_DivRegister:         return DivRegister(*this);
op_DivSafe:             return DivSafe(*this);
op_Return:              return Return(*this);
op_Deallocate:          return Deallocate(*this);
op_Sub32:               return Sub32(*this);
op_SubFloat:            return SubFloat(*this);
op_Sub8:                return Sub8(*this);
op_SubReg:              return SubReg(*this);
op_SubSafe32:           return SubSafe32(*this);
op_SubSafeFloat:        return SubSafeFloat(*this);
op_SubSafe8:            return SubSafe8(*this);
op_IncrementLocal:      return IncrementLocal(*this);
op_ReadLocal:           return ReadLocal(*this);
op_CompareJump:         return CompareJump(*this);
}


Error CPU::Push(const char value) noexcept 
{
    if (this->state.sp+1 > this->board.ram.StackSize())
    {
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " can't push value onto stack, stack is full."
        );
        return System::ErrorCode::StackOverflow;
    }

    Error errc { this->board.ram.Write(this->state.sp, value) };

    if (errc == System::ErrorCode::Ok)
        this->state.sp++;
    else
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " error while pushing value onto stack. Error code: ",
            System::ErrorCodeString(errc)
        );

    return errc;
}

Error CPU::Pop() noexcept
{
    if (this->state.sp < 1)
    {
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " error while popping value from stack. Can't pop while SP < 1. Error Code: ",
            System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
        );

        return System::ErrorCode::IndexOutOfBounds;
    }

    this->state.sp--;
    return Error::Ok;
}

Error CPU::PushSome(const Slice values) noexcept
{
    if (this->state.sp+values.size > this->board.ram.StackSize())
    {
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " can't push value onto stack, stack is full."
        );
        return System::ErrorCode::StackOverflow;
    }

    Error errc { this->board.ram.WriteSome(this->state.sp, values) };

    if (errc == System::ErrorCode::Ok)
        this->state.sp+=values.size;
    else
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " error while pushing value onto stack. Error code: ",
            System::ErrorCodeString(errc)
        );

    return errc;
}

Error CPU::PopSome(const sysbit_t size) noexcept
{
    if (this->state.sp-size < 0)
    {
        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.GetExecutingProcess().Stringify(),
            " error while popping value from stack. Can't pop while SP-size < 0. Error Code: ",
            System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
        );

        return System::ErrorCode::IndexOutOfBounds;
    }

    this->state.sp -= size;
    return Error::Ok;
}
