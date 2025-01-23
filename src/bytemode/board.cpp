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
Board::Board(const class Assembly& assembly) : parent(assembly), cpu(*this)
{
    // CPU will be initialized beforehand, so it checks the ROM.


    // second 32 bits of ROM is stack size
    systembit_t stackSize { IntegerFromBytes<systembit_t>(assembly.Rom()&4) }; 

    // third 32 bits of ROM is heap size
    systembit_t heapSize { IntegerFromBytes<systembit_t>(assembly.Rom()&8)};
    
    LOGD("Stack Size: ", std::to_string(stackSize));
    LOGD("Heap Size: ", std::to_string(heapSize));

    this->ram.stackSize = stackSize;
    this->ram.heapSize = heapSize;

    this->ram.data = new char[stackSize + heapSize];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap size must
    // be multiple of 8
    this->ram.allocationMap= new char[heapSize/8];

    // CPU is already created. 
}

//
// RAM Implementation
//
char RAM::Read(const systembit_t address) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        LOGE(System::LogLevel::High, "Attempt to read out of bounds memory '", std::to_string(address), "'");
    
    return this->data[address];
}

char* RAM::ReadSome(const systembit_t address, const systembit_t size) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        LOGE(System::LogLevel::High, "Attempt to read out of bounds memory '", std::to_string(address), "'");

    return this->data+address;
}

System::ErrorCode RAM::Write(const systembit_t address, char value) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        return System::ErrorCode::Ok;
    this->data[address] = value; 
    return System::ErrorCode::Ok;
}

System::ErrorCode RAM::WriteSome(const systembit_t address, const systembit_t size, char* values) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        LOGE(System::LogLevel::High, "Attempt to read out of bounds memory '", std::to_string(address), "'");

    System::ErrorCode status = System::ErrorCode::Ok;
    for(systembit_t i = 0; i < size; i++)
        status = status == System::ErrorCode::Ok ? this->Write(address+i, values[i]) : status;
    return status;
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

System::ErrorCode RAM::Deallocate(const systembit_t address, const systembit_t size) noexcept
{
    for (systembit_t i = 0; i < size; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 
        const bool flag = (static_cast<uchar_t>(this->allocationMap[index]) >> (8 - offset)) & 1;
        if (!flag)
            return System::ErrorCode::Bad;
    }
    for (systembit_t i = 0; i < size; i++)
    {
        systembit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 

        data[address+offset+index*8] = 0;
        allocationMap[index] |= ~((uchar_t{1} << (8 - offset))); 
    }

    return System::ErrorCode::Ok;
}
