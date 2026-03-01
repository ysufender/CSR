#include "CSRConfig.hpp"

#ifndef TOOLCHAIN_MODE
#include <csignal>

#include "platform.hpp"
// TODO: Stack trace on unhandled signals
#if defined(CSR_WIN)
#elif defined(CSR_UNIX)
#else
#error "Uhh, what the hell?"
#endif

#include "bytemode/flat/flatvm.hpp"
#include "extensions/stringextensions.hpp"
#include "CLIParser.hpp"
#include "fastcout.hpp"
#include "system.hpp"
#include "csr.hpp"

constexpr std::string SigToStr(int sign)
{
#define SIGNAL(name) case name: return #name
    switch (sign)
    {
        SIGNAL(SIGABRT);
        SIGNAL(SIGSEGV);
        default: return "UNKNOWN";
    }
#undef SIGNAL
}

extern "C" void UnhandledSignalHandler(int signum)
{
    LOGE(System::LogLevel::Medium, "Unexpected signal ", SigToStr(signum), ", aborting program.");

    if (FastCout::Flush() != System::ErrorCode::Ok) 
        std::cerr << "[ERROR] Couldn't flush the stdout, some messages may be missing.\n";

    exit(static_cast<int>(System::ErrorCode::ProcessInterrupt));
}

int csrmain(int argc, char** args)
{
    FastCout::Init();
    signal(SIGSEGV, UnhandledSignalHandler);
    signal(SIGABRT, UnhandledSignalHandler);

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

            FlatVM vm {FlatVM::VMSettings {
                .path = exec,
#ifdef ENABLE_JIT
                .jit = flags.GetFlag<CLIParser::FlagType::Bool>("jit"),
#endif
            }};

            errc = vm.Run();
        }
    }
    catch (const CSRException& exc)
    {
        std::cerr << exc.Stringify();
        return static_cast<int>(exc.GetCode());
    }
    catch (const std::exception& exc)
    {
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " 
                  << exc.what() 
                  << '\n';

        return 1;
    }

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
              << "\n\tBuild Details: Flat VM"
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
    parser.Separator();
    parser.AddFlag<FlagType::String>("exe", "Executable file to execute.");
    parser.Separator();
#ifndef NDEBUG
    parser.AddFlag<FlagType::Bool>("step", "Run the VM once every input.");

    parser.BindFlag("s", "step");
#endif

    parser.BindFlag("h", "help");
    parser.BindFlag("v", "version");
    parser.BindFlag("e", "exe");

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

namespace CSR
{
    static CSRSettings globalSettings { };

    void Setup(CSRSettings settings)
    {
        globalSettings = settings;
    }

    CSRSettings Settings()
    {
        return globalSettings;
    }
}
#endif
