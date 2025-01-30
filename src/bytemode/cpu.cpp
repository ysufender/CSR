#include <cassert>
#include <string>

#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/cpu.hpp"
#include "CSRConfig.hpp"
#include "extensions/stringextensions.hpp"
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

const System::ErrorCode CPU::Cycle() noexcept
{
    static constexpr OperationFunction ops[] = {
        Nop
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

    LOGD(
        "CPU read op-code: ", 
        OpCodesString(op), 
        " ", 
        std::to_string(op), 
        " at state.pc: ", 
        std::to_string(state.pc)
    );

    if (sizeof(ops)/8 > op)
    {
        code = ops[op](*this);

        if (code == System::ErrorCode::Ok)
            return code;

        LOGE(
            System::LogLevel::Medium,
            "In ", this->board.Stringify(),
            ", error while executing the instruction ", OpCodesString(op),
            ". Error code: ", System::ErrorCodeString(code)
        );
    }

    LOGE(
        System::LogLevel::Medium,
        "In ", this->board.Stringify(),
        ", error while executing the instruction ", OpCodesString(op), 
        " at ROM index ", std::to_string(this->state.pc),
        ". Instruction hasn't been implemented yet or instruction is wrong."
    );

    return System::ErrorCode::InvalidInstruction;
}
