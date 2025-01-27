#include "bytemode/cpu.hpp"
#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
#include "system.hpp"
#include <string>

CPU::CPU(Board& board) : board(board)
{
    // Check ROM for stack/heap sizes beforehand.
    char tmp;
    for (int i = 0; i < 12; i++)
        board.Assembly().Rom().TryRead(i, tmp, true);

    sysbit_t origin { IntegerFromBytes<sysbit_t>(&board.Assembly().Rom()) };

    this->pc = origin;
}

const System::ErrorCode CPU::Cycle() noexcept
{
    return System::ErrorCode::Ok;
}
