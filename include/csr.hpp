#pragma once

#include <CLIParser.hpp>

int csrmain(int argc, char** args);

void PrintHeader() noexcept;
void PrintHelp(const CLIParser::Flags& flags) noexcept;
void SetStdout(const CLIParser::Flags& flags);

CLIParser::Flags SetUpCLI(char** args, int argc);
