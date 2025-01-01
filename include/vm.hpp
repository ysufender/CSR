#pragma once

#include <unordered_map>
#include <string>

#include "bytemode/assembly.hpp"
#include "system.hpp"

using AssemblyCollection = std::unordered_map<std::string, Assembly>;

class VM
{
    public:

        VM(VM const&) = delete;
        VM(VM const&&) = delete;
        void operator=(VM const&) = delete;
        void operator=(VM const&&) = delete;

        static VM& GetVM() noexcept;

        const AssemblyCollection& Assemblies() const noexcept;
        const System::ErrorCode AddAssembly(Assembly::AssemblySettings&& settings) noexcept;

    private:
        AssemblyCollection assemblies;

        VM() { }
};
