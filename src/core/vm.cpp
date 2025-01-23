#include <string>
#include <limits>

#include "CSRConfig.hpp"
#include "extensions/syntaxextensions.hpp"
#include "bytemode/assembly.hpp"
#include "system.hpp"
#include "vm.hpp"

const AssemblyCollection& VM::Assemblies() const noexcept
{
    return this->assemblies;
}

const Assembly& VM::GetAssembly(const std::string& name) const
{
    if (!this->assemblies.contains(name))
        LOGE(System::LogLevel::High, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(const std::string&& name) const
{
    if (!this->assemblies.contains(name))
        LOGE(System::LogLevel::High, "Assembly with given name '", name, "' couldn't be found.");
    return this->assemblies.at(name);
}

const Assembly& VM::GetAssembly(systembit_t id) const
{
    if (!this->asmIds.contains(id))
        LOGE(System::LogLevel::High, "Assembly with given id'", std::to_string(id), "' couldn't be found.");
    return *(this->asmIds.at(id));
}

systembit_t VM::GenerateNewAssemblyID() const
{
    systembit_t id { static_cast<systembit_t>(this->assemblies.size()) };

    while (this->asmIds.contains(id))
        id++;

    return id;
}

const System::ErrorCode VM::AddAssembly(Assembly::AssemblySettings&& settings) noexcept
{
    if(this->assemblies.contains(settings.name))
        return System::ErrorCode::Bad;

    if(this->assemblies.size() >= std::numeric_limits<systembit_t>::max())
        return System::ErrorCode::Bad;

    settings.id = this->GenerateNewAssemblyID();
    this->assemblies.emplace(settings.name, rval(settings));
    this->asmIds[settings.id] = &this->assemblies.at(settings.name);
    // Create an asynch system for loading assemblies
    return this->asmIds.at(settings.id)->Load();
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
