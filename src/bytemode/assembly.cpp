#include <cassert>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "extensions/converters.hpp"
#include "extensions/streamextensions.hpp"
#include "extensions/syntaxextensions.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// Assembly Implementation
//
Assembly::Assembly(Assembly::AssemblySettings&& settings) :
    settings(settings),
    syscallHandler()
{ }

Error Assembly::Load() noexcept
{
    if (!std::filesystem::exists(this->settings.path))
        return System::ErrorCode::SourceFileNotFound;

    std::string extension { this->settings.path.extension().string() };

    if (extension == ".jef")
        this->settings.type = AssemblyType::Executable;
    else if (extension == ".shd")
        this->settings.type = AssemblyType::Library;
    else if (extension == ".stc")
    {
        LOGW(
                "The file (", 
                this->settings.path.generic_string(), 
                ") you provided is a static library and can't be handled by the VM."
            );
        return System::ErrorCode::UnsupportedFileType;
    }
    else 
    {
        LOGW(
            "The file (",
            this->settings.path.generic_string(),
            ") you provided has an unrecognized extension and can't be handled by the VM"
        );
        return System::ErrorCode::UnsupportedFileType;
    }

    std::ifstream bytecode { System::OpenInFile(this->settings.path) };
    bytecode.seekg(0, std::ios::end);
    IStreamPos(bytecode, length, {
        bytecode.close();
        return System::ErrorCode::FileIOError;
    });
    bytecode.seekg(0, std::ios::beg);

    std::unique_ptr<char[]> data { new char[length] };
    bytecode.read(data.get(), length);
    bytecode.close();

    this->rom.data = rval(data);
    this->rom.size = static_cast<sysbit_t>(length);

    // If the assembly is a library, no need to initialize boards,
    if (this->settings.type == AssemblyType::Library)
        return System::ErrorCode::Ok;

    // initialize the initial board.
    System::ErrorCode err;
    try_catch(
        if (this->boards.size() == 0)
            err = this->AddBoard();,

        LOGE(System::LogLevel::Medium, this->Stringify(), " ROM access error while initializing Board.");
        return exc.GetCode();,

        std::cerr << "Unexpected error in " << this->Stringify() << '\n';
        return System::ErrorCode::UnhandledException;
    );

    return err;
}

const std::string& Assembly::Stringify() const noexcept
{
    if (reprStr.size() != 0)
        return reprStr;

    std::stringstream ss;
    ss << '[' << this->settings.name << ':' << this->settings.id << ']'; 

    reprStr = ss.str();
    return reprStr;
}

sysbit_t Assembly::GenerateNewBoardID() const
{
    sysbit_t id { 0 };
    for (; id <= std::numeric_limits<sysbit_t>::max(); id++)
        if (!this->boards.contains(id))
            break;
    return id;
}

Error Assembly::AddBoard() noexcept
{
    if (this->boards.size() >= std::numeric_limits<sysbit_t>::max())
        return System::ErrorCode::Bad;
    
    sysbit_t id { this->GenerateNewBoardID() };
    this->boards.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(*this, id)
    );

    return System::ErrorCode::Ok;
}

Error Assembly::RemoveBoard(sysbit_t id) noexcept
{
    if (!this->boards.contains(id))
        return System::ErrorCode::InvalidSpecifier;

    this->boards.erase(id);

    return System::ErrorCode::Ok;
}

