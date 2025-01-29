#include <memory>
#include <sstream>
#include <string>

#include "bytemode/cpu.hpp"
#include "extensions/converters.hpp"
#include "bytemode/process.hpp"
#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// Process Implementation
//
const std::string& Process::Stringify() const noexcept
{
    if (this->reprStr.size() != 0)
        return this->reprStr;

    std::stringstream ss;
    ss << this->board.Stringify() << '[' << this->id << ']';
    
    reprStr = ss.str();
    return reprStr;
}

const System::ErrorCode Process::Cycle() noexcept
{
    System::ErrorCode code { this->DispatchMessages() };

    if (code != System::ErrorCode::Ok)
        LOGE(
            System::LogLevel::Medium,
            "In ", this->Stringify(),
            " error while dispatching messages. Error code: ", std::to_string(static_cast<int>(code))
        );

    OpCodes op { this->board.Assembly().Rom()[this->board.cpu.DumpState().pc] };

    // New callStack will be initialized, or destroyed
    // either way that means it's interrupt for this process.
    // Send message to Board to switch to the next process.
    if (op == OpCodes::cal || op == OpCodes::calr || op == OpCodes::ret)
    {
        std::unique_ptr<char[]> data { std::make_unique_for_overwrite<char[]>(2) };
        data[0] = this->id;
        data[1] = 0;
        this->SendMessage({MessageType::PtoB, rval(data)});
    }

    code = this->board.cpu.Cycle();

    if (code != System::ErrorCode::Ok)
        LOGE(
            System::LogLevel::Medium,
            "In ", this->Stringify(),
            " error in CPU cycle. Error code: ", std::to_string(static_cast<int>(code))
        );

    return code;
}

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

    this->board.ReceiveMessage(message);
    return System::ErrorCode::Ok;
}
