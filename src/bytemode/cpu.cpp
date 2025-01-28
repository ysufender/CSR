#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/cpu.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"
#include <string>

CPU::CPU(Board& board) : board(board), state()
{
    // Check ROM for stack/heap sizes beforehand.
    char tmp;
    for (int i = 0; i < 12; i++)
        board.Assembly().Rom().TryRead(i, tmp, true);

    this->state.pc = IntegerFromBytes<sysbit_t>(&board.Assembly().Rom());
}

const System::ErrorCode CPU::Cycle() noexcept
{
    char op;
    System::ErrorCode code { this->board.Assembly().Rom().TryRead(this->state.pc, op) };

    if (code != System::ErrorCode::Ok)
        LOGE(
            System::LogLevel::Medium, 
            "In assembly ", this->board.Assembly().Stringify(),
        );

    return System::ErrorCode::Ok;
}
