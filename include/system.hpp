#pragma once

#include <filesystem>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "extensions/stringextensions.hpp"
#include "CLIParser.hpp"

#ifndef NDEBUG
    #define LOGD(...) System::LogInternal(Extensions::String::Concat({__VA_ARGS__}), __FILE__, __LINE__)
#else
    #define LOGD(...)
#endif

#define LOG(...) std::cout << Extensions::String::Concat({__VA_ARGS__}) << '\n'
#define LOGW(...) System::LogWarning(Extensions::String::Concat({__VA_ARGS__}), __FILE__, __LINE__)
#define LOGE(level, ...) System::LogError(Extensions::String::Concat({__VA_ARGS__}), level, __FILE__, __LINE__)

#define CSR_ERR(message) CSRException { message, __FILE__, __LINE__ };

struct System
{
    enum class LogLevel
    {
        Normal,
        Low,
        Medium,
        High
    };

    static void LogInternal(std::string_view message, std::string_view file, int line);
    static void LogWarning(std::string_view message, std::string_view file, int line);
    static void LogError(std::string_view message, LogLevel level, std::string_view file, int line);

    static void Setup(const CLIParser::Flags& flags, std::ostream& cout, std::ostream& cerr);
    
    static std::ifstream OpenInFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);
    static std::ofstream OpenOutFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);
};

class CSRException : std::runtime_error
{
    private:
        int _line;
        std::string _file;
        std::string _message;

        std::string _fullStr;

    public:
        CSRException(std::string message, std::string file, int line);
        
        int GetLine() const;
        const std::string& GetFile() const;
        const std::string& GetMsg() const;

        const std::string& Stringify() const;

        friend std::ostream& operator<<(std::ostream& out, const CSRException& exc);
};
