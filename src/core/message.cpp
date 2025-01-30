#include "extensions/syntaxextensions.hpp"
#include "message.hpp"

Message::Message(Message& other)
{
    this->_type = other._type;
    this->_data = rval(other._data);
}

Message::Message(Message&& other)
{
    this->_type = other._type;
    this->_data = rval(other._data);
}

Message::Message(MessageType type, std::unique_ptr<char[]> data)
{
    this->_type = type;
    this->_data = rval(data);
}
