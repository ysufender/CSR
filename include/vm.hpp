#pragma once

#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
#include "system.hpp"

using AssemblyCollection = std::unordered_map<std::string, Assembly>;
using AssemblyIDCollection = std::unordered_map<systembit_t, Assembly*>;

class VM
{
    public:
        struct VMSettings
        {
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

        const AssemblyCollection& Assemblies() const noexcept;
        const Assembly& GetAssembly(const std::string& name) const;
        const Assembly& GetAssembly(const std::string&& name) const;
        const Assembly& GetAssembly(systembit_t id) const;
        const System::ErrorCode AddAssembly(Assembly::AssemblySettings&& settings) noexcept;
        const System::ErrorCode Run(VMSettings&& settings);

    private:
        AssemblyCollection assemblies;
        AssemblyIDCollection asmIds;

        systembit_t GenerateNewAssemblyID() const;

        VM() { }
};
