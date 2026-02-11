#pragma once

#include <filesystem>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <variant>

#include "extensions/syntaxextensions.hpp"
#include "extensions/stringextensions.hpp"

#ifndef NDEBUG
    #define LOGD(...) System::LogInternal(Extensions::String::Concat({"[DEBUG] ", __VA_ARGS__}), __FILE__, __LINE__)
#else
    #define LOGD(...)
#endif

#define LOG(...) System::LogInternal(Extensions::String::Concat({__VA_ARGS__}), __FILE__, __LINE__)
#define LOGW(...) System::LogWarning(Extensions::String::Concat({__VA_ARGS__}), __FILE__, __LINE__)
#define LOGE(level, ...) System::LogError(Extensions::String::Concat({__VA_ARGS__}), level, __FILE__, __LINE__, System::ErrorCode::Bad)
#define CRASH(code, ...) System::LogError(Extensions::String::Concat({__VA_ARGS__}), System::LogLevel::High, __FILE__, __LINE__, code)

#define CSR_ERR(code, message) CSRException { message, __FILE__, __LINE__, code };

struct System
{
#define ERER(E) \
    E(Bad) \
    E(UnhandledException) \
    E(ROMAccessError) \
    E(RAMAccessError) \
    E(SourceFileNotFound) \
    E(UnsupportedFileType) \
    E(HeapOverflow) \
    E(StackOverflow) \
    E(NoSourceFile) \
    E(InvalidSpecifier) \
    E(FileIOError) \
    E(MessageSendError) \
    E(IndexOutOfBounds) \
    E(InvalidInstruction) \
    E(MessageReceiveError) \
    E(MessageDispatchError) \
    E(MemoryOverflow) \
    E(NotImplemented) \
    E(FragmentedHeap) \
    E(StackUnderflow) \
    E(DuplicateSysBind) \
    E(InvalidKey) \
    E(DLLoadError) \
    E(DLSymbolError) \
    E(VMError) \
    E(DLInitError) \
    E(DoubleFree) \
    E(Shutdown) \
    E(ProcessInterrupt) \
    E(IOError) \
    E(NativeCallError) \
    E(JITError) \
    E(CLIParseError) \
    E(DivideByZero) \
    E(SysCallError)
MAKE_ENUM(ErrorCode, Ok, 0, ERER, IN_CLASS)
#undef ERER

    template<typename T>
    using Result = std::variant<T, System::ErrorCode>;

    template<typename T>
    static constexpr bool Some(const Result<T>& res) noexcept { return std::holds_alternative<T>(res); }

    template<typename T>
    static constexpr T&& Get(Result<T>&& res) { return std::move(std::get<T>(res)); }

    template<typename T>
    static constexpr bool None(const Result<T>& res) noexcept { return std::holds_alternative<System::ErrorCode>(res); }

    template<typename T>
    static constexpr System::ErrorCode GetErr(Result<T>& res) { return std::get<System::ErrorCode>(res); }

    enum class LogLevel
    {
        Low,
        High
    };

    static void LogInternal(std::string_view message, std::string_view file, int line);
    static void LogWarning(std::string_view message, std::string_view file, int line);
    static void LogError(std::string_view message, LogLevel level, std::string_view file, int line, System::ErrorCode errCode);

    static Result<std::ifstream> OpenInFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);
    static Result<std::ofstream> OpenOutFile(const std::filesystem::path& path, const std::ios::openmode mode = std::ios::binary);

    private:
        System() = delete;
        System(System&) = delete;
        System(const System&) = delete;
        System(System&&) = delete;
        System(const System&&) = delete;
        ~System() = delete;

        System& operator=(System&) = delete;
        System& operator=(const System&) = delete;
        System& operator=(System&&) = delete;
        System& operator=(const System&&) = delete;

        void* operator new(size_t) = delete;
        void* operator new[](size_t) = delete;
        void operator delete(void*) = delete;
        void operator delete[](void*) = delete;
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
        
        const int GetLine() const noexcept { return _line; }
        System::ErrorCode GetCode() const noexcept { return _errCode; }
        const std::string& GetFile() const noexcept { return _file; }
        const std::string& GetMsg() const noexcept { return _message; }

        const std::string& Stringify() const noexcept { return _fullStr; }

        friend std::ostream& operator<<(std::ostream& out, const CSRException& exc)
        {
            out << exc.Stringify();
            return out;
        }
};
