#include <cstdio>
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
// VM Implementation
//
const Assembly& VM::GetAssembly(const std::string& name) const
{
    if (!this->assemblies.contains(name))
        CRASH(System::ErrorCode::InvalidSpecifier, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(const std::string&& name) const
{
    if (!this->assemblies.contains(name))
        CRASH(System::ErrorCode::InvalidSpecifier, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(sysbit_t id) const
{
    if (!this->asmIds.contains(id))
        CRASH(System::ErrorCode::InvalidSpecifier, "Assembly with given id'", std::to_string(id), "' couldn't be found.");
    return *(this->asmIds.at(id));
}

sysbit_t VM::GenerateNewAssemblyID() const
{
    sysbit_t id { static_cast<sysbit_t>(this->assemblies.size()) };
    while (this->asmIds.contains(id))
        id++;
    return id;
}

Error VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if (this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    if (this->assemblies.size() >= std::numeric_limits<sysbit_t>::max())
        return System::ErrorCode::Bad;

    settings.id = this->GenerateNewAssemblyID();
    this->assemblies.emplace(settings.name, rval(settings));
    this->asmIds.emplace(settings.id, &this->assemblies.at(settings.name));

    // Create an async system for loading assemblies
    System::ErrorCode code { this->asmIds.at(settings.id)->Load() };

    if (code == System::ErrorCode::Ok)
        return code;

    this->RemoveAssembly(settings.id);
    return code;
}

Error VM::RemoveAssembly(sysbit_t id) noexcept
{
    if (!this->asmIds.contains(id))  
        return System::ErrorCode::InvalidSpecifier;

    this->assemblies.erase(this->asmIds.at(id)->Settings().name);
    this->asmIds.erase(id);

    return System::ErrorCode::Ok;
}

Error VM::Run(VMSettings&& settings)
{
    this->settings = settings;
    System::ErrorCode code = System::ErrorCode::Ok;

    while (!this->assemblies.empty())
    {
        // Dispatch Messages
        code = this->DispatchMessages();

//        if (code != System::ErrorCode::Ok)
//            LOGE(
//                System::LogLevel::Medium, 
//                "Error while dispatching messages. Error code: ", 
//                System::ErrorCodeString(code) 
//            );

        // Run the assemblies
        for (auto& [name, assembly] : this->assemblies)
        {
            try_catch(
                code = assembly.Run();
                
//                if (code != System::ErrorCode::Ok)
//                    LOGE(
//                        System::LogLevel::Low,
//                        "Error while running assembly ", assembly.Stringify(),
//                        " Error code: ", System::ErrorCodeString(code) 
//                    );,
                ,

                LOGE(
                    System::LogLevel::Low, 
                    "Error while running assembly ", 
                    assembly.Stringify()
                );,

                LOGE(
                    System::LogLevel::Medium, 
                    "Fatal unexpected error while running",
                    assembly.Stringify()
                );
            )
        }

        if (this->settings.step)
        {
            int c = std::getchar();
            if (c == 'r')
                this->settings.step = false;
        }
    }

    return code;
}

//
// IMessageObject Implementation
//
Error VM::DispatchMessages() noexcept
{
    System::ErrorCode code { System::ErrorCode::Ok };

    while (!this->messagePool.empty())
    {
        const Message& message { this->messagePool.front() };

        if (message.type() == MessageType::AtoV)
        {
            if (message.data()[4] == 0)
                code = this->RemoveAssembly(IntegerFromBytes<sysbit_t>(message.data().get()));
        }
        else
        {
            LOGE(
                System::LogLevel::Low, 
                "Unhandled message, type: ",
                MessageTypeString(message.type())
            );
            code = System::ErrorCode::MessageDispatchError;
        } 

        if (code != System::ErrorCode::Ok)
            LOGE(
                System::LogLevel::Medium,
                "Message dispatch exited with code ", System::ErrorCodeString(code),
                ". Message type: ", MessageTypeString(message.type()) 
            );
    
        this->messagePool.pop();
    }
    
    return code;
}

Error VM::ReceiveMessage(Message message) noexcept
{
    if (!this->settings.strictMessages)
    {
        this->messagePool.push(message);
        return System::ErrorCode::Ok;
    }

    // message.type() must either be AtoA or AtoV
    if (message.type() != MessageType::AtoA && message.type() != MessageType::AtoV)
        return System::ErrorCode::Bad;

    // data must be either
    //      [targetId(4bytes), senderID(4bytes), message...]
    //      or
    //      [senderId(4bytes), message...]
    // check the first 4bytes to verify that sender/target exists.
    if (!this->asmIds.contains(IntegerFromBytes<sysbit_t>(message.data().get())))
        return System::ErrorCode::Bad;

    if (message.type() == MessageType::AtoA)
    {
        // additionally check the second 4bytes to verify that sender exists.
        if (!this->asmIds.contains(IntegerFromBytes<sysbit_t>(message.data().get()+4)))
            return System::ErrorCode::Bad;
    }

    this->messagePool.push(message);

    return System::ErrorCode::Ok;
}

Error VM::SendMessage(Message message) noexcept
{
    if (!this->settings.strictMessages)
    {
        sysbit_t id { IntegerFromBytes<sysbit_t>(message.data().get()) };
        this->asmIds.at(id)->ReceiveMessage(message);
        return System::ErrorCode::Ok;
    }

    // message.type() must be VtoA
    if (message.type() != MessageType::VtoA)
        return System::ErrorCode::Bad;

    // data must be [targetId(4bytes), message...]
    // check the first 4bytes to verify that target exists
    sysbit_t id { IntegerFromBytes<sysbit_t>(message.data().get()) };
    if (!this->asmIds.contains(id))
        return System::ErrorCode::Bad;

    this->asmIds.at(id)->ReceiveMessage(message);

    return System::ErrorCode::Ok;
}
