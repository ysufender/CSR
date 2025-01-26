#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <ostream>
#include <string>
#include <string_view>

#include "CLIParser.hpp"
#include "system.hpp"


// 
// System Implementation
//
void System::LogInternal(std::string_view message, std::string_view file, int line)
{
    size_t idx { file.find_first_of("CSR") };
    std::cout << "[CSR::Log](" << file.substr(idx, file.size() - idx) << ":" << line << ") >>> "  << message << '\n';
}

void System::LogWarning(std::string_view message, std::string_view file, int line)
{
    size_t idx { file.find_first_of("CSR") };
    std::cout << "[CSR::Warning](" << file.substr(idx, file.size() - idx) << ':' << line << ") >>> " << message << '\n';
}

void System::LogError(std::string_view message, LogLevel level, std::string_view file, int line, System::ErrorCode errCode)
{
    size_t idx { file.find_first_of("CSR") };

    switch (level)
    {
        case System::LogLevel::Low:
            break;
        case System::LogLevel::Medium:
            std::cerr << "IMPORTANT ERROR ";
            break;
        case System::LogLevel::High:
            std::cerr << "ALERT [CSR::Error](" << file.substr(idx, file.size() - idx) << ':' << line << ") >>> " << message << '\n';
            throw CSRException{message.data(), file.data(), line, errCode};
            break;
    }

    std::cerr << "[CSR::Error](" << file.substr(idx, file.size() - idx) << ':' << line << ") >>> " << message << '\n';
}

std::ifstream System::OpenInFile(const std::filesystem::path& path, const std::ios::openmode mode)
{
    if (!std::filesystem::exists(path))
        CRASH(System::ErrorCode::FileIOError, "The file at path '", path.generic_string(), "' does not exist.");

    std::ifstream file { path, mode };

    if (file.fail() || file.bad() || !file.is_open())
        CRASH(System::ErrorCode::FileIOError, "An error occured while opening the file '", path.generic_string(), "'.");

    return file;
}

std::ofstream System::OpenOutFile(const std::filesystem::path& path, const std::ios::openmode mode)
{
    if (!std::filesystem::exists(path))
        CRASH(System::ErrorCode::FileIOError, "The file at path '", path.generic_string(), "' does not exist.");

    std::ofstream file { path, mode };

    if (file.fail() || file.bad() || !file.is_open())
        CRASH(System::ErrorCode::FileIOError, "An error occured while opening the file '", path.generic_string(), "'.");

    return file;
}

//
// System::CSRException Implementation
//
CSRException::CSRException(std::string message, std::string file, int line, System::ErrorCode errCode)
    : std::runtime_error(message), _line(line), _message(message), _errCode(errCode)
{
    size_t idx { file.find_first_of("CSR") };
    _file = { file.substr(idx, file.size() - idx) };

    std::stringstream ss;

    ss << message << " [" << _file << ':' << _line << "]\n";
    _fullStr = ss.str();
}
