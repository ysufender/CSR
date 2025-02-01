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
    ss << this->board.Stringify() << '[' << static_cast<int>(this->id) << ']';
    
    reprStr = ss.str();
    return reprStr;
}

Error Process::Cycle() noexcept
{
    System::ErrorCode code { this->DispatchMessages() };

    if (code != System::ErrorCode::Ok)
        LOGE(
            System::LogLevel::Medium,
            "In ", this->Stringify(),
            " error while dispatching messages. Error code: ", System::ErrorCodeString(code)
        );

    // Send Shutdown signal to board
    if (this->board.cpu.DumpState().pc >= this->board.assembly.Rom().Size())
    {
        std::unique_ptr<char[]> data { new char[2] };
        data[0] = this->id;
        data[1] = 1;

        System::ErrorCode code { this->SendMessage({
            MessageType::PtoB, 
            rval(data)
        })};

        if (code != System::ErrorCode::Ok)
            LOGE(
                System::LogLevel::Medium,
                "In ", this->Stringify(), " error while sending Shutdown signal to board."
            );
        
        return code;
    }

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
            " error in CPU cycle. Error code: ", System::ErrorCodeString(code)
        );
    

    return code;
}

//
// IMessageObject Implementation
//
Error Process::DispatchMessages() noexcept
{
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };

        this->messagePool.pop();
    }

    return System::ErrorCode::Ok;
}

Error Process::ReceiveMessage(Message message) noexcept
{
    if (!VM::GetVM().GetSettings().strictMessages)
    {
        this->messagePool.push(message);
        return System::ErrorCode::Ok;
    }

    // message.type() can only be BtoP
    if (message.type() != MessageType::BtoP)
        return System::ErrorCode::MessageReceiveError;

    // message.data() must be
    //      [targetId(1byte), message...]
    if (IntegerFromBytes<uchar_t>(message.data().get()) != this->id)
        return System::ErrorCode::MessageReceiveError;

    this->messagePool.push(message);
    return System::ErrorCode::Ok;
}

Error Process::SendMessage(Message message) noexcept
{
    if (!VM::GetVM().GetSettings().strictMessages)
        return this->board.ReceiveMessage(message);

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

    return this->board.ReceiveMessage(message);
}
