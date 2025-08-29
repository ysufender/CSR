add_library(_csr_core
    STATIC
        csr.cpp
        system.cpp
        platform.cpp
)
add_library(csr::core ALIAS _csr_core)

target_link_libraries(_csr_core
    PRIVATE
        csr::libs
        csr::bytemode
        csr::extensions
)
