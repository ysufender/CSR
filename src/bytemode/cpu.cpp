#include <cassert>
#include <string>

#include "bytemode/instructions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/cpu.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

CPU::CPU(Board& board) : board(board), state()
{
    // Check ROM for stack/heap sizes beforehand.
    char tmp;
    System::ErrorCode code;
    for (int i = 0; i < 12; i++)
    {
        code = board.Assembly().Rom().TryRead(i, tmp);
        if (code != System::ErrorCode::Ok)
            CRASH(code, "Error while initializing CPU for ", this->board.Stringify());
    }

    this->state.pc = IntegerFromBytes<sysbit_t>(&board.Assembly().Rom());
}

Error CPU::Cycle() noexcept
{
    static constexpr OperationFunction ops[] = {
        NoOperation,
        StoreThirtyTwo, StoreEight, StoreFromSymbol, StoreFromSymbol,
        LoadFromStack, LoadFromStack, ReadFromHeap, ReadFromHeap, ReadFromRegister,
        Move, Move, Move,
        Add32, AddFloat, Add8, AddReg, AddReg, AddReg,
        AddSafe32, AddSafeFloat, AddSafe8,
        MemCopy,
        Increment, Increment, Increment, IncrementReg, IncrementReg, IncrementReg,
        IncrementSafe, IncrementSafe, IncrementSafe,
        Decrement, Decrement, Decrement, DecrementReg, DecrementReg, DecrementReg,
        DecrementSafe, DecrementSafe, DecrementSafe,
        BitAnd, BitAnd, BitAnd,
        BitOr, BitOr, BitOr,
        BitNor, BitNor, BitNor,
        SwapTop, SwapTop, SwapTop,
        DuplicateTop, DuplicateTop,
        RawDataStack, RawDataStack,
        Invert, Invert, Invert, InvertSafe, InvertSafe,
        Compare, Compare,
        PopInstruction, PopInstruction,
        Jump, Jump,
        SwapRange, DuplicateRange,
        Repeat,
        Allocate,
        PowRegister, PowRegister, PowRegister,
        PowStack, PowStack, PowStack,
        PowConst, PowConst, PowConst,
        SqrtConst, SqrtConst, SqrtConst,
        SqrtRegister, SqrtRegister, SqrtRegister,
        SqrtStack, SqrtStack, SqrtStack,
        ConditionalJump, ConditionalJump,
        CallFunc, CallFunc,
        MulStack, MulStack, MulStack,
        MulRegister, MulRegister, MulRegister,
        MulSafe, MulSafe, MulSafe,
        DivStack, DivStack, DivStack,
        DivRegister, DivRegister, DivRegister,
        DivSafe, DivSafe, DivSafe,
        Return,
        Deallocate,
        Sub32, SubFloat, Sub8, SubReg, SubReg, SubReg,
        SubSafe32, SubSafeFloat, SubSafe8
    };

    char op;
    System::ErrorCode code { this->board.Assembly().Rom().TryRead(this->state.pc, op) };

    if (code != System::ErrorCode::Ok)
    {
        LOGE(
            System::LogLevel::Medium, 
            "In ", this->board.Stringify(),
            ", error while trying to read the instructions from ROM. Exit code: ",
            System::ErrorCodeString(code)
        );
        return code;
    }

    if (sizeof(ops)/8 > op)
    {
        this->state.pc++;
        code = ops[op](*this);

        if (code == System::ErrorCode::Ok)
            return code;

        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.Stringify(),
            ", error while executing the instruction ", OpCodesString(op),
            ". Error code: ", System::ErrorCodeString(code)
        );

        return code;
    }

    LOGE(
        System::LogLevel::Low,
        "In ", this->board.Stringify(),
        ", error while executing the instruction '", OpCodesString(op), 
        "' at ROM index '", std::to_string(this->state.pc),
        "'. Instruction hasn't been implemented yet or instruction is wrong."
    );

    return System::ErrorCode::InvalidInstruction;
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
