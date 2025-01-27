#include "bytemode/process.hpp"
#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
#include "bytemode/board.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// IMessageObject Implementation
//
const System::ErrorCode Process::DispatchMessages() noexcept
{
    // TODO

    LOGE(System::LogLevel::Medium, "Process::DispatchMessages has not been implemented yet");
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };
        this->messagePool.pop();
    }

    return System::ErrorCode::Ok;
}

const System::ErrorCode Process::ReceiveMessage(Message message) noexcept
{
    if (!VM::GetVM().GetSettings().strictMessages)
        return System::ErrorCode::Ok;

    // message.type() can only be BtoP
    if (message.type() != MessageType::BtoP)
        return System::ErrorCode::Bad;

    // message.data() must be
    //      [targetId(1byte), message...]
    if (IntegerFromBytes<uchar_t>(message.data().get()) != this->id)
        return System::ErrorCode::Bad;

    this->messagePool.push(message);
    return System::ErrorCode::Ok;
}

const System::ErrorCode Process::SendMessage(Message message) noexcept
{
    if (!VM::GetVM().GetSettings().strictMessages)
        return System::ErrorCode::Ok;

    // message.type() can be either PtoP or PtoB
    // message.data() must be
    //      [targetId(1byte), senderId(1byte), message...]
    //      or
    //      [senderId(1byte), message...]
    if (message.type() != MessageType::PtoP && message.type() != MessageType::PtoB)
        return System::ErrorCode::Bad;

    const char* senderOffset { message.type() == MessageType::PtoP ? message.data().get()+1 : message.data().get() };
    
    if (IntegerFromBytes<uchar_t>(senderOffset) != this->id)
        return System::ErrorCode::Bad;

    this->parent.ReceiveMessage(message);
    return System::ErrorCode::Ok;
}

//
// Process Implementation
//
Process::Process(Board& parent, uchar_t id) : parent(parent), id(id)
{ }
