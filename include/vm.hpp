#pragma once

#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
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
#ifndef NDEBUG
            bool step;
#endif
        };

        VM(VM const&) = delete;
        VM(VM const&&) = delete;
        VM& operator=(VM const&) = delete;
        VM& operator=(VM const&&) = delete;

        static inline VM& GetVM() noexcept
        {
            static VM singletonVM { };
            return singletonVM; 
        }

        inline const AssemblyCollection& Assemblies() const noexcept 
        { return this->assemblies; }

        Error DispatchMessages() noexcept override;
        Error ReceiveMessage(const Message message) noexcept override;
        Error SendMessage(const Message message) noexcept override;

        const Assembly& GetAssembly(const std::string& name) const;
        const Assembly& GetAssembly(const std::string&& name) const;
        const Assembly& GetAssembly(sysbit_t id) const;
        Error AddAssembly(Assembly::AssemblySettings&& settings) noexcept;
        Error RemoveAssembly(sysbit_t id) noexcept;

        inline const VMSettings& GetSettings() const noexcept
        { return this->settings; }

        Error Run(VMSettings&& settings);

    private:
        AssemblyCollection assemblies;
        AssemblyIDCollection asmIds;

        VMSettings settings;

        sysbit_t GenerateNewAssemblyID() const;

        VM() { }
};
