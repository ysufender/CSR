#pragma once

#include <iostream>

#include "CLIParser.hpp"

int csrmain(int argc, char** args);

void PrintHeader() noexcept;
void PrintHelp(const CLIParser::Flags& flags) noexcept;

CLIParser::Flags SetUpCLI(char** args, int argc);
