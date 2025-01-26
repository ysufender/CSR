#include <string>
#include <limits>
#include <string_view>

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
    // 3- sender might request a self shutdown 
    // TODO

    //LOGE(System::LogLevel::Medium, "VM::DispatchMessages has not been implemented yet");
    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };

        if (message.type() == MessageType::AtoV && std::string_view{message.data()+4, message.data()+8} == "Shut")
        {
            LOGD("Received Shutdown signal from ", this->asmIds.at(*reinterpret_cast<systembit_t*>(message.data()))->Stringify());
            this->RemoveAssembly(*reinterpret_cast<systembit_t*>(message.data()));
        }
    
        this->messagePool.pop();
    }
    
    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::ReceiveMessage(Message message) noexcept
{
    if (!this->settings.strictMessages)
        return System::ErrorCode::Ok;

    // message.type() must either be AtoA or AtoV
    if (message.type() != MessageType::AtoA && message.type() != MessageType::AtoV)
        return System::ErrorCode::Bad;

    // data must be either
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4bytes), message...]
    // check the first 4bytes to verify that sender/target exists.
    if (!this->asmIds.contains(IntegerFromBytes<systembit_t>(message.data())))
        return System::ErrorCode::Bad;

    if (message.type() == MessageType::AtoA)
    {
        // additionally check the second 4bytes to verify that sender exists.
        if (!this->asmIds.contains(IntegerFromBytes<systembit_t>(message.data()+4)))
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);

    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::SendMessage(Message message) noexcept
{
    if (!this->settings.strictMessages)
        return System::ErrorCode::Ok;

    // message.type() must be VtoA
    if (message.type() != MessageType::VtoA)
        return System::ErrorCode::Bad;

    // data must be [targetId(4bytes), message...]
    // check the first 4bytes to verify that target exists
    systembit_t id { IntegerFromBytes<systembit_t>(message.data()) };
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
        CRASH(System::ErrorCode::InvalidAssemblySpecifier, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(const std::string&& name) const
{
    if (!this->assemblies.contains(name))
        CRASH(System::ErrorCode::InvalidAssemblySpecifier, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(systembit_t id) const
{
    if (!this->asmIds.contains(id))
        CRASH(System::ErrorCode::InvalidAssemblySpecifier, "Assembly with given id'", std::to_string(id), "' couldn't be found.");
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
    System::ErrorCode code { this->asmIds.at(settings.id)->Load() };

    if (code == System::ErrorCode::Ok)
        return code;

    this->RemoveAssembly(settings.id);
    return code;
}

const System::ErrorCode VM::RemoveAssembly(systembit_t id) noexcept
{
    if (!this->asmIds.contains(id))  
        return System::ErrorCode::Bad;

    this->assemblies.erase(this->asmIds.at(id)->Settings().name);
    Assembly* assembly { this->asmIds.at(id) };

    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::Run(VMSettings&& settings)
{
    try_catch(
        this->settings = settings;
        System::ErrorCode code = System::ErrorCode::Ok;

        while (!this->assemblies.empty())
        {
            // Dispatch Messages
            code = (code == System::ErrorCode::Bad ? code : this->DispatchMessages());

            // Run the assemblies
            for (auto& [name, assembly] : this->assemblies)
                code = (code == System::ErrorCode::Bad ? code : assembly.Run());
        }
        return code;,

        return System::ErrorCode::Bad;,

        return System::ErrorCode::Bad;
    )
}