Error Assembly::Run() noexcept
{
    System::ErrorCode code { this->DispatchMessages() };

    if (code != System::ErrorCode::Ok)
        return code;

    // Send Shutdown Signal to VM
    if (this->boards.size() == 0)
    {
        std::unique_ptr<char[]> data { new char[5] };
        char* id { BytesFromInteger<sysbit_t, char>(this->settings.id) };

        std::memcpy(data.get(), id, sizeof(sysbit_t));
        data[4] = 0;

        delete[] id;

        System::ErrorCode code { this->SendMessage({
            MessageType::AtoV,
            rval(data),
        })};

        if (code != System::ErrorCode::Ok)
            CRASH(System::ErrorCode::MessageSendError, "Error, couldn't send shutdown signal to VM");
    }

    for (auto& [id, board] : this->boards)
    {
        try_catch(
            code = board.Run();

//            if (code != System::ErrorCode::Ok)
//                LOGE(
//                    System::LogLevel::Low,
//                    "Error while running board ", board.Stringify(),
//                    " Error code: ", System::ErrorCodeString(code)
//                );,
            ,

            LOGE(
                System::LogLevel::Low, 
                "Error while running ", board.Stringify()
            );,

            LOGE(
                System::LogLevel::Medium, 
                "Fatal unexpected error while running board ", board.Stringify()
            );
        )
    }

    return code;
}

//
// IMessageObject Implementation
//
Error Assembly::DispatchMessages() noexcept
{
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };
        System::ErrorCode code { System::ErrorCode::Ok };

        if (message.type() == MessageType::BtoA)
        {
            if (message.data()[4] == 0)
            {
                sysbit_t id { IntegerFromBytes<sysbit_t>(message.data().get()) };
                this->RemoveBoard(id);
            }
        }
        else
            LOGE(
                System::LogLevel::Low, 
                "Unhandled message, type: ",
                MessageTypeString(message.type())
            );

        if (code != System::ErrorCode::Ok)
            LOGE(
                System::LogLevel::Medium,
                "Message dispatch exited with code ", System::ErrorCodeString(code),
                ". Message type: ", MessageTypeString(message.type())
            );

        this->messagePool.pop();
    }
    return System::ErrorCode::Ok;
}

Error Assembly::ReceiveMessage(Message message) noexcept
{
    // message.type() must be BtoB, BtoA or VtoA
    // data must be
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4byte), message...]
    //      or
    //      [targetId(4bytes), message...]
    // check the first 4bytes to verify the sender/target

    if (!VM::GetVM().GetSettings().strictMessages)
    {
        this->messagePool.push(message);
        return System::ErrorCode::Ok;
    }

    switch (message.type())
    {
        case MessageType::BtoB:
        // [targetId(4bytes), senderID(4bytes), message...]
        {
            sysbit_t target { IntegerFromBytes<sysbit_t>(message.data().get()) };
            sysbit_t sender { IntegerFromBytes<sysbit_t>(message.data().get()+4) };

            if (!this->boards.contains(target) || !this->boards.contains(sender))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::BtoA:
        // [senderId(4byte), message...]
        {
            if (!this->boards.contains(IntegerFromBytes<sysbit_t>(message.data().get())))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::VtoA:
        // [targetId(4bytes), message...]
        {
            if (IntegerFromBytes<sysbit_t>(message.data().get()) != this->settings.id)
                return System::ErrorCode::Bad;
        }
        break;

        default:
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);
    return System::ErrorCode::Ok;
}

Error Assembly::SendMessage(Message message) noexcept
{
    // message.type() must be AtoA, AtoB, AtoV
    // data must be
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [targetId(4byte), message...]
    //      or
    //      [senderId(4bytes), message...]

    bool check { VM::GetVM().GetSettings().strictMessages };

    switch (message.type())
    {
        case MessageType::AtoA:
        {
            if (check && IntegerFromBytes<sysbit_t>(message.data().get()+4) != this->settings.id)
                return System::ErrorCode::Bad;

            VM::GetVM().ReceiveMessage(message);
        } break;
        
        case MessageType::AtoB:
        {
            sysbit_t id { IntegerFromBytes<sysbit_t>(message.data().get()) };
            if (check && !this->boards.contains(id))
                return System::ErrorCode::Bad;

            this->boards.at(id).ReceiveMessage(message);
        } break;

        case MessageType::AtoV:
        {
            if (check && IntegerFromBytes<sysbit_t>(message.data().get()) != this->settings.id)
                return System::ErrorCode::Bad;

            VM::GetVM().ReceiveMessage(message);
        } break;
        
        default:
            return System::ErrorCode::Bad;
    }


    return System::ErrorCode::Ok;
}
