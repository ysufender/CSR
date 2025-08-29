add_library(_csr_extensions
    STATIC
        stringextensions.cpp
)
add_library(csr::extensions ALIAS _csr_extensions)
