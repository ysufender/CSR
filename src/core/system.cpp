#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <ostream>
#include <string>
#include <string_view>

#include "system.hpp"

// 
// System Implementation
//
void System::LogInternal(std::string_view message, std::string_view file, int line)
{
#ifdef TOOLCHAIN_MODE
    if (CSR::Settings() & CSR_Settings_Silent)
        return;
#endif

    size_t idx { file.find_first_of("CSR") };
    std::cout << "[CSR::Log](" << file.substr(idx, file.size() - idx) << ":" << line << ") >>> "  << message << '\n';
}

void System::LogWarning(std::string_view message, std::string_view file, int line)
{
#ifdef TOOLCHAIN_MODE
    if (CSR::Settings() & CSR_Settings_Silent)
        return;
#endif

    size_t idx { file.find_first_of("CSR") };
    std::cout << "[CSR::Warning](" << file.substr(idx, file.size() - idx) << ':' << line << ") >>> " << message << '\n';
}

void System::LogError(std::string_view message, LogLevel level, std::string_view file, int line, System::ErrorCode errCode)
{
#ifdef TOOLCHAIN_MODE
    if (CSR::Settings() & CSR_Settings_Silent)
        return;
#endif

    size_t idx { file.find_first_of("CSR") };

    std::cerr
        << "[CSR::Error"
        << (level == System::LogLevel::Low ? "" : "::High")
        << "]("
        << file.substr(idx, file.size() - idx)
        << ':' << line << ") >>> " << message << '\n';
}

System::Result<std::ifstream> System::OpenInFile(const std::filesystem::path& path, const std::ios::openmode mode)
{
    if (!std::filesystem::exists(path))
    {
        LOGE(LogLevel::High, "The file at path '", path.generic_string(), "' does not exist.");
        return System::ErrorCode::FileIOError;
    }

    std::ifstream file { path, mode };

    if (file.fail() || file.bad() || !file.is_open())
    {
        LOGE(LogLevel::High, "An error occured while opening the file '", path.generic_string(), "'.");
        return System::ErrorCode::FileIOError;
    }

    return file;
}

System::Result<std::ofstream> System::OpenOutFile(const std::filesystem::path& path, const std::ios::openmode mode)
{
    if (!std::filesystem::exists(path))
    {
        LOGE(LogLevel::High, "The file at path '", path.generic_string(), "' does not exist.");
        return System::ErrorCode::FileIOError;
    }

    std::ofstream file { path, mode };

    if (file.fail() || file.bad() || !file.is_open())
    {
        LOGE(LogLevel::High, "An error occured while opening the file '", path.generic_string(), "'.");
        return System::ErrorCode::FileIOError;
    }

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

    ss << message << " [" << _file << ':' << _line << "|<" << System::ErrorCodeString(_errCode) << ">]\n";
    _fullStr = ss.str();
}
