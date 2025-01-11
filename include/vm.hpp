#pragma once

#include <unordered_map>
#include <string>

#include "bytemode/assembly.hpp"
#include "system.hpp"

using AssemblyCollection = std::unordered_map<std::string, Assembly>;

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
        const System::ErrorCode AddAssembly(Assembly::AssemblySettings&& settings) noexcept;
        const System::ErrorCode Run(VMSettings&& settings);

    private:
        AssemblyCollection assemblies;

        VM() { }
};
