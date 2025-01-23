#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
#include "extensions/serialization.hpp"
#include "system.hpp"
#include <cassert>
#include <string>

Board::Board(const Assembly& assembly) : parent(assembly)
{
    // Check if there is a problem reading the sizes
    char tmp;
    for (int i = 4; i < 12; i++)
        assembly.Rom().TryRead(i, tmp, true);

    // second 32 bits of ROM is stack size
    systembit_t stackSize { IntegerFromBytes<systembit_t>(assembly.Rom()&4) }; 

    // third 32 bits of ROM is heap size
    systembit_t heapSize { IntegerFromBytes<systembit_t>(assembly.Rom()&8)};

    std::cout << "Stack Size: " << stackSize << "\nHeap Size: " << heapSize << '\n';
    CSR_ERR("Error");

    this->ram.stackSize = stackSize;
    this->ram.heapSize = heapSize;

    this->ram.data = new char[stackSize + heapSize];
    this->ram.data = new char[(stackSize+heapSize)/8];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap and stack
    // sizes must be multiple of 8 in total.

    // TODO: Finish Board, Create CPU
}
