#include <cassert>
#include <string>

#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
#include "extensions/serialization.hpp"
#include "system.hpp"

//
// Board Implementation
//
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

    this->ram.stackSize = stackSize;
    this->ram.heapSize = heapSize;

    this->ram.data = new char[stackSize + heapSize];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap size must
    // be multiple of 8
    this->ram.allocationMap= new char[heapSize/8];

    // TODO: Finish Board, Create CPU
}

//
// RAM Implementation
//
char RAM::Read(const systembit_t index) const
{
    if (index >= (this->stackSize+this->heapSize) || index < 0)
        LOGE(System::LogLevel::High, "Attempt to read out of bounds memory '", std::to_string(index), "'");
    
    return this->data[index];
}

bool RAM::Write(const systembit_t index, char value) noexcept
{
    if (index >= (this->stackSize+this->heapSize) || index < 0)
        return false;
    this->data[index] = value; 
    return true;
}

systembit_t RAM::Allocate(const systembit_t size)
{
    systembit_t counter { size };
    systembit_t allocationAddress { 0 };
    for (systembit_t i = 0; i < this->heapSize/8; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index/8) };

        const bool flag = (static_cast<uchar_t>(this->allocationMap[index]) >> (8 - offset)) & 1;
        if (flag)
        {
            counter = size;
            allocationAddress = 0;
            continue;
        }
        allocationAddress = allocationAddress == 0 ? i : allocationAddress; 
        counter--;
        if (counter == 0)
            break;
    }
    if (counter != 0)
        LOGE(System::LogLevel::High, "Couldn't allocate memory on heap, board is out of memory.");
    for (systembit_t i = 0; i < size; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) };
        this->allocationMap[index] |= (uchar_t{1} << (8-offset));
    }
    return allocationAddress;
}

bool RAM::Deallocate(const systembit_t address, const systembit_t size)
{
    for (systembit_t i = 0; i < size; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 
        const bool flag = (static_cast<uchar_t>(this->allocationMap[index]) >> (8 - offset)) & 1;
        if (!flag)
            return false;
    }
    for (systembit_t i = 0; i < size; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 

        data[address+offset+index*8] = 0;
        allocationMap[index] |= ~((uchar_t{1} << (8 - offset))); 
    }

    return true;
}
