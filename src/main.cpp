#include "CSRConfig.hpp"

#ifndef TOOLCHAIN_MODE
#include "csr.hpp"

int main(int argc, char** args)
{
    return csrmain(argc, args);
}
#endif
