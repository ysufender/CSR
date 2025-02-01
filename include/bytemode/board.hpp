#pragma once

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
    friend class CPU;

    public:
        Board() = delete;
        Board(Board&) = delete;
        Board(Board&&) = delete;
        Board(class Assembly& assembly, sysbit_t id);

#ifndef NDEBUG
        ~Board()
        {
            for (sysbit_t i = 0; i < ram.Size() / 8; i++)
            {
                std::cout << i*8 << ": ";
                for (sysbit_t j = 0; j < 8; j++)
                    std::cout << static_cast<int>(ram.Read(i*8+j)) << ' ';
                std::cout << '\n';
            }
        }
#endif

        const class Assembly& Assembly() const 
        { return this->assembly; }

        Error DispatchMessages() noexcept; 
        Error ReceiveMessage(Message message) noexcept; 
        Error SendMessage(Message message) noexcept; 

        Error ChangeExecutingProcess() noexcept;
        Error AddProcess() noexcept;
        Error RemoveProcess(uchar_t id) noexcept;
        Error Run() noexcept;

        const std::string& Stringify() const noexcept;

        const Process& GetExecutingProcess() const noexcept
        { return this->processes.at(this->currentProcess); }

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
