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
    public:
        Board() = delete;
        Board(Board&) = delete;
        Board(Board&&) = delete;
        Board(class Assembly& assembly, sysbit_t id);

        const class Assembly& Assembly() const 
        { return this->parent; }


        const System::ErrorCode DispatchMessages() noexcept; 
        const System::ErrorCode ReceiveMessage(Message message) noexcept; 
        const System::ErrorCode SendMessage(Message message) noexcept; 

        const System::ErrorCode AddProcess() noexcept;
        const System::ErrorCode Run() noexcept;

        const std::string& Stringify() const noexcept;

        const sysbit_t id;
        RAM ram { *this };

    private:
        ProcessCollection processes;
        class Assembly& parent;
        CPU cpu; 

        mutable std::string reprStr;

        uchar_t GenerateNewProcessID() const;
};
