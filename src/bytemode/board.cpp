#include <bit>
#include <bitset>
#include <cassert>
#include <cstring>
#include <fstream>
#include <ios>
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
#ifndef NDEBUG
Board::~Board()
{
    std::cout << "\nCPU\n" << std::hex << std::uppercase
        << "pc : " << static_cast<sysbit_t>(this->cpu.DumpState().pc) 
        << " sp : " << static_cast<sysbit_t>(this->cpu.DumpState().sp) 
        << " bp : " << static_cast<sysbit_t>(this->cpu.DumpState().bp) << '\n' 
        << "eax : " << static_cast<sysbit_t>(this->cpu.DumpState().eax) 
        << " ebx : " << static_cast<sysbit_t>(this->cpu.DumpState().ebx)
        << " ecx : " << static_cast<sysbit_t>(this->cpu.DumpState().ecx) 
        << " edx : " << static_cast<sysbit_t>(this->cpu.DumpState().edx) 
        << " esi : " << static_cast<sysbit_t>(this->cpu.DumpState().esi) 
        << " edi : " << static_cast<sysbit_t>(this->cpu.DumpState().edi) << '\n' 
        << "al : " << static_cast<sysbit_t>(this->cpu.DumpState().al) 
        << " bl : " << static_cast<sysbit_t>(this->cpu.DumpState().bl) 
        << " cl : " << static_cast<sysbit_t>(this->cpu.DumpState().cl) 
        << " dl : " << static_cast<sysbit_t>(this->cpu.DumpState().dl) << '\n' 
        << "flg : " << static_cast<sysbit_t>(this->cpu.DumpState().flg) << '\n'
        << std::dec
        << "\nRAM Stack (" << (sysbit_t)ram.StackSize() << ")";
    for (sysbit_t i = 0; i < ram.StackSize(); i++)
    {
        if (i - 8*(i/8) == 0)
            std::cout << "\n0x" << std::hex << std::uppercase << i << " |";
        std::cout << ' ' << std::hex << std::uppercase << (sysbit_t)ram.Read(i) << " |";
    }

    std::cout << "\n\nRAM Heap (" << std::dec << ram.HeapSize() << ")";
    for (sysbit_t i = 0; i+ram.StackSize() < ram.Size(); i++)
    {
        if (i - 8*(i/8) == 0)
            std::cout << "\n0x" << std::hex << std::uppercase << i+ram.StackSize() << " |";
        std::cout << ' ' << std::hex << std::uppercase << (sysbit_t)ram.Read(i+ram.StackSize()) << " |";
    }
    std::cout << '\n';
}
#endif

Board::Board(class Assembly& assembly, sysbit_t id) 
    : assembly(assembly), cpu(*this), id(id)
{
    // CPU will be initialized beforehand, so it checks the ROM.
     
    // second 32 bits of ROM is stack size
    sysbit_t stackSize { IntegerFromBytes<sysbit_t>(assembly.Rom().ReadSome(4, 4).data)}; 

    // third 32 bits of ROM is heap size
    sysbit_t heapSize { IntegerFromBytes<sysbit_t>(assembly.Rom().ReadSome(8, 4).data)};
    
    // Create RAM
    this->ram = {
        stackSize,
        heapSize,
        *this
    };

    // CPU is already created. 

    // Create the initial process
    if (this->processes.size() == 0)
    {
        System::ErrorCode code { this->AddProcess() };

        if (code != System::ErrorCode::Ok)
            CRASH(
                code,
                "In ", this->Stringify(), " failed to initialize initial process. Error code: ",
                System::ErrorCodeString(code)
            );
    }
}

uchar_t Board::GenerateNewProcessID() const
{
    uchar_t id { 0 };
    for (; id <= std::numeric_limits<uchar_t>::max(); id++)
        if (!this->processes.contains(id))
            break;
    return id;
}

Error Board::ChangeExecutingProcess() noexcept
{
    // Dump current state of CPU to currentProcess
    this->processes.at(currentProcess).LoadState(this->cpu.DumpState());

    // set the next process to be the current
    this->currentProcess = std::min<uchar_t>(
                                this->currentProcess, 
                                this->processes.size()
                            );

    // Load the state from new state to CPU
    this->cpu.LoadState(this->processes.at(currentProcess).DumpState());

    return System::ErrorCode::Ok;
}

Error Board::AddProcess() noexcept
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

