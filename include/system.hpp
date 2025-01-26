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
#define LOGE(level, ...) System::LogError(Extensions::String::Concat({__VA_ARGS__}), level, __FILE__, __LINE__, System::ErrorCode::Bad)
#define CRASH(code, ...) System::LogError(Extensions::String::Concat({__VA_ARGS__}), System::LogLevel::High, __FILE__, __LINE__, code)

#define CSR_ERR(message) CSRException { message, __FILE__, __LINE__ };

struct System
{
    enum class ErrorCode
    {
        Ok,
        Bad,
        ROMAccessError,
        RAMAccessError,
        SourceFileNotFound,
        UnsupportedFileType,
        HeapOverflow,
        StackOverflow,
        NoSourceFile,
        InvalidAssemblySpecifier,
        FileIOError,
        MessageSendError,
    };

    enum class LogLevel
    {
        Low,
        Medium,
        High
    };

    static void LogInternal(std::string_view message, std::string_view file, int line);
    static void LogWarning(std::string_view message, std::string_view file, int line);
    static void LogError(std::string_view message, LogLevel level, std::string_view file, int line, System::ErrorCode errCode);

    static std::ifstream OpenInFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);
    static std::ofstream OpenOutFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);
};

class CSRException : public std::runtime_error
{
    private:
        int _line;
        std::string _file;
        std::string _message;

        std::string _fullStr;

        System::ErrorCode _errCode;

    public:
        CSRException(std::string message, std::string file, int line, System::ErrorCode errCode);
        
        inline const int GetLine() const noexcept { return _line; }
        inline const System::ErrorCode GetCode() const noexcept { return _errCode; }
        inline const std::string& GetFile() const noexcept { return _file; }
        inline const std::string& GetMsg() const noexcept { return _message; }

        inline const std::string& Stringify() const noexcept { return _fullStr; }

        friend inline std::ostream& operator<<(std::ostream& out, const CSRException& exc)
        {
            out << exc.Stringify();
            return out;
        }
};
