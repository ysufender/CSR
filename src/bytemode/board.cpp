#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "bytemode/board.hpp"
#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
#include "extensions/converters.hpp"
#include "extensions/syntaxextensions.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

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
            CRASH(System::ErrorCode::MessageSendError, "Error, couldn't send shutdown signal to");
    }

    return System::ErrorCode::Ok;
}

//
// RAM Implementation
//
char RAM::Read(const sysbit_t address) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        CRASH(System::ErrorCode::RAMAccessError, "Attempt to read out of bounds memory '", std::to_string(address), "'");
    
    return this->data[address];
}

const char* RAM::ReadSome(const sysbit_t address, const sysbit_t size) const
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        CRASH(System::ErrorCode::RAMAccessError, "Attempt to read out of bounds memory '", std::to_string(address), "'");

    return this->data+address;
}

System::ErrorCode RAM::Write(const sysbit_t address, char value) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0)
        return System::ErrorCode::Ok;
    this->data[address] = value; 
    return System::ErrorCode::Ok;
}

System::ErrorCode RAM::WriteSome(const sysbit_t address, const sysbit_t size, char* values) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        CRASH(System::ErrorCode::RAMAccessError, "Attempt to read out of bounds memory '", std::to_string(address), "'");

    System::ErrorCode status = System::ErrorCode::Ok;
    for(sysbit_t i = 0; i < size; i++)
        status = status == System::ErrorCode::Ok ? this->Write(address+i, values[i]) : status;
    return status;
}

sysbit_t RAM::Allocate(const sysbit_t size)
{
    sysbit_t counter { size };
    sysbit_t allocationAddress { 0 };
    for (sysbit_t i = 0; i < this->heapSize/8; i++)
    {
        sysbit_t index { i/8 };
        char offset { static_cast<char>(i - index/8) };

        const bool flag = (static_cast<uchar_t>(this->allocationMap[index]) >> (8 - offset)) & 1;
        if (flag)
        {
            counter = size;
            allocationAddress = 0;
            continue;
        }
        allocationAddress = allocationAddress == 0 ? i : allocationAddress; 
        counter--;
        if (counter == 0)
            break;
    }
    if (counter != 0)
        CRASH(System::ErrorCode::HeapOverflow, "Couldn't allocate memory on heap, board is out of memory.");
    for (sysbit_t i = 0; i < size; i++)
    {
        sysbit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) };
        this->allocationMap[index] |= (uchar_t{1} << (8-offset));
    }
    return allocationAddress;
}

System::ErrorCode RAM::Deallocate(const sysbit_t address, const sysbit_t size) noexcept
{
    if (address >= (this->stackSize+this->heapSize) || address < 0 || (address+size) > this->stackSize+this->heapSize)
        CRASH(System::ErrorCode::RAMAccessError, "Attempt to read out of bounds memory '", std::to_string(address), "'");

    for (sysbit_t i = 0; i < size; i++)
    {
        sysbit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 
        const bool flag = (static_cast<uchar_t>(this->allocationMap[index]) >> (8 - offset)) & 1;
        if (!flag)
            return System::ErrorCode::Bad;
    }
    for (sysbit_t i = 0; i < size; i++)
    {
        sysbit_t index { i/8 };
        char offset { static_cast<char>(i - index*8) }; 

        data[address+offset+index*8] = 0;
        allocationMap[index] |= ~((uchar_t{1} << (8 - offset))); 
    }

    return System::ErrorCode::Ok;
}

RAM::RAM(sysbit_t stackSize, sysbit_t heapSize)
    : stackSize(stackSize), heapSize(heapSize)
{
    this->data = new char[stackSize+heapSize];

    // allocation map will hold 1 bit for each cell. 
    // so each byte refers to 8 cells. heap size must
    // be multiple of 8
    this->allocationMap = new char[heapSize/8];
}

void RAM::operator=(RAM&& other)
{
    this->stackSize = other.stackSize;
    this->heapSize = other.heapSize;
    this->data = other.data;
    this->allocationMap = other.allocationMap;

    other.stackSize = 0;
    other.heapSize = 0;
    other.data = nullptr;
    other.allocationMap = nullptr;
}

RAM::~RAM()
{
    if (this->data)
        delete[] this->data;
    if (this->allocationMap)
        delete[] this->allocationMap;
}
