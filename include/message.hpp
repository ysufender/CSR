#pragma once

#include <memory>
#include <vector>

#include "extensions/syntaxextensions.hpp"
#include "system.hpp"


#define MTER(E) \
    E(PtoB) \
    E(BtoP) \
    E(BtoB) \
    E(BtoA) \
    E(AtoB) \
    E(AtoA) \
    E(AtoV) \
    E(VtoA)
MAKE_ENUM(MessageType, PtoP, 0, MTER, OUT_CLASS)
#undef MTER

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

        size_t size() const noexcept
        { return this->_underlyingVec.size(); }

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
