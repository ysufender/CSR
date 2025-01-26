#include <bitset>
#include <cassert>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <limits>
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
// IMessageObject Implementation
//
const System::ErrorCode Assembly::DispatchMessages() noexcept
{
    // TODO

    //LOGE(System::LogLevel::Medium, "Assembly::DispatchMessages has not been implemented yet");

    // Send Shutdown Signal to VM
    {
        char* data { new char[8] };
        char* id { BytesFromInteger<systembit_t, char>(this->settings.id) };
        const char* shutm { "Shut" };

        std::memcpy(data, id, sizeof(systembit_t));
        std::memcpy(data+4, shutm, 4);

        delete[] id;

        System::ErrorCode code { this->SendMessage({
            MessageType::AtoV,
            data,
        })};

        if (code != System::ErrorCode::Ok)
            LOGE(System::LogLevel::High, "Error, couldn't send shutdown signal to VM");
    }

    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };
        this->messagePool.pop();
    }
    return System::ErrorCode::Ok;
}

const System::ErrorCode Assembly::ReceiveMessage(Message message) noexcept
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
        return System::ErrorCode::Ok;

    switch (message.type())
    {
        case MessageType::BtoB:
        // [targetId(4bytes), senderID(4bytes), message...]
        {
            systembit_t target { IntegerFromBytes<systembit_t>(message.data()) };
            systembit_t sender { IntegerFromBytes<systembit_t>(message.data()+4) };

            if (!this->boards.contains(target) || !this->boards.contains(sender))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::BtoA:
        // [senderId(4byte), message...]
        {
            if (!this->boards.contains(IntegerFromBytes<systembit_t>(message.data())))
                return System::ErrorCode::Bad;
        }
        break;

        case MessageType::VtoA:
        // [targetId(4bytes), message...]
        {
            if (IntegerFromBytes<systembit_t>(message.data()) != this->settings.id)
                return System::ErrorCode::Bad;
        }
        break;

        default:
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);
    return System::ErrorCode::Ok;
}

const System::ErrorCode Assembly::SendMessage(Message message) noexcept
{
    // message.type() must be AtoA, AtoB, AtoV
    // data must be
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [targetId(4byte), message...]
    //      or
    //      [senderId(4bytes), message...]

    if (!VM::GetVM().GetSettings().strictMessages)
        return System::ErrorCode::Ok;

    switch (message.type())
    {
        case MessageType::AtoA:
        {
            if (IntegerFromBytes<systembit_t>(message.data()+4) != this->settings.id)
                return System::ErrorCode::Bad;

            VM::GetVM().ReceiveMessage(message);
        } break;
        
        case MessageType::AtoB:
        {
            systembit_t id { IntegerFromBytes<systembit_t>(message.data()) };
            if (!this->boards.contains(id))
                return System::ErrorCode::Bad;

            // TODO
            this->boards.at(id).ReceiveMessage(message);
        } break;

        case MessageType::AtoV:
        {
            if (*reinterpret_cast<systembit_t*>(message.data()) != this->settings.id)
                return System::ErrorCode::Bad;

            VM::GetVM().ReceiveMessage(message);
        } break;
        
        default:
            return System::ErrorCode::Bad;
    }


    return System::ErrorCode::Ok;
}


//
// Assembly Implementation
//
Assembly::Assembly(Assembly::AssemblySettings&& settings)
{
    this->settings = settings;
}

const System::ErrorCode Assembly::Load() noexcept
{
    if (!std::filesystem::exists(this->settings.path))
        return System::ErrorCode::Bad;

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
        return System::ErrorCode::Bad;
    }
    else 
    {
        LOGW(
            "The file (",
            this->settings.path.generic_string(),
            ") you provided has an unrecognized extension and can't be handled by the VM"
        );
        return System::ErrorCode::Bad;
    }

    std::ifstream bytecode { System::OpenInFile(this->settings.path) };
    bytecode.seekg(0, std::ios::end);
    IStreamPos(bytecode, length, {
        bytecode.close();
        return System::ErrorCode::Bad;
    });
    bytecode.seekg(0, std::ios::beg);

    char* data { new char[length] };
    bytecode.read(data, length);
    bytecode.close();

    this->rom.data = data;
    this->rom.size = static_cast<systembit_t>(length);

    // If the assembly is not ran, no need to initialize boards,
    // it might be a shared library.

    return System::ErrorCode::Ok;
}

const std::string& Assembly::Stringify() const noexcept
{
    static std::string stringified { };

    if (stringified.size() != 0)
        return stringified;

    const AssemblySettings& set { this->settings };
    std::stringstream ss;
    ss << '[' << set.name << ':' << set.id << ']'; 

    stringified = ss.str();
    return stringified;
}

systembit_t Assembly::GenerateNewBoardID() const
{
    systembit_t id { static_cast<systembit_t>(this->boards.size()) };
    while (this->boards.contains(id))
        id++;
    return id;
}

const System::ErrorCode Assembly::AddBoard() noexcept
{
    if (this->boards.size() >= std::numeric_limits<systembit_t>::max())
        return System::ErrorCode::Bad;
    
    systembit_t id { this->GenerateNewBoardID() };
    this->boards.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(*this, id)
    );

    return System::ErrorCode::Ok;
}

const System::ErrorCode Assembly::Run() noexcept
{
    // initialize the initial board.
    try_catch(
        if (this->boards.size() == 0)
        {
            LOGD("Initializing initial board...");
            this->AddBoard();
        },

        LOGE(System::LogLevel::Medium, this->Stringify(), " ROM access error while initializing Board.");
        return System::ErrorCode::Bad;,

        std::cerr << "Unexpected error in " << this->Stringify() << '\n';
        return System::ErrorCode::Bad;
    );

    System::ErrorCode code { this->DispatchMessages() };

    if (code == System::ErrorCode::Bad)
        return System::ErrorCode::Bad;

    for (auto& [id, board] : this->boards)
        break;

    return System::ErrorCode::Ok;
}

//
// ROM Implementation
//
char ROM::operator[](systembit_t index) const noexcept
{
    if (index >= size || index < 0)
        return 0;

    return data[index];
}

const char* ROM::operator&(systembit_t index) const noexcept
{
    if (index >= size || index < 0)
        LOGE(System::LogLevel::High, "Index '", std::to_string(index), "' of ROM is invalid.");

    return data+index;
}

const char* ROM::operator&() const noexcept
{
    return this->operator&(0); 
}

System::ErrorCode ROM::TryRead(systembit_t index, char& data, bool raise, std::function<void()> failAct) const
{
    System::ErrorCode isOk { !(index >= size || index < 0) ? System::ErrorCode::Ok : System::ErrorCode::Bad };

    if (isOk == System::ErrorCode::Ok)
        data = (*this)[index];
    if (!(isOk == System::ErrorCode::Ok) && failAct)
        failAct();
    if (!(isOk == System::ErrorCode::Ok) && raise)
        LOGE(System::LogLevel::High, "Cannot access index '", std::to_string(index), "' of ROM");
    return isOk;
}

ROM::~ROM()
{
    delete[] this->data;
}
