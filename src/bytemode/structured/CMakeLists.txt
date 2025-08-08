add_library(structured
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

target_link_libraries(structured
    PRIVATE
        libs
        core
)
