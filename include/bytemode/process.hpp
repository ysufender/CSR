#pragma once

#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"

class Process : IMessageObject
{
    public:
        Process(uchar_t id) : id(id)
        { }

        // TODO: Implement IMessageObject for Process
        const System::ErrorCode DispatchMessages() noexcept override
        {
            return System::ErrorCode::Ok;
        }
        const System::ErrorCode ReceiveMessage(const Message message) noexcept override
        {
            return System::ErrorCode::Ok;
        }
        const System::ErrorCode SendMessage(const Message message) noexcept override
        {
            return System::ErrorCode::Ok;
        }

        const uchar_t id;
};
