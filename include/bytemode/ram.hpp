#pragma once

#include "slice.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

class Board;

class RAM
{
    public:
        RAM(const Board& board) :
            allocationMap(nullptr),
            data(nullptr),
            stackSize(0),
            heapSize(0),
            board(board)
        { }

        RAM(sysbit_t stackSize, sysbit_t heapSize, const Board& board) :
            stackSize(stackSize),
            heapSize(heapSize),
            data(std::make_unique_for_overwrite<char[]>(stackSize+heapSize)),
            allocationMap(std::make_unique<uchar_t[]>(heapSize/8)),
            board(board)
        {
            // allocation map will hold 1 bit for each cell. 
            // so each byte refers to 8 cells. heap size must
            // be multiple of 8
            if (heapSize % 8 != 0)
                CRASH(
                    Error::Bad,
                    "heap size must be a multiple of 8."
                );
        }

        inline RAM& operator=(RAM&& other)
        {
            this->stackSize = other.stackSize;
            this->heapSize = other.heapSize;
            this->data = rval(other.data);
            this->allocationMap = rval(other.allocationMap);

            return *this;
        }

        inline char Read(const sysbit_t address) const
        {
            if (address >= (this->stackSize+this->heapSize) || address < 0)
                crashRead(address); 
            return this->data[address];
        }

        inline Error Write(const sysbit_t address, char value) noexcept
        {
            if (address >= (this->stackSize+this->heapSize) || address < 0)
                return System::ErrorCode::RAMAccessError;
            this->data[address] = value; 
            return System::ErrorCode::Ok;
        }

        inline const Slice ReadSome(const sysbit_t address, const sysbit_t size) const
        {
            if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
                crashRead(address); 

            return { this->data.get()+address, size };
        }

        Error WriteSome(const sysbit_t address, const Slice values) noexcept;

        sysbit_t Allocate(sysbit_t size);
        Error Deallocate(const sysbit_t address, const sysbit_t size) noexcept;

        inline constexpr sysbit_t Size() const noexcept
        { return heapSize+stackSize; }

        inline constexpr sysbit_t StackSize() const noexcept
        { return stackSize; }

        inline constexpr sysbit_t HeapSize() const noexcept
        { return heapSize; }

    private:
        void crashRead(const sysbit_t address) const;
        std::unique_ptr<uchar_t[]> allocationMap;
        std::unique_ptr<char[]> data;
        sysbit_t stackSize;
        sysbit_t heapSize;
        const Board& board;
};
