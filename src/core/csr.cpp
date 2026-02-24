#include "CSRConfig.hpp"
#include "csr.hpp"

#include "bytemode/flat/flatvm.hpp"
#include <execinfo.h>

#ifndef TOOLCHAIN_MODE
#include <csignal>
#include <exception>
#include <sched.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "extensions/stringextensions.hpp"

#include "CLIParser.hpp"
#include "fastcout.hpp"
#include "system.hpp"

constexpr std::string SigToStr(int sign)
{
    switch (sign)
    {
        case SIGABRT: return "SIGABRT";
        case SIGSEGV: return "SIGSEGV";
        default: return "Unknown";
    }
}

void sigHandler(int signum)
{
    // TODO: Make cross platform
    pid_t pid { fork() };

    if (pid == 0)
    {
        LOGE(System::LogLevel::High, "Unexpected signal (", SigToStr(signum), "), aborting the program. Trace:");

        constexpr int traceMax { 1024 };
        void* array[traceMax];
        int size { backtrace(array, traceMax) };
        backtrace_symbols_fd(array, size, STDERR_FILENO);

        if (FastCout::Flush() != System::ErrorCode::Ok)
            std::cerr << "[ERROR] Couldn't flush the stoud, some messages may be missing.";

        _exit(0);
    }
    else
        waitpid(pid, nullptr, 0);

    _exit(1);
}

int csrmain(int argc, char** args)
{
    FastCout::Init();

    void* dummy[1];
    backtrace(dummy, 1);
    std::signal(SIGABRT, sigHandler);
    std::signal(SIGSEGV, sigHandler);

    System::ErrorCode errc { System::ErrorCode::Ok };

    try
    {
        CLIParser::Flags flags { SetUpCLI(args, argc) };

        if (flags.GetFlag<CLIParser::FlagType::Bool>("help"))
            PrintHelp(flags);
        else if (flags.GetFlag<CLIParser::FlagType::Bool>("version"))
            PrintHeader();
        else
        {
            std::filesystem::path exec { flags.GetFlag<CLIParser::FlagType::String>("exe") };

            if (exec.empty())
                CRASH(System::ErrorCode::NoSourceFile, "CSR must have at least one file to execute.");

            System::Result<FlatVM> vmRes { FlatVM::New(FlatVM::VMSettings {
                .unsafe = flags.GetFlag<CLIParser::FlagType::Bool>("unsafe"),
                .path = exec,
#ifdef ENABLE_JIT
                .jit = flags.GetFlag<CLIParser::FlagType::Bool>("jit"),
#endif
            })};

            if (System::NoneCheck(vmRes))
                CRASH(System::GetErr(vmRes), "Failed to initialize the VM.");

            FlatVM vm { System::GetValue(std::move(vmRes)) };

            errc = vm.Run();
        }
    }
    catch (const CSRException& exc)
    {
        [[unlikely]]
        std::cerr << exc.Stringify();
        return static_cast<int>(exc.GetCode());
    }
    catch (const std::exception& exc)
    {
        [[unlikely]]
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " 
                  << exc.what() 
                  << '\n';

        return 1;
    }

    [[likely]]
    if (errc != System::ErrorCode::Ok)
        std::cout << 
            "\nExited With " << static_cast<int>(errc) << " (" <<
            System::ErrorCodeString(errc) << ")" << std::endl;

    return static_cast<int>(errc);
}

void PrintHeader() noexcept
{
    std::cout << "\nCommon Script Runtime (CSR)"
              << "\n\tDescription: " << CSR_DESCRIPTION
              << "\n\tVersion: " << CSR_VERSION
              << "\n\tBuild Type: " << 
#ifndef NDEBUG
              "Debug"
#else
              "Release"
#endif
              << "\n\tBuild Details: "
#ifdef BUILD_STRUCTURED
              << "Structured VM --"
#endif
#ifdef BUILD_FLAT
              <<  "Flat VM --"
#endif
              << "\n\tEnable JIT: "
#ifdef ENABLE_JIT
              << "Available"
#else
              << "Unavailable"
#endif
              << '\n';
}

