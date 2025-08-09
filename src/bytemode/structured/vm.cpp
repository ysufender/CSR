#include <cstdio>
#include <filesystem>
#include <limits>

#include "bytemode/syscall.hpp"
#include "extensions/syntaxextensions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/structured/assembly.hpp"
#include "CSRConfig.hpp"
#include "platform.hpp"
#include "bytemode/structured/message.hpp"
#include "system.hpp"
#include "bytemode/structured/vm.hpp"

Error InitStandardLibrary(SysCallHandler&);

//
// VM Implementation
//

Error VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if (this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    if (this->assemblies.size() >= std::numeric_limits<sysbit_t>::max())
        return System::ErrorCode::IndexOutOfBounds;

    settings.id = this->GenerateNewAssemblyID();
    this->assemblies.emplace(settings.name, rval(settings));
    this->asmIds.emplace(settings.id, &this->assemblies.at(settings.name));

    // TODO: create an async system for loading assemblies
    Assembly& assembly { *this->asmIds.at(settings.id) };
    System::ErrorCode code { assembly.Load() };

    if (code != System::ErrorCode::Ok)
    {
        this->assemblies.erase(settings.name);
        return code;
    }

    // Bind standard functions
    SysCallHandler& handler { assembly.SysCallHandler() };
    code = InitStandardLibrary(handler);
    if (code != System::ErrorCode::Ok)
    {
        LOGE(System::LogLevel::Medium, "Failed to load standard library for assembly ", settings.path.generic_string());
        this->assemblies.erase(settings.name);
        return code;
    }

    // Load shared library associated with the assembly
    if (!this->settings.unsafe)
        return code;

    std::filesystem::path dlPath { std::filesystem::absolute(settings.path.parent_path().append("lib"+settings.path.filename().string())) };
#ifdef CSR_WIN
    dlPath.replace_extension("dll");
#elif defined(CSR_UNIX)
    dlPath.replace_extension("so");
#elif defined(CSR_APPLE)
    dlPath.replace_extension("dylib");
#endif
    
    LOGD("Loading ",
        dlPath.string(),
        " for assembly ",
        settings.path.filename().string()
    );

    dlID_t extDl;
    try_catch(
        extDl = handler.LoadDl(dlPath.string());,
        code = exc.GetCode();,
        code = System::ErrorCode::UnhandledException; 
    )

    if (code != System::ErrorCode::Ok)
    {
        LOGE(System::LogLevel::Medium, "Failed to load extender DL for assembly ", settings.path.string());
        //this->RemoveAssembly(settings.id);
        this->assemblies.erase(settings.name);
        return code;
    }

    LOGD("Calling InitExtender for", dlPath.string());

    extenderInit_t extenderInit { DLSym<extenderInit_t>(extDl, "InitExtender") };
    if (!extenderInit)
    {
        LOGE(System::LogLevel::Medium, "Failed to initialize extender. No symbol InitExtender found.");
        return System::ErrorCode::DLInitError;
    }

    if (extenderInit(&context) != static_cast<char>(System::ErrorCode::Ok))
    {
        LOGE(System::LogLevel::Medium, "Failed to initialize extender for assembly ", settings.path.generic_string());
        //this->RemoveAssembly(settings.id);
        this->assemblies.erase(settings.name);
        this->assemblies.erase(settings.name);
        return System::ErrorCode::DLInitError;
    }

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

Error VM::Setup(VM::VMSettings settings) noexcept
{
    static bool set { false };

    if (set)
    {
        LOGE(
            System::LogLevel::Low,
            "VM can only be set once."
        );
        return System::ErrorCode::VMError;
    }

    set = true;
    this->settings = settings;
    return System::ErrorCode::Ok;
}

Error VM::Run() noexcept
{
    System::ErrorCode code = System::ErrorCode::Ok;

    while (!this->assemblies.empty())
    {
        // Dispatch Messages
        if (!this->messagePool.empty() && this->GetSettings().communication)
            code = this->DispatchMessages();

        // Run the assemblies
//        for (auto& [name, assembly] : this->assemblies)
        for (auto it = this->assemblies.begin(); it != this->assemblies.end();)
        {
            Assembly& assembly { it->second };

            try_catch(
                code = assembly.Run();
                
                if (code == System::ErrorCode::Shutdown)
                {
                    it = this->assemblies.erase(it);
                    continue;
                }
                if (code != System::ErrorCode::Ok)
                    LOGE(
                        System::LogLevel::Low,
                        "Error while running assembly ", assembly.Stringify(),
                        " Error code: ", System::ErrorCodeString(code) 
                    );

                it++;,

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

#ifndef NDEBUG
        if (this->settings.step)
        {
            int c = std::getchar();
            if (c == 'r')
                this->settings.step = false;
        }
#endif
    }

    return code == System::ErrorCode::Shutdown ? System::ErrorCode::Ok : code;
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
//            if (message.data()[4] == 0)
//                code = this->RemoveAssembly(IntegerFromBytes<sysbit_t>(message.data().get()));
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
