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
        };

        VM(VM const&) = delete;
        VM(VM const&&) = delete;
        void operator=(VM const&) = delete;
        void operator=(VM const&&) = delete;

        static inline VM& GetVM() noexcept
        {
            static VM singletonVM { };
            return singletonVM; 
        }

        inline const AssemblyCollection& Assemblies() const noexcept 
        { return this->assemblies; }

        const System::ErrorCode DispatchMessages() noexcept override;
        const System::ErrorCode ReceiveMessage(const Message message) noexcept override;
        const System::ErrorCode SendMessage(const Message message) noexcept override;

        const Assembly& GetAssembly(const std::string& name) const;
        const Assembly& GetAssembly(const std::string&& name) const;
        const Assembly& GetAssembly(sysbit_t id) const;
        const System::ErrorCode AddAssembly(Assembly::AssemblySettings&& settings) noexcept;
        const System::ErrorCode RemoveAssembly(sysbit_t id) noexcept;

        inline const VMSettings& GetSettings() const noexcept
        { return this->settings; }

        const System::ErrorCode Run(VMSettings&& settings);

    private:
        AssemblyCollection assemblies;
        AssemblyIDCollection asmIds;

        VMSettings settings;

        sysbit_t GenerateNewAssemblyID() const;

        VM() { }
};