void PrintHelp(const CLIParser::Flags& flags) noexcept
{
    PrintHeader();
    std::cout << flags.GetHelpText() << '\n';
}

CLIParser::Flags SetUpCLI(char** args, int argc)
{
    using namespace CLIParser;

    Parser parser {args, argc, "--", "-"};

    parser.AddFlag<FlagType::Bool>("help", "Print this help text.");
    parser.AddFlag<FlagType::Bool>("version", "Print version.");
#ifdef ENABLE_JIT
    parser.Separator();
    parser.AddFlag<FlagType::Bool>("jit", "Mark this execution as JIT target.");
#endif
#if defined(BUILD_STRUCTURED)
    parser.Separator();
#endif
#if defined(BUILD_FLAT) && (defined(BUILD_STRUCTURED))
    parser.AddFlag<CLIParser::FlagType::Bool>("flat", "Run a flat VM without the whole VM structure.", false);
#endif
#if defined(BUILD_STRUCTURED)
    parser.AddFlag<FlagType::Bool>("no-new", "Do not create a new instance of CSR, use an already running one.", false);
    parser.AddFlag<FlagType::Bool>("no-strict-messages", "Don't strictly verify messages in each checkpoint when dispatching.", true);
    parser.AddFlag<FlagType::Bool>("messaging", "Enable communication within VM.", false);
#endif
    parser.Separator();
#if defined(BUILD_STRUCTURED)
    parser.AddFlag<FlagType::StringList>("exe", "Executable files to execute.");
#else
    parser.AddFlag<FlagType::String>("exe", "Executable file to execute.");
#endif
    parser.Separator();
    parser.AddFlag<FlagType::Bool>("unsafe", "Load extender dll of each executable.", false);
#ifndef NDEBUG
    parser.Separator();
    parser.AddFlag<FlagType::Bool>("step", "Run the VM once every input.");

    parser.BindFlag("s", "step");
#endif

    parser.BindFlag("h", "help");
    parser.BindFlag("v", "version");

    parser.BindFlag("e", "exe");
    parser.BindFlag("u", "unsafe");

    try
    {
        return parser.Parse();
    }
    catch (const std::exception& e)
    {
        throw CSR_ERR(System::ErrorCode::CLIParseError, Extensions::String::Concat({"An exception occured while parsing the CLI.\n\tProvided Information: ", e.what()}));
    }

    __builtin_unreachable();
}
#else
#include "csr.hpp"

//
// API Funcs
//
CSRInfo CSRGetBuildInfo()
{
    return (CSRInfo){
        CSR_VERSION,
        CSR_DESCRIPTION,
        CSR_VERSION_MAJOR,
        CSR_VERSION_MINOR,
        CSR_VERSION_PATCH,
        0
#ifdef ENABLE_JIT
        | CSR_BuildFlags_EnableJIT
#endif
#ifdef BUILD_FLAT
        | CSR_BuildFlags_BuildFlat
#endif
#ifdef BUILD_STRUCTURED
        | CSR_BuildFlags_BuildStructured
#endif
    };
}

static CSRSettingFlags flags {  };
void CSRSettings(CSRSettingFlags settings) { flags = settings;
}

CSRSettingFlags CSR::Settings() { return flags; }

//
// VMContext
//
CSRVMContext CreateVMContext(int unsafe, int jit, const Str executable)
{
    FlatVM* vm { new FlatVM{{
        static_cast<bool>(unsafe),
#ifndef NDEBUG
        false,
#endif
        executable,
        static_cast<bool>(jit)
    }}};
    return (CSRVMContext){ vm };
}

CSRErrorCode RunVM(CSRVMContext context)
{
    FlatVM* vm { static_cast<FlatVM*>(context.ptr) };
    return CSRErrorCode(vm->Run());
}

void DeleteVMContext(CSRVMContext context)
{
    FlatVM* vm { static_cast<FlatVM*>(context.ptr) };
    delete vm;
}
#endif
