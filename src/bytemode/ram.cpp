#include "CSRConfig.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/board.hpp"
#include "system.hpp"
#include <bitset>
#include <cassert>
#include <string>
#include "bytemode/ram.hpp"

//
// RAM Implementation
//
char RAM::Read(const sysbit_t address) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        CRASH(
            System::ErrorCode::RAMAccessError, 
            "Error in ", this->board.Stringify(),
            " Attempt to read out of bounds memory ", std::to_string(address)
        );
    
    return this->data[address];
}

const Slice RAM::ReadSome(const sysbit_t address, const sysbit_t size) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        CRASH(
            System::ErrorCode::RAMAccessError, 
            "Error in ", this->board.Stringify(),
            " Attempt to read out of bounds memory ", std::to_string(address)
        );

    return {
        this->data.get()+address,
        size
    };}

const System::ErrorCode RAM::Write(const sysbit_t address, char value) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        return System::ErrorCode::RAMAccessError;
    this->data[address] = value; 
    return System::ErrorCode::Ok;
}

const System::ErrorCode RAM::WriteSome(const sysbit_t address, const sysbit_t size, const char* values) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
    {
        LOGE(
            System::LogLevel::Medium, 
            "Error in ", this->board.Stringify(),
            ". Attempt to read out of bounds memory ",
            std::to_string(address)
        );
        return System::ErrorCode::RAMAccessError;
    }

    System::ErrorCode status = System::ErrorCode::Ok;
    for(sysbit_t i = 0; i < size; i++)
    {
        status = this->Write(address+i, values[i]);

        if (status != System::ErrorCode::Ok)
        {
            LOGE(
                System::LogLevel::Medium,
                "Error in ",
                this->board.Stringify(), 
                ". Can't write to RAM address ", std::to_string(address),
                ". Error code: ", System::ErrorCodeString(status)
            );
            break;
        }
            
    }
    return status;
}

sysbit_t RAM::Allocate(const sysbit_t size)
{
    LOGD("Allocating ", std::to_string(size), " bytes from ", this->board.Stringify());
    LOGD("Heap Size: ", std::to_string(heapSize));
    LOGD("Allocation Map Size: ", std::to_string(heapSize/8));

    sysbit_t counter { size };
    sysbit_t allocationAddr { 0 };
    bool set = false;
    for (sysbit_t i = 0; i < this->heapSize; i++)
    {
        if (counter == 0)
            break;

        const sysbit_t index { i/8 };
        const uchar_t offset { static_cast<uchar_t>(i - (index*8)) };
        const bool isAvailable { this->allocationMap[i/8] >> (8-offset) == 0 }; 
        if (!isAvailable)
        {
            counter = size;
            allocationAddr = 0;
            set = false;
            continue;
        }

        allocationAddr = set ? allocationAddr : i;
        counter--;
        set = true;
    }
    if (counter != 0)
        CRASH(
            System::ErrorCode::HeapOverflow,
            "Can't allocate memory of size ", std::to_string(size),
            " bytes from ", this->board.Stringify(), ". Board is out of memory."
        );
    for (sysbit_t i = allocationAddr; i < allocationAddr+size; i++)
    {
        const sysbit_t index { i/8 }; 
        const uchar_t offset { static_cast<uchar_t>(i - (index*8)) };
        this->allocationMap[index] |= (uchar_t{1} << (7-offset));
    }

    return allocationAddr;
}

const System::ErrorCode RAM::Deallocate(const sysbit_t address, const sysbit_t size) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
    {
        LOGE(
            System::LogLevel::Medium, 
            "Error in ", this->board.Stringify(),
            ". Attempt to read out of bounds memory ", std::to_string(address)
        );
        return System::ErrorCode::RAMAccessError;
    }
        

    for (sysbit_t i = address; i < address+size; i++)
    {
        const sysbit_t index { i/8 };
        const uchar_t offset { static_cast<uchar_t>(i - (index*8)) };
        this->allocationMap[index] &= ~(uchar_t{1} << (7-offset));
        this->data[i] = 0;
    }

    return System::ErrorCode::Ok;
}

RAM& RAM::operator=(RAM&& other)
{
    this->stackSize = other.stackSize;
    this->heapSize = other.heapSize;
    this->data = rval(other.data);
    this->allocationMap = rval(other.allocationMap);

    return *this;
}
