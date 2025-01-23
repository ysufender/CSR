#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "extensions/serialization.hpp"
#include "extensions/streamextensions.hpp"
#include "extensions/syntaxextensions.hpp"
#include "system.hpp"
#include "vm.hpp"

//
// Assembly Implementation
//
Assembly::Assembly(Assembly::AssemblySettings&& settings)
{
    this->settings = settings;
}

const System::ErrorCode Assembly::Load() noexcept
{
    if (!std::filesystem::exists(this->settings.path))
        return System::ErrorCode::Bad;

    std::string extension { this->settings.path.extension().string() };

    if (extension == ".jef")
        this->settings.type = AssemblyType::Executable;
    else if (extension == ".shd")
        this->settings.type = AssemblyType::Library;
    else if (extension == ".stc")
    {
        LOGW(
                "The file (", 
                this->settings.path, 
                ") you provided is a static library and can't be handled by the VM."
            );
        return System::ErrorCode::Bad;
    }
    else 
    {
        LOGW(
            "The file (",
            this->settings.path,
            ") you provided has an unrecognized extension and can't be handled by the VM"
        );
        return System::ErrorCode::Bad;
    }

    std::ifstream bytecode { System::OpenInFile(this->settings.path) };
    bytecode.seekg(0, std::ios::end);
    IStreamPos(bytecode, length, {
        bytecode.close();
        return System::ErrorCode::Bad;
    });
    bytecode.seekg(0, std::ios::beg);

    char* data { new char[length] };
    bytecode.read(data, length);
    bytecode.close();

    this->rom.data = data;
    this->rom.size = static_cast<systembit_t>(length);

    // If the assembly is not ran, no need to initialize boards,
    // it might be a shared library.

    return System::ErrorCode::Ok;
}

const Assembly::AssemblySettings& Assembly::Settings() const noexcept
{
    return this->settings;
}

const ROM& Assembly::Rom() const noexcept
{
    return this->rom;
}

const BoardCollection& Assembly::Boards() const noexcept
{
    return this->boards;
}

std::string Assembly::Stringify() const noexcept
{
    const AssemblySettings& set { this->settings };
    std::stringstream ss;
    ss << '[' << set.name << ':' << set.id << ']'; 
    return rval(ss.str());
}

const System::ErrorCode Assembly::Run()
{
    try_catch(
        if (this->boards.size() == 0)
            this->boards.emplace(0, *this),
        std::cerr << this->Stringify() << " ROM access error while initializing Board \n";
        return System::ErrorCode::Bad,

        std::cerr << this->Stringify() << '\n';
        return System::ErrorCode::Bad
    );

    return System::ErrorCode::Ok;
}

//
// ROM Implementation
//
char ROM::operator[](systembit_t index) const noexcept
{
    if (index >= size || index < 0)
        return 0;

    return data[index];
}

char* ROM::operator&(systembit_t index) const noexcept
{
    if (index >= size || index < 0)
        return nullptr;

    return data+index;
}

char* ROM::operator&() const noexcept
{
    return this->operator&(0); 
}

bool ROM::TryRead(systembit_t index, char& data, bool raise, std::function<void()> failAct) const
{
    bool isOk { !(index >= size || index < 0) };

    if (isOk)
        data = (*this)[index];
    if (!isOk && failAct)
        failAct();
    if (!isOk && raise)
        LOGE(System::LogLevel::High, "Cannot access index '", std::to_string(index), "' of ROM");
    return isOk;
}
