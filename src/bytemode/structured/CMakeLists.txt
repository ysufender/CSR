add_library(_csr_structured
    STATIC
        process.cpp
        assembly.cpp        
        board.cpp
        cpu.cpp
        ram.cpp
        rom.cpp
        instructions.cpp
        vm.cpp
)
add_library(csr::bytemode::structured ALIAS _csr_structured)

target_link_libraries(_csr_structured
    PRIVATE
        csr::libs
        csr::core
)
