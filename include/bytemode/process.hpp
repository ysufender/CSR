#pragma once

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
        Process(Board& parent, uchar_t id);

        // TODO: Implement IMessageObject for Process
        const System::ErrorCode DispatchMessages() noexcept override;
        const System::ErrorCode ReceiveMessage(Message message) noexcept override;
        const System::ErrorCode SendMessage(Message message) noexcept override;

        const uchar_t id;

    private:
        Board& parent;
};
