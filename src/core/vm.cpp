#include "extensions/syntaxextensions.hpp"
#include "bytemode/assembly.hpp"
#include "system.hpp"
#include "vm.hpp"

const AssemblyCollection& VM::Assemblies() const noexcept
{
    return this->assemblies;
}

const System::ErrorCode VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if(this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    this->assemblies.emplace(settings.name, rval(settings));
    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::Run(VMSettings&& settings)
{
    System::ErrorCode code = System::ErrorCode::Ok;

    for (auto& [name, assembly] : this->assemblies)
    {
        System::ErrorCode assemblyCode { assembly.Run() };
        code = assemblyCode == System::ErrorCode::Bad ? assemblyCode : code;
    }

    return code;
}
