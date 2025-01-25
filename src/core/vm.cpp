#include <string>
#include <limits>

#include "extensions/syntaxextensions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// IMessageObject Implementation
//
const System::ErrorCode VM::DispatchMessages() noexcept
{
    // TODO
    // 1- sender might request a redirect
    // 2- sender might request a target shutdown 
    // 2- sender might request a self shutdown 
    // TODO

    LOGE(System::LogLevel::Medium, "VM::DispatchMessages has not been implemented yet");
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };
        this->messagePool.pop();
    }
    
    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::ReceiveMessage(const Message message) noexcept
{
    // message.type must either be AtoA or AtoV
    if (message.type != MessageType::AtoA && message.type != MessageType::AtoV)
        return System::ErrorCode::Bad;

    // data must be either
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4bytes), message...]
    // check the first 4bytes to verify that sender/target exists.
    if (!this->asmIds.contains(IntegerFromBytes<systembit_t>(message.data)))
        return System::ErrorCode::Bad;

    if (message.type == MessageType::AtoA)
    {
        // additionally check the second 4bytes to verify that sender exists.
        if (!this->asmIds.contains(IntegerFromBytes<systembit_t>(message.data+4)))
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);

    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::SendMessage(const Message message) noexcept
{
    // message.type must be VtoA
    if (message.type != MessageType::VtoA)
        return System::ErrorCode::Bad;

    // data must be [targetId(4bytes), message...]
    // check the first 4bytes to verify that target exists
    systembit_t id { IntegerFromBytes<systembit_t>(message.data) };
    if (!this->asmIds.contains(id))
        return System::ErrorCode::Bad;

    this->asmIds.at(id)->ReceiveMessage(message);

    return System::ErrorCode::Ok;
}

//
// VM Implementation
//
const Assembly& VM::GetAssembly(const std::string& name) const
{
    if (!this->assemblies.contains(name))
        LOGE(System::LogLevel::High, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(const std::string&& name) const
{
    if (!this->assemblies.contains(name))
        LOGE(System::LogLevel::High, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(systembit_t id) const
{
    if (!this->asmIds.contains(id))
        LOGE(System::LogLevel::High, "Assembly with given id'", std::to_string(id), "' couldn't be found.");
    return *(this->asmIds.at(id));
}

systembit_t VM::GenerateNewAssemblyID() const
{
    systembit_t id { static_cast<systembit_t>(this->assemblies.size()) };
    while (this->asmIds.contains(id))
        id++;
    return id;
}

const System::ErrorCode VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if (this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    if (this->assemblies.size() >= std::numeric_limits<systembit_t>::max())
        return System::ErrorCode::Bad;

    settings.id = this->GenerateNewAssemblyID();
    this->assemblies.emplace(settings.name, rval(settings));
    this->asmIds[settings.id] = &this->assemblies.at(settings.name);
    // Create an async system for loading assemblies
    return this->asmIds.at(settings.id)->Load();
}

const System::ErrorCode VM::Run(VMSettings&& settings)
{
    System::ErrorCode code = System::ErrorCode::Ok;

    while (true)
    {
        // Dispatch Messages
        code = (code == System::ErrorCode::Bad ? code : this->DispatchMessages());

        // Run the assemblies
        for (auto& [name, assembly] : this->assemblies)
            code = (code == System::ErrorCode::Bad ? code : assembly.Run());
    }

    return code;
}
