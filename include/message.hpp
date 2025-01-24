#pragma once

#include <functional>
#include <queue>

#include "system.hpp"

enum class MessageType : char
{
    PtoB,
        // data is [senderID(1byte), size(4bytes), message...]
    PtoP,
        // data is [targetId(1byte), senderID(1byte), size(4bytes), message...]
    BtoB,
        // data is [targetId(4bytes), senderID(4bytes), size(4bytes), message...]
    BtoA,
        // data is [senderId(1byte), size(4bytes), message...]
    AtoA,
        // data is [targetId(4bytes), senderID(4bytes), size(4bytes), message...]
    AtoV,
        // data is [senderId(4bytes), size(4bytes), message...]
    VtoA,
        // data is [targetId(4bytes), size(4bytes), message...]
};

struct Message
{
    Message(const MessageType type, const char* data) : type(type), data(data) 
    { }

    const MessageType type;
    const char* data;
};

class MessagePool
{
    public:
        const Message& front() const;
        void pop() const;
        bool empty() const noexcept;
        void push(const Message&& message);

    private:
        std::queue<Message> _underlingQueue { };
};

class IMessageObject
{
    public: 
        virtual const System::ErrorCode DispatchMessages() noexcept = 0;
        virtual const System::ErrorCode ReceiveMessage(const Message&& message) noexcept = 0;
        virtual const System::ErrorCode SendMessage(const Message&& message) const noexcept = 0;

    protected:
};
