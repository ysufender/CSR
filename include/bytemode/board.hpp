#pragma once

#include <unordered_map>

#include "CSRConfig.hpp"
#include "bytemode/process.hpp"
#include "bytemode/cpu.hpp"
#include "message.hpp"
#include "system.hpp"

using ProcessCollection = std::unordered_map<uchar_t, Process>;

class Assembly;

class RAM
{
    public:
        RAM() : allocationMap(nullptr), data(nullptr), stackSize(0), heapSize(0)
        { }

        RAM(sysbit_t stackSize, sysbit_t heapSize);

        ~RAM();

        void operator=(RAM&& other);

        char Read(const sysbit_t address) const;
        System::ErrorCode Write(const sysbit_t address, char value) noexcept;

        const char* ReadSome(const sysbit_t address, const sysbit_t size) const;
        System::ErrorCode WriteSome(const sysbit_t address, const sysbit_t size, char* values) noexcept;

        sysbit_t Allocate(const sysbit_t size);
        System::ErrorCode Deallocate(const sysbit_t address, const sysbit_t size) noexcept;

    private:
        char* allocationMap;
        char* data;
        sysbit_t stackSize;
        sysbit_t heapSize;
};

class Board : IMessageObject
{
    public:
        Board() = delete;
        Board(Board&) = delete;
        Board(Board&&) = delete;
        Board(class Assembly& assembly, sysbit_t id);

        inline class RAM& RAM() { return this->ram; }
        inline const class Assembly& Assembly() { return this->parent; }

        const System::ErrorCode DispatchMessages() noexcept; 
        const System::ErrorCode ReceiveMessage(Message message) noexcept; 
        const System::ErrorCode SendMessage(Message message) noexcept; 

        const System::ErrorCode AddProcess() noexcept;
        
        const System::ErrorCode Run() noexcept;

        const sysbit_t id;

    private:
        ProcessCollection processes;
        class Assembly& parent;
        class RAM ram;
        CPU cpu; 

        uchar_t GenerateNewProcessID() const;
};
