#pragma once

#include <cstring>

#include "slice.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

class FlatRAM
{
    public:
        FlatRAM() :
            allocationMap(nullptr),
            data(nullptr),
            stackSize(0),
            heapSize(0)
        { }

        FlatRAM(sysbit_t stackSize, sysbit_t heapSize) :
            stackSize(stackSize),
            heapSize(heapSize),
            data(std::make_unique_for_overwrite<char[]>(stackSize+heapSize)),
            allocationMap(std::make_unique<uchar_t[]>(heapSize/8))
        {
            // allocation map will hold 1 bit for each cell. 
            // so each byte refers to 8 cells. heap size must
            // be multiple of 8
            if (heapSize % 8 != 0)
                CRASH(
                    System::ErrorCode::Bad,
                    "heap size must be a multiple of 8."
                );
        }

        VM_INLINE FlatRAM& operator=(FlatRAM&& other)
        {
            this->stackSize = other.stackSize;
            this->heapSize = other.heapSize;
            this->data = rval(other.data);
            this->allocationMap = rval(other.allocationMap);
            return *this;
        }

        VM_INLINE char* const operator&() { return this->data.get(); }

        VM_INLINE char Read(const sysbit_t address) const
        {
            if (address >= (stackSize+heapSize)) [[unlikely]]
                CRASH(
                    System::ErrorCode::RAMAccessError, 
                    "const System::ErrorCode in RAM. Attempt to read out of bounds memory ", std::to_string(address)
                ); 
            return this->data[address];
        }

        VM_INLINE const System::ErrorCode Write(const sysbit_t address, char value) noexcept
        {
            if (address >= (stackSize+heapSize)) [[unlikely]]
                return System::ErrorCode::RAMAccessError;
            this->data[address] = value; 
            return System::ErrorCode::Ok;
        }

        VM_INLINE const Slice ReadSome(const sysbit_t address, const sysbit_t size) const
        {
            if (address >= (stackSize+heapSize) || (address+size) > (stackSize+heapSize)) [[unlikely]]
                CRASH(
                    System::ErrorCode::RAMAccessError, 
                    "const System::ErrorCode in RAM. Attempt to read out of bounds memory ", std::to_string(address)
                );
            return { this->data.get()+address, size };
        }

        VM_INLINE const System::ErrorCode WriteSome(const sysbit_t address, const Slice values) noexcept
        {
            const sysbit_t limit { this->stackSize+this->heapSize };
            if (address >= limit || (address+values.size) > limit) [[unlikely]]
            {
                LOGE(
                    System::LogLevel::Medium, 
                    "const System::ErrorCode in RAM. Attempt to write to out of bounds memory ",
                    std::to_string(address)
                );
                return System::ErrorCode::RAMAccessError;
            }

            std::memcpy(this->data.get()+address, values.data, values.size);
            return System::ErrorCode::Ok;
        }

        sysbit_t Allocate(sysbit_t size);
        const System::ErrorCode Deallocate(const sysbit_t address, const sysbit_t size) noexcept;

        VM_INLINE constexpr sysbit_t Size() const noexcept
        { return heapSize+stackSize; }

        VM_INLINE constexpr sysbit_t StackSize() const noexcept
        { return stackSize; }

        VM_INLINE constexpr sysbit_t HeapSize() const noexcept
        { return heapSize; }

    private:
        std::unique_ptr<uchar_t[]> allocationMap;
        std::unique_ptr<char[]> data;
        sysbit_t stackSize;
        sysbit_t heapSize;
};
