#pragma once

#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "bytemode/nativecalls.hpp"
#include "bytemode/structured/assembly.hpp"
#include "message.hpp"
#include "system.hpp"

using AssemblyCollection = std::unordered_map<std::string, Assembly>;
// We won't need to deallocate so no smart pointer
using AssemblyIDCollection = std::unordered_map<sysbit_t, Assembly*>;

class VM : IMessageObject
{
    public:
        struct VMSettings
        {
            bool strictMessages;
            bool unsafe;
#ifndef NDEBUG
            bool step;
#endif
            bool communication;
        };

        VM(VM const&) = delete;
        VM(VM const&&) = delete;
        VM& operator=(VM const&) = delete;
        VM& operator=(VM const&&) = delete;

        static VM_INLINE VM& GetVM() noexcept
        {
            static VM singletonVM { };
            return singletonVM; 
        }

        VM_INLINE const AssemblyCollection& Assemblies() const noexcept 
        { return this->assemblies; }

        Error DispatchMessages() noexcept override;
        Error ReceiveMessage(const Message message) noexcept override;
        Error SendMessage(const Message message) noexcept override;

//        const Assembly& GetAssembly(const std::string& name) const;
//        const Assembly& GetAssembly(const std::string&& name) const;

        VM_INLINE const Assembly& GetAssembly(sysbit_t id) const
        {
            if (!this->asmIds.contains(id))
                CRASH(System::ErrorCode::InvalidSpecifier, "Assembly with given id'", std::to_string(id), "' couldn't be found.");
            return *(this->asmIds.at(id));
        }

        Error AddAssembly(Assembly::AssemblySettings&& settings) noexcept;
        Error RemoveAssembly(sysbit_t id) noexcept;

        VM_INLINE const VMSettings& GetSettings() const noexcept
        { return this->settings; }

        VM_INLINE VMContext& GetContext() { return context; }

        Error Setup(VMSettings settings) noexcept;

        Error Run() noexcept;

    private:
        AssemblyCollection assemblies;
        AssemblyIDCollection asmIds;
        VMSettings settings;
        VMContext context;

        VM() { }

        VM_INLINE sysbit_t GenerateNewAssemblyID()
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
};
