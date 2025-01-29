#pragma once

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
        }

        void operator=(RAM&& other);

        char Read(const sysbit_t address) const;
        const System::ErrorCode Write(const sysbit_t address, char value) noexcept;

        const char* ReadSome(const sysbit_t address, const sysbit_t size) const;
        const System::ErrorCode WriteSome(const sysbit_t address, const sysbit_t size, const char* values) noexcept;

        sysbit_t Allocate(const sysbit_t size);
        const System::ErrorCode Deallocate(const sysbit_t address, const sysbit_t size) noexcept;

    private:
        std::unique_ptr<uchar_t[]> allocationMap;
        std::unique_ptr<char[]> data;
        sysbit_t stackSize;
        sysbit_t heapSize;
        const Board& board;
};
