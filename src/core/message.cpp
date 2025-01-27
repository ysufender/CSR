#include "extensions/syntaxextensions.hpp"
#include "message.hpp"

Message::Message(Message& other)
{
    this->_data = rval(other._data);
    this->_type = other._type;

    other._data = nullptr;
}

Message::Message(Message&& other)
{
    this->_data = rval(other._data);
    this->_type = other._type;

    other._data = nullptr;
}

Message::Message(MessageType type, std::unique_ptr<char[]> data)
{
    this->_type = type;
    this->_data = rval(data);
}
