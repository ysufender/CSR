#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"

Board::Board(const Assembly& assembly) : residingAssembly(assembly)
{
    // first 32 bits of ROM is stack size,
    // second 32 bits of ROM is heap size,
    systembit_t stackSize;
    stackSize |= assembly.Rom()[0].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[1].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[2].data; stackSize <<= 8;
    stackSize |= assembly.Rom()[3].data;

    systembit_t heapSize;
    heapSize |= assembly.Rom()[0].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[1].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[2].data; heapSize <<= 8;
    heapSize |= assembly.Rom()[3].data;

    this->ram.stackSize = stackSize;
    this->ram.heapSize = heapSize;

    this->ram.data = new char[stackSize + heapSize];
    this->ram.data = new char[(stackSize+heapSize)/8];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap and stack
    // sizes must be multiple of 8 in total.

    // TODO: Finish Board, Create CPU
}
