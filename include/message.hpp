#pragma once

#include <queue>

#include "system.hpp"

enum class MessageType : char
{
    PtoP, // receiver is B
        // data is [targetId(1byte), senderID(1byte), message...]

    PtoB, // receiver is B
        // data is [senderID(1byte), message...]

    BtoP, // receiver is P
        // data is [targetId(1byte), message...]

    BtoB, // receiver is A
        // data is [targetId(4bytes), senderID(4bytes), message...]

    BtoA, // receiver is A
        // data is [senderId(4byte), message...]

    AtoB, // receiver is B
        // data is [targetId(4byte), message...]
        
    AtoA, // receiver is V
        // data is [targetId(4bytes), senderID(4bytes), message...]

    AtoV, // receiver is V
        // data is [senderId(4bytes), message...]

    VtoA, // receiver is A
        // data is [targetId(4bytes), message...]
};

struct Message
{
    const MessageType type;
    const char* data;
};

class MessagePool
{
    public:
        inline const Message& front() const 
        { return this->_underlyingQueue.front(); }

        inline void pop() noexcept
        { this->_underlyingQueue.pop(); }

        bool empty() const noexcept
        { return this->_underlyingQueue.empty(); }

        void push(const Message message) noexcept
        { this->_underlyingQueue.push(message); }

    private:
        std::queue<Message> _underlyingQueue { };
};

class IMessageObject
{
    public: 
        virtual const System::ErrorCode DispatchMessages() noexcept = 0;
        virtual const System::ErrorCode ReceiveMessage(const Message message) noexcept = 0;
        virtual const System::ErrorCode SendMessage(const Message message) const noexcept = 0;

    protected:
        MessagePool messagePool { };
};