Error Board::RemoveProcess(uchar_t id) noexcept
{
    if (!this->processes.contains(id))
        return System::ErrorCode::InvalidSpecifier;

    this->processes.erase(id);

    return System::ErrorCode::Ok;
}

Error Board::Run() noexcept
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
            CRASH(
                System::ErrorCode::MessageSendError, 
                "Error in ", this->Stringify(),
                ". Couldn't send shutdown signal to ", this->Assembly().Stringify()
            );

        return code;
    }

    code = this->processes.at(this->currentProcess).Cycle();

//    if (code != System::ErrorCode::Ok)
//        LOGE(
//            System::LogLevel::Medium,
//            "In ", this->Stringify(), " error while running current process, id: ",
//            std::to_string(currentProcess),
//            ". Error code: ", System::ErrorCodeString(code)
//        );

    return code;
}

const std::string& Board::Stringify() const noexcept
{
    if (reprStr.size() != 0)
        return reprStr;

    std::stringstream ss;
    ss << this->assembly.Stringify() << '[' << this->id << ']';

    reprStr = ss.str();
    return reprStr;
}

//
// IMessageObject Implementation
//
Error Board::DispatchMessages() noexcept
{
    System::ErrorCode code { System::ErrorCode::Ok };

    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };

        if (message.type() == MessageType::PtoB)
        {
            // Process Interrupt, set currentProcess to next
            if (message.data()[1] == 0)
            {
                if (static_cast<uchar_t>(message.data()[0]) != currentProcess) 
                    return System::ErrorCode::InvalidSpecifier;
                code = this->ChangeExecutingProcess();
                if (code != System::ErrorCode::Ok)
                    LOGE(System::LogLevel::Medium, this->Stringify(), " error while dispatching messages ", System::ErrorCodeString(code));
            }

            // Process requests Shutdown
            else if (message.data()[1] == 1)
            {
                if (this->currentProcess == message.data()[0])
                    this->ChangeExecutingProcess(); 
                code = this->RemoveProcess(message.data()[0]);
                if (code != System::ErrorCode::Ok)
                    LOGE(System::LogLevel::Medium, this->Stringify(), " error while dispatching messages ", System::ErrorCodeString(code));
            }
        }

        this->messagePool.pop();
    }

    return code;
}

Error Board::ReceiveMessage(Message message) noexcept
{
    // message.type() must be PtoP, PtoB, AtoB
    // message.data() must be
    //      [targetId(1byte), senderID(1byte), message...]
    //      or
    //      [senderID(1byte), message...]
    //      or
    //      [targetId(4byte), message...]

    if (!VM::GetVM().GetSettings().strictMessages)
    {
        this->messagePool.push(message);
        return System::ErrorCode::Ok;
    }

    switch (message.type())
    {
        case MessageType::PtoP:
        // [targetId(1byte), senderID(1byte), message...]
        {
            if (!this->processes.contains(message.data()[0]) || !this->processes.contains(message.data()[1]))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::PtoB:
        // [senderID(1byte), message...]
        {
            if (!this->processes.contains(message.data()[0]))  
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

Error Board::SendMessage(Message message) noexcept
{
    // message.type() must be BtoP, BtoB, BtoA
    // message.data() must be
    //      [targetId(1byte), message...]
    //      or
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4byte), message...]

    bool check { VM::GetVM().GetSettings().strictMessages };

    switch (message.type())
    {
        case MessageType::BtoP:
        // [targetId(1byte), message...]
        {
            sysbit_t id { IntegerFromBytes<uchar_t>(message.data().get()) };
            if (check && !this->processes.contains(id))
                return System::ErrorCode::Bad;

            this->processes.at(id).ReceiveMessage(message);
        }
        break;

        case MessageType::BtoB:
        // [targetId(4bytes), senderID(4bytes), message...]
        {
            if (check && IntegerFromBytes<sysbit_t>(message.data().get()+4) != this->id)
                return System::ErrorCode::Bad;

            this->assembly.ReceiveMessage(message);
        }
        break;

        case MessageType::BtoA:
        // [senderId(4byte), message...]
        {
            if (check && IntegerFromBytes<sysbit_t>(message.data().get())) 
                return System::ErrorCode::Bad;

            this->assembly.ReceiveMessage(message);
        }
        break;

        default:
            return System::ErrorCode::Bad;
    }

    return System::ErrorCode::Ok;
}
