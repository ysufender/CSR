#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "bytemode/process.hpp"
#include "bytemode/cpu.hpp"
#include "bytemode/ram.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"

using ProcessCollection = std::unordered_map<uchar_t, Process>;

class Assembly;

class Board : IMessageObject
{
    friend class Process;

    public:
        Board() = delete;
        Board(Board&) = delete;
        Board(Board&&) = delete;
        Board(class Assembly& assembly, sysbit_t id);

        const class Assembly& Assembly() const 
        { return this->assembly; }


        const System::ErrorCode DispatchMessages() noexcept; 
        const System::ErrorCode ReceiveMessage(Message message) noexcept; 
        const System::ErrorCode SendMessage(Message message) noexcept; 

        const System::ErrorCode AddProcess() noexcept;
        const System::ErrorCode Run() noexcept;

        const std::string& Stringify() const noexcept;

        uchar_t GetExecutingProcess() const noexcept
        { return this->currentProcess; }

        const sysbit_t id;

    private:
        uchar_t GenerateNewProcessID() const;

        ProcessCollection processes;
        uchar_t currentProcess { 0 };

        class Assembly& assembly;
        RAM ram { *this };
        CPU cpu; 

        mutable std::string reprStr;
};
