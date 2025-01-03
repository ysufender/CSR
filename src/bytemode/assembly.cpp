#include <filesystem>
#include <fstream>
#include <string_view>

#include "bytemode/assembly.hpp"
#include "CSRConfig.hpp"
#include "extensions/streamextensions.hpp"
#include "system.hpp"

//
// Assembly Implementation
//
Assembly::Assembly(Assembly::AssemblySettings& settings)
{
    this->settings = settings;
}

const System::ErrorCode Assembly::Load() noexcept
{
    if (std::filesystem::exists(this->settings.path))
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

//
// ROM Implementation
//
ROM::ROMIndex ROM::operator[](systembit_t index) const
{
    if (index >= size || index < 0)
        return {false, 0};

    return {true, data[index]};
}
