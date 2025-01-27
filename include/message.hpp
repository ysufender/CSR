#pragma once

#include <memory>
#include <vector>

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

class Message
{
    public:
        Message() = delete;
        Message(Message& other);
        Message(Message&& other);
        Message(MessageType type, std::unique_ptr<char[]> data);

        inline const MessageType type() const { return _type; }
        inline const std::unique_ptr<char[]>& data() const { return _data; }

    private:
        MessageType _type = MessageType::PtoP;
        std::unique_ptr<char[]> _data = nullptr;

};

class MessagePool
{
    public:
        inline const Message& front() const 
        { return this->_underlyingVec.at(this->_underlyingVec.size()-1); }

        inline void pop() noexcept
        { this->_underlyingVec.pop_back(); }

        bool empty() const noexcept
        { return this->_underlyingVec.empty(); }

        void push(Message message) noexcept
        { this->_underlyingVec.emplace_back(message); }

    private:
        std::vector<Message> _underlyingVec { };
};

class IMessageObject
{
    public: 
        virtual const System::ErrorCode DispatchMessages() noexcept = 0;
        virtual const System::ErrorCode ReceiveMessage(Message message) noexcept = 0;
        virtual const System::ErrorCode SendMessage(Message message) noexcept = 0;

    protected:
        MessagePool messagePool { };
};
