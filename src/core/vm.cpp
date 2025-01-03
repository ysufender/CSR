#include "bytemode/assembly.hpp"
#include "system.hpp"
#include "vm.hpp"

VM& VM::GetVM() noexcept
{
    static VM singletonVM { };
    return singletonVM; 
}

const AssemblyCollection& VM::Assemblies() const noexcept
{
    return this->assemblies;
}

const System::ErrorCode VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if(this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    this->assemblies[settings.name] = Assembly{settings};
    return System::ErrorCode::Ok;
}

const System::ErrorCode VM::Run(VMSettings&& settings)
{
    return System::ErrorCode::Ok;
}
