#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"

Board::Board(const Assembly& assembly) : parent(assembly)
{
    // Check if there is a problem reading the sizes
    char tmp;
    for (int i = 4; i < 12; i++)
        assembly.Rom().TryRead(i, tmp, true);

    // second 32 bits of ROM is stack size
    systembit_t stackSize;
    stackSize |= assembly.Rom()[4].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[5].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[6].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[7].data;

    // third 32 bits of ROM is heap size
    systembit_t heapSize;
    heapSize |= assembly.Rom()[8].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[9].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[10].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[11].data;

    this->ram.stackSize = stackSize;
    this->ram.heapSize = heapSize;

    this->ram.data = new char[stackSize + heapSize];
    this->ram.data = new char[(stackSize+heapSize)/8];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap and stack
    // sizes must be multiple of 8 in total.

    // TODO: Finish Board, Create CPU
}
