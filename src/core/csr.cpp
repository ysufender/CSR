#include "CSRConfig.hpp"

#ifndef TOOLCHAIN_MODE
#include "CLIParser.hpp"
#include "fastcout.hpp"
#include "system.hpp"
#include "csr.hpp"

#ifdef BUILD_FLAT
#include "bytemode/flat/flatvm.hpp"
#endif
#ifdef BUILD_STRUCTURED
#include "bytemode/structured/vm.hpp"
static_assert(false, "Structured VM is incomplete and not compatible with the current version of JASM. Use FlatVM instead.");
#endif

int csrmain(int argc, char** args)
{
    FastCout::Init();

    System::ErrorCode errc { System::ErrorCode::Ok };

    try
    {
        CLIParser::Flags flags { SetUpCLI(args, argc) };

        if (flags.GetFlag<CLIParser::FlagType::Bool>("help"))
            PrintHelp(flags);
        else if (flags.GetFlag<CLIParser::FlagType::Bool>("version"))
            PrintHeader();
#ifdef BUILD_FLAT
#if defined(BUILD_STRUCTURED)
        else if (flags.GetFlag<CLIParser::FlagType::Bool>("flat"))
#else
        else
#endif
         {
#ifdef BUILD_STRUCTURED
            std::vector<std::string> execs { flags.GetFlag<CLIParser::FlagType::StringList>("exe") };
            if (exec.size() > 1)
                LOGW("FlatVM requires a single executable. Only the first executable will be used.");
            std::filesystem::path exec { execs.at(0) };
#else
            std::filesystem::path exec { flags.GetFlag<CLIParser::FlagType::String>("exe") };
#endif

            FlatVM vm {FlatVM::VMSettings {
                .unsafe = flags.GetFlag<CLIParser::FlagType::Bool>("unsafe"),
                .path = exec,
#ifdef ENABLE_JIT
                .jit = flags.GetFlag<CLIParser::FlagType::Bool>("jit"),
#endif
            }};
            errc = vm.Run();
        }
#endif
#ifdef BUILD_STRUCTURED
        {
            if (flags.GetFlag<CLIParser::FlagType::Bool>("no-new"))
                LOGW("Single-process runtime is currently unavailable. A new instance will be created.");

            VM::GetVM().Setup(VM::VMSettings {
                .strictMessages = !flags.GetFlag<CLIParser::FlagType::Bool>("no-strict-messages"),
                .unsafe = flags.GetFlag<CLIParser::FlagType::Bool>("unsafe"),
#ifndef NDEBUG
                .step = flags.GetFlag<CLIParser::FlagType::Bool>("step"),
#endif
                .communication = flags.GetFlag<CLIParser::FlagType::Bool>("messaging")
            });

            std::vector<std::string> files { flags.GetFlag<CLIParser::FlagType::StringList>("exe") };

            if (files.size() == 0)
                CRASH(System::ErrorCode::NoSourceFile, "CSR must have at least one file to execute.");

            for (const std::filesystem::path& file : files)
            {
                LOGE(System::LogLevel::High, "Structured VM is not compatible with the current JASM. Use FlatVM by using -f flag");
                errc = VM::GetVM().AddAssembly({
#ifdef ENABLE_JIT
                    .jit = flags.GetFlag<CLIParser::FlagType::Bool>("jit"),
#endif
                    .name = file.filename().generic_string(),
                    .path = file,
                    /*type = will be set by the Assembly class*/
                });

                switch (errc) 
                {
                    case System::ErrorCode::Bad:
                        LOGE(System::LogLevel::Medium, "Can't register assembly '", file.filename().generic_string(), "', it already exists.");
                        break;
                    case System::ErrorCode::IndexOutOfBounds:
                        LOGE(System::LogLevel::Medium, "Can't register assembly '", file.filename().generic_string(), "', VM has reached the max number of assemblies.");
                        break;
                    case System::ErrorCode::SourceFileNotFound:
                        LOGE(System::LogLevel::Medium, "File at given path '", file.generic_string(), "' can't be found.");
                        break;
                    case System::ErrorCode::FileIOError:
                        LOGE(System::LogLevel::Medium, "Couldn't open assembly '", file.generic_string(), "'.");
                        break;
                    case System::ErrorCode::UnsupportedFileType:
                        LOGE(System::LogLevel::Medium, "Couldn't open assembly '", file.generic_string(), "'.");
                        break;
                    case System::ErrorCode::Ok:
                        break;
                    default:
                        LOGE(System::LogLevel::Medium, "Error, '", System::ErrorCodeString(errc), "'");
                }
            }

            if (VM::GetVM().Assemblies().size() > 0)
                errc = VM::GetVM().Run();
        }
#endif
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
#if defined(BUILD_STRUCTURED)
    parser.BindFlag("n", "no-new");
    parser.BindFlag("nsm", "no-strict-messages");
    parser.BindFlag("m", "messaging");
#endif
#if defined(BUILD_FLAT) && (defined(BUILD_STRUCTURED))
    parser.BindFlag("f", "flat");
#endif
    parser.BindFlag("e", "exe");
    parser.BindFlag("u", "unsafe");

    return parser.Parse();
}
#endif
