#pragma once

#include "CSRConfig.hpp"

#ifndef TOOLCHAIN_MODE
#include "CLIParser.hpp"
#include <iostream>
int csrmain(int argc, char** args);

void PrintHeader() noexcept;
void PrintHelp(const CLIParser::Flags& flags) noexcept;

CLIParser::Flags SetUpCLI(char** args, int argc);
#else
//
// TODO: C Bindings so you can use them from other languages. Mmmmmmh, C ABI.
//
#endif
