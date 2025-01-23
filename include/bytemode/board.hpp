#pragma once

#include <unordered_map>

#include "CSRConfig.hpp"
#include "bytemode/process.hpp"

using ProcessCollection = std::unordered_map<uchar_t, Process>;

class Assembly;

class RAM
{
    friend class Board;

    public:
        RAM() = default;
        RAM(RAM&) = delete;
        void operator=(RAM const&) = delete;
        void operator=(RAM const&&) = delete;

        char Read(const systembit_t index) const;
        bool Write(const systembit_t index, char value) noexcept;

        systembit_t Allocate(const systembit_t size);
        bool Deallocate(const systembit_t address, const systembit_t size);

    private:
        char* allocationMap = nullptr;
        char* data = nullptr;
        systembit_t stackSize = 0;
        systembit_t heapSize = 0;
};

class Board
{
    public:
        Board() = delete;
        Board(const Assembly& assembly);

    private:
        ProcessCollection processes;
        const Assembly& parent;
        RAM ram;
        //const CPU cpu; 
};
