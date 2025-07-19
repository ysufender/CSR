#include <cstdio>
#include <filesystem>
#include <string>
#include <limits>

#include "bytemode/syscall.hpp"
#include "extensions/syntaxextensions.hpp"
#include "extensions/converters.hpp"
#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "platform.hpp"
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
    sysbit_t id { 0 };
    for (; id <= std::numeric_limits<sysbit_t>::max(); id++)
        if (!this->asmIds.contains(id))
            break;

    // Since AddAssembly returns Error::Bad when asm count is max, no need to
    // error check here because it'll always find an id. Same with all GenerateNewXID
    // around the codebase.

    return id;
}

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
    System::ErrorCode code { this->asmIds.at(settings.id)->Load() };

    if (code != System::ErrorCode::Ok)
    {
        this->RemoveAssembly(settings.id);
        return code;
    }

    // Load and set up standard library dlls and functions
    std::filesystem::path stdlibPath { GetExePath().parent_path().append("libstdjasm.lib") };
#if defined(_WIN32) || defined(__CYGWIN__)
    stdlibPath.replace_extension("dll");
#elif defined(unix) || defined(__unix) || defined(__unix__)
    stdlibPath.replace_extension("so");
#elif defined(__APPLE__) || defined(__MACH__)
    stdlibPath.replace_extension("dylib");
#endif

    dlID_t stdlib;
    try_catch(
        stdlib =  this->asmIds.at(settings.id)->SysCallHandler().LoadDl(stdlibPath.generic_string());,
        code = exc.GetCode();,
        code = Error::UnhandledException; 
    )

    if (code != Error::Ok)
    {
        LOGE(System::LogLevel::Medium, "Failed to load standard library for assembly ", settings.path.generic_string());
        this->RemoveAssembly(settings.id);
        return code;
    }

    extInit_t stdlibInit { DLSym<extInit_t>(stdlib, "STDLibInit") };
    if (stdlibInit(&this->asmIds.at(settings.id)->SysCallHandler()) != static_cast<char>(Error::Ok))
    {
        LOGE(System::LogLevel::Medium, "Failed to load standard library for assembly ", settings.path.generic_string());
        this->RemoveAssembly(settings.id);
        return Error::DLInitError;
    }

    // Load shared library associated with the assembly
    if (!this->settings.unsafe)
        return code;

    std::filesystem::path dlPath { settings.path };
#if defined(_WIN32) || defined(__CYGWIN__)
    dlPath.replace_extension("dll");
#elif defined(unix) || defined(__unix) || defined(__unix__)
    dlPath.replace_extension("so");
#elif defined(__APPLE__) || defined(__MACH__)
    dlPath.replace_extension("dylib");
#endif
    
    LOG("Loading ",
        dlPath.filename().generic_string(),
        " for assembly ",
        settings.path.filename().generic_string()
    );

    dlID_t extDl;
    try_catch(
        extDl = this->asmIds.at(settings.id)->SysCallHandler().LoadDl(dlPath.generic_string());,
        code = exc.GetCode();,
        code = Error::UnhandledException; 
    )

    if (code != Error::Ok)
    {
        LOGE(System::LogLevel::Medium, "Failed to load extender DL for assembly ", settings.path.generic_string());
        this->RemoveAssembly(settings.id);
        return code;
    }

    extInit_t extenderInit { DLSym<extInit_t>(extDl, "InitExtender") };
    if (extenderInit(&this->asmIds.at(settings.id)->SysCallHandler()) != static_cast<char>(Error::Ok))
    {
        LOGE(System::LogLevel::Medium, "Failed to initialize extender for assembly ", settings.path.generic_string());
        this->RemoveAssembly(settings.id);
        return Error::DLInitError;
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
        return Error::VMError;
    }

    set = true;
    this->settings = settings;
    return Error::Ok;
}

Error VM::Run() noexcept
{
    System::ErrorCode code = System::ErrorCode::Ok;

    while (!this->assemblies.empty())
    {
        // Dispatch Messages
        code = this->DispatchMessages();

        // Run the assemblies
        for (auto& [name, assembly] : this->assemblies)
        {
            try_catch(
                code = assembly.Run();
                
                if (code != System::ErrorCode::Ok)
                    LOGE(
                        System::LogLevel::Low,
                        "Error while running assembly ", assembly.Stringify(),
                        " Error code: ", System::ErrorCodeString(code) 
                    );,

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
