#include <exception>
#include <filesystem>
#include <iostream>
#include <ostream>

#include "CLIParser.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"
#include "csr.hpp"
#include "vm.hpp"

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
            if (flags.GetFlag<CLIParser::FlagType::Bool>("no-new"))
                LOGW("Single-process runtime is currently unavailable. A new instance will be created.");

            std::vector<std::string> files { flags.GetFlag<CLIParser::FlagType::StringList>("exe") };

            if (files.size() == 0)
                LOGE(System::LogLevel::High, "CSR must have at least one file to execute.");

            for (const std::filesystem::path& file : files)
            {
                System::ErrorCode err { VM::GetVM().AddAssembly({
                        // /*jit =*/ flags.GetFlag<CLIParser::FlagType::Bool>("jit"), 
                        /*jit =*/ false,
                        /*name =*/ file.filename().generic_string(),
                        /*path =*/ file,
                        /*type = will be set by the Assembly class*/
                        })};

                switch (err) 
                {
                    case System::ErrorCode::Bad:
                        LOGE(System::LogLevel::Medium, "Failed to register '", file.filename().generic_string(), "' into the VM.");
                    case System::ErrorCode::Ok:
                    default:
                        break;
                }
            }

            VM::GetVM().Run({
                .strictMessages = flags.GetFlag<CLIParser::FlagType::Bool>("no-strict-messages"),
            });
        } 
    }
    catch (const CSRException& exc)
    {
        std::cerr << exc.Stringify();
        return 1;
    }
    catch (const std::exception& exc)
    {
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " 
                  << exc.what() 
                  << '\n';

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

CLIParser::Flags SetUpCLI(char **args, int argc)
{
    using namespace CLIParser;

    Parser parser {args, argc, "--", "-"};

    parser.AddFlag<FlagType::Bool>("help", "Print this help text.");
    parser.AddFlag<FlagType::Bool>("version", "Print version.");
    parser.Separator();
    parser.AddFlag<FlagType::Bool>("jit", "Mark this execution as JIT target.");
    parser.AddFlag<FlagType::Bool>("no-new", "Do not create a new instance of CSR, use an already running one.");
    parser.AddFlag<FlagType::Bool>("no-strict-messages", "Strictly verifies messages in each checkpoint.", true);
    parser.Separator();
    parser.AddFlag<FlagType::StringList>("exe", "Executable files to execute.");

    parser.BindFlag("h", "help");
    parser.BindFlag("v", "version");
    parser.BindFlag("n", "no-new");
    parser.BindFlag("nsm", "no-strict-messages");
    parser.BindFlag("e", "exe");

    return parser.Parse();
}
