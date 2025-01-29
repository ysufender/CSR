#include <bit>
#include <bitset>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "extensions/syntaxextensions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// Board Implementation
//
Board::Board(class Assembly& assembly, sysbit_t id) 
    : parent(assembly), cpu(*this), id(id)
{
    // CPU will be initialized beforehand, so it checks the ROM.
     
    // second 32 bits of ROM is stack size
    sysbit_t stackSize { IntegerFromBytes<sysbit_t>(assembly.Rom()&4) }; 

    // third 32 bits of ROM is heap size
    sysbit_t heapSize { IntegerFromBytes<sysbit_t>(assembly.Rom()&8)};
    
    // Create RAM
    this->ram = {
        stackSize,
        heapSize,
        *this
    };

    // CPU is already created. 
}

uchar_t Board::GenerateNewProcessID() const
{
    uchar_t id { static_cast<uchar_t>(this->processes.size()) };
    while (this->processes.contains(id))
        id++;
    return id;
}

const System::ErrorCode Board::AddProcess() noexcept
{
    if (this->processes.size() >= std::numeric_limits<uchar_t>::max())
        return System::ErrorCode::Bad;

    uchar_t id { this->GenerateNewProcessID() };
    this->processes.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(*this, id)
    );

    return System::ErrorCode::Ok;
}

const System::ErrorCode Board::Run() noexcept
{
    // Dispatch messages
    System::ErrorCode code { this->DispatchMessages() }; 

    if (code != System::ErrorCode::Ok)
        return code;

    this->cpu.Cycle();

    // Send Shutdown Signal to Assembly
    if (this->processes.size() == 0)
    {
        std::unique_ptr<char[]> data { new char[5] };
        char* id { BytesFromInteger<sysbit_t, char>(this->id) };

        std::memcpy(data.get(), id, sizeof(sysbit_t));
        data[4] = 0;

        delete[] id;

        System::ErrorCode code { this->SendMessage({
            MessageType::BtoA,
            rval(data),
        })};

        if (code != System::ErrorCode::Ok)
            CRASH(
                System::ErrorCode::MessageSendError, 
                "Error in ", this->Stringify(),
                ". Couldn't send shutdown signal to ", this->Assembly().Stringify()
            );
    }

    return System::ErrorCode::Ok;
}

const std::string& Board::Stringify() const noexcept
{
    if (reprStr.size() != 0)
        return reprStr;

    std::stringstream ss;
    ss << this->parent.Stringify() << '[' << this->id << ']';

    reprStr = ss.str();
    return reprStr;
}

//
// IMessageObject Implementation
//
const System::ErrorCode Board::DispatchMessages() noexcept
{
    // TODO

    LOGE(System::LogLevel::Medium, "Board::DispatchMessages has not been implemented yet");
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };
        this->messagePool.pop();
    }

    return System::ErrorCode::Ok;
}

const System::ErrorCode Board::ReceiveMessage(Message message) noexcept
{
    // message.type() must be PtoP, PtoB, AtoB
    // message.data() must be
    //      [targetId(1byte), senderID(1byte), message...]
    //      or
    //      [senderID(1byte), message...]
    //      or
    //      [targetId(4byte), message...]

    if (!VM::GetVM().GetSettings().strictMessages)
        return System::ErrorCode::Ok;

    switch (message.type())
    {
        case MessageType::PtoP:
        // [targetId(1byte), senderID(1byte), message...]
        {
            sysbit_t target { IntegerFromBytes<sysbit_t>(message.data().get()) };
            sysbit_t sender { IntegerFromBytes<sysbit_t>(message.data().get()+4) };

            if (!this->processes.contains(target) || !this->processes.contains(sender))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::PtoB:
        // [senderID(1byte), message...]
        {
            if (!this->processes.contains(IntegerFromBytes<sysbit_t>(message.data().get())))  
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::AtoB:
            // [targetId(4byte), message...]
            {
                if (!this->processes.contains(IntegerFromBytes<sysbit_t>(message.data().get())))
                    return System::ErrorCode::Bad;
            }
            break;

        default:
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);
    return System::ErrorCode::Ok;
}

const System::ErrorCode Board::SendMessage(Message message) noexcept
{
    // message.type() must be BtoP, BtoB, BtoA
    // message.data() must be
    //      [targetId(1byte), message...]
    //      or
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4byte), message...]

    if (!VM::GetVM().GetSettings().strictMessages)
        return System::ErrorCode::Ok;

    switch (message.type())
    {
        case MessageType::BtoP:
        // [targetId(1byte), message...]
        {
            sysbit_t id { IntegerFromBytes<uchar_t>(message.data().get()) };
            if (!this->processes.contains(id))  
                return System::ErrorCode::Bad;

            this->processes.at(id).ReceiveMessage(message);
        }
        break;

        case MessageType::BtoB:
        // [targetId(4bytes), senderID(4bytes), message...]
        {
            if (IntegerFromBytes<sysbit_t>(message.data().get()+4) != this->id)
                return System::ErrorCode::Bad;

            this->parent.ReceiveMessage(message);
        }
        break;

        case MessageType::BtoA:
        // [senderId(4byte), message...]
        {
            if (IntegerFromBytes<sysbit_t>(message.data().get())) 
                return System::ErrorCode::Bad;

            this->parent.ReceiveMessage(message);
        }
        break;

        default:
            return System::ErrorCode::Bad;
    }

    return System::ErrorCode::Ok;
}


