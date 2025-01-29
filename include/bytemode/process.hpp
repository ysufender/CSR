#pragma once

#include "bytemode/cpu.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"

class Board;

class Process : IMessageObject
{
    public:
        Process() = delete;
        Process(Process&) = delete;
        Process(Process&&) = delete;
        Process(Board& parent, uchar_t id) : board(parent), id(id)
        { }

        const System::ErrorCode DispatchMessages() noexcept override;
        const System::ErrorCode ReceiveMessage(Message message) noexcept override;
        const System::ErrorCode SendMessage(Message message) noexcept override;

        const std::string& Stringify() const noexcept;

        const CPU::State& DumpState() const noexcept
        { return this->state; }

        void LoadState(const CPU::State& loadFrom) noexcept
        { this->state = loadFrom; }

        const System::ErrorCode Cycle() noexcept;

        const uchar_t id;

    private:
        Board& board;
        CPU::State state;

        mutable std::string reprStr;
};
