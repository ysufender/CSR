#include "message.hpp"
#include "system.hpp"

Message::Message(Message& other)
{
    this->_data = other._data;
    this->_type = other._type;

    other._data = nullptr;
}

Message::Message(Message&& other)
{
    this->_data = other._data;
    this->_type = other._type;

    other._data = nullptr;
}

Message::Message(MessageType type, char* data)
{
    this->_type = type;
    this->_data = data;
}

Message::~Message()
{
    if (this->_data != nullptr)
        delete[] _data;
}
