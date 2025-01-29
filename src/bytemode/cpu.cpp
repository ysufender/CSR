#include <cassert>
#include <string>

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

const System::ErrorCode CPU::Cycle() noexcept
{
    constexpr OperationFunction ops[] = {
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr,
        CPU::MoveReg
    };

    char op;
    System::ErrorCode code { this->board.Assembly().Rom().TryRead(this->state.pc, op) };

    if (code != System::ErrorCode::Ok)
    {
        LOGE(
            System::LogLevel::Medium, 
            "In ", this->board.Stringify(),
            ", error while trying to read the instructions from ROM. Exit code: ",
            std::to_string(static_cast<int>(code))
        );
        return code;
    }

    LOGD("CPU read op-code: ", std::to_string(op));

    return ops[op](*this);
}
