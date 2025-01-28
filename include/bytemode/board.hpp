#pragma once

#include <string>
#include <unordered_map>

#include "bytemode/process.hpp"
#include "bytemode/cpu.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"

using ProcessCollection = std::unordered_map<uchar_t, Process>;

class Assembly;

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

        inline RAM(sysbit_t stackSize, sysbit_t heapSize, const Board& board) :
            stackSize(stackSize),
            heapSize(heapSize),
            data(std::make_unique<char[]>(stackSize+heapSize)),
            allocationMap(std::make_unique<char[]>(heapSize/8)),
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
        std::unique_ptr<char[]> allocationMap;
        std::unique_ptr<char[]> data;
        sysbit_t stackSize;
        sysbit_t heapSize;
        const Board& board;
};

class Board : IMessageObject
{
    public:
        Board() = delete;
        Board(Board&) = delete;
        Board(Board&&) = delete;
        Board(class Assembly& assembly, sysbit_t id);

        class RAM& RAM() 
        { return this->ram; }

        const class Assembly& Assembly() const 
        { return this->parent; }


        const System::ErrorCode DispatchMessages() noexcept; 
        const System::ErrorCode ReceiveMessage(Message message) noexcept; 
        const System::ErrorCode SendMessage(Message message) noexcept; 

        const System::ErrorCode AddProcess() noexcept;
        const System::ErrorCode Run() noexcept;

        const std::string& Stringify() const noexcept;

        const sysbit_t id;

    private:
        ProcessCollection processes;
        class Assembly& parent;
        class RAM ram { *this };
        CPU cpu; 

        mutable std::string reprStr;

        uchar_t GenerateNewProcessID() const;
};
