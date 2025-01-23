#pragma once

#include <unordered_map>

#include "CSRConfig.hpp"
#include "bytemode/process.hpp"
#include "bytemode/cpu.hpp"
#include "system.hpp"

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

        char Read(const systembit_t address) const;
        System::ErrorCode Write(const systembit_t address, char value) noexcept;

        char* ReadSome(const systembit_t address, const systembit_t size) const;
        System::ErrorCode WriteSome(const systembit_t address, const systembit_t size, char* values) noexcept;

        systembit_t Allocate(const systembit_t size);
        System::ErrorCode Deallocate(const systembit_t address, const systembit_t size) noexcept;

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

        inline class RAM& RAM() { return this->ram; }
        inline const class Assembly& Assembly() { return this->parent; }

    private:
        ProcessCollection processes;
        const class Assembly& parent;
        class RAM ram;
        CPU cpu; 
};
