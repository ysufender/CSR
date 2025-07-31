#include "CSRConfig.hpp"
#include "bytemode/board.hpp"
#include "system.hpp"
#include <cassert>
#include <cstring>
#include <string>
#include "bytemode/ram.hpp"

//
// RAM Implementation
//
void RAM::crashRead(const sysbit_t address) const
{
    CRASH(
        System::ErrorCode::RAMAccessError, 
        "Error in ", this->board.Stringify(),
        " Attempt to read out of bounds memory ", std::to_string(address)
    );
}

Error RAM::WriteSome(const sysbit_t address, const Slice values) noexcept
{
    const sysbit_t limit { this->stackSize+this->heapSize };
    if (address >= limit || address < 0 || (address+values.size) > limit)
    {
        LOGE(
            System::LogLevel::Medium, 
            "Error in ", this->board.Stringify(),
            ". Attempt to write to out of bounds memory ",
            std::to_string(address)
        );
        return System::ErrorCode::RAMAccessError;
    }

    std::memcpy(this->data.get()+address, values.data, values.size);
    return System::ErrorCode::Ok;
}

sysbit_t RAM::Allocate(sysbit_t size)
{
    sysbit_t counter = size;
    sysbit_t allocationAddr = 0;
    bool set = false;

    for (sysbit_t i = this->StackSize(); i < this->Size(); i++)
    {
        if (i + counter > this->Size()) break;

        if (counter == 0)
            break;

        const sysbit_t reali = i - this->StackSize();
        const sysbit_t index = reali / 8;
        const uchar_t offset = static_cast<uchar_t>(reali % 8);

        const bool isAvailable = ((this->allocationMap[index] >> offset) & 1) == 0;

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
        CRASH(System::ErrorCode::HeapOverflow,
              "Can't allocate memory of size ", std::to_string(size),
              " bytes from ", this->board.Stringify(), ". Board is out of memory.");
    else if (!set)
        CRASH(System::ErrorCode::FragmentedHeap,
              "Can't allocate memory of size ", std::to_string(size),
              " bytes from ", this->board.Stringify(), ". No suitable fragment found on heap.");

    for (sysbit_t i = allocationAddr - this->StackSize(); size > 0; i++, size--)
    {
        const sysbit_t index = i / 8;
        const uchar_t offset = static_cast<uchar_t>(i % 8);
        this->allocationMap[index] |= (uchar_t{1} << offset);
    }

    return allocationAddr;
}

Error RAM::Deallocate(const sysbit_t address, const sysbit_t size) noexcept
{
    if (address >= (this->stackSize + this->heapSize) || address < 0 ||
        (address + size) > this->stackSize + this->heapSize)
    {
        LOGE(System::LogLevel::Medium, 
             "Error in ", this->board.Stringify(),
             ". Attempt to read out of bounds memory ", std::to_string(address));
        return System::ErrorCode::RAMAccessError;
    }

    for (sysbit_t i = address - this->StackSize(); i < (address - this->StackSize()) + size; ++i)
    {
        const sysbit_t index = i / 8;
        const uchar_t offset = static_cast<uchar_t>(i % 8);
        if ((this->allocationMap[index] & (1 << offset)) == 0) {
            LOGE(System::LogLevel::Medium,
                "Error in ", this->board.Stringify(),
                ". Attemt to double free memory ", std::to_string(address)
            );
            return System::ErrorCode::DoubleFree;
        }
        this->allocationMap[index] &= ~(uchar_t{1} << offset);
    }

    return System::ErrorCode::Ok;
}
