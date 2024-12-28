#include <exception>
#include <iostream>
#include <fstream>

#include "CLIParser.hpp"

#include "CSRConfig.hpp"
#include "system.hpp"
#include "csr.hpp"

int csrmain(int argc, char** args)
{
    try
    {
        CLIParser::Flags flags { SetUpCLI(args, argc) };

        if (flags.GetFlag<CLIParser::FlagType::Bool>("help"))
            PrintHelp(flags);
        else if (flags.GetFlag<CLIParser::FlagType::Bool>("version"))
            PrintHeader();
        else
        {
            if (flags.GetFlag<CLIParser::FlagType::Bool>("jit"))
               LOGW("JIT support is currently unavailable. The program will use the standart execution.");

            SetStdout(flags);

            // Call create context for each executable.
            // Flags such as 'jit' will be stored in the context, not the system. 
        }
    }
    catch (const std::exception& exc)
    {
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " 
                  << exc.what() << std::endl;
        return 1;
    }

    std::cout << std::endl;
    return 0; 
}

void PrintHeader() noexcept
{
    std::cout << "\nCommon Script Runtime (CSR)"
              << "\n\tDescription: " << CSR_DESCRIPTION
              << "\n\tVersion: " << CSR_VERSION
              << "\n\tEnable JIT: "
#ifdef ENABLE_JIT
              << "Available"
#else
              << "Unavailable"
#endif
              << '\n';
}

void PrintHelp(const CLIParser::Flags &flags) noexcept
{
    PrintHeader();
    std::cout << flags.GetHelpText() << '\n';
}

void SetStdout(const CLIParser::Flags& flags)
{
    std::vector<std::string> rdout { flags.GetFlag<CLIParser::FlagType::StringList>("redirect-stdout") };

    std::ofstream out;
    std::ofstream err;

    if (!rdout.empty())
    {
        if (rdout.size() == 0)
            LOGW("At least one path must be given after flag `--redirect-output (-r)`. Using default settings.");
        else if (rdout.size() == 1)
        {
            out = System::OpenOutFile(rdout[0]);
            err = System::OpenOutFile(rdout[0]);
        }
        else
        {
            out = System::OpenOutFile(rdout[0]);
            err = System::OpenOutFile(rdout[1]);
        }

        if (rdout.size() > 2)
            LOGE(System::LogLevel::Low, "--redirect-stdout cannot take more than 2 arguments.");
    }

    if (out.is_open() && err.is_open())
        System::Setup(flags, out, err);
    else
        System::Setup(flags, std::cout, std::cerr);
}

CLIParser::Flags SetUpCLI(char **args, int argc)
{
    using namespace CLIParser;

    Parser parser {args, argc, "--", "-"};

    parser.AddFlag<FlagType::Bool>("help", "Print this help text.");
    parser.AddFlag<FlagType::Bool>("version", "Print version.");
    parser.Separator();
    parser.AddFlag<FlagType::Bool>("jit", "Mark this execution as JIT target.");
    parser.AddFlag<FlagType::StringList>("redirect-stdout", "Redirect stdout and stderr to given files. If only one is provided, both get redirected to it.");
    parser.AddFlag<FlagType::Bool>("no-new", "Do not create a new instance of CSR, use an already running one.");
    parser.Separator();
    parser.AddFlag<FlagType::StringList>("exe", "Executable files to execute.");

    parser.BindFlag("h", "help");
    parser.BindFlag("v", "version");
    parser.BindFlag("n", "no-new");
    parser.BindFlag("e", "exe");
    parser.BindFlag("rdout", "redirect-stdout");

    return parser.Parse();
}
