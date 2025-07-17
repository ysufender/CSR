add_library(bytemode
    STATIC
        process.cpp
        assembly.cpp        
        board.cpp
        cpu.cpp
        ram.cpp
        rom.cpp
        instructions.cpp
        syscall.cpp
)

target_link_libraries(bytemode
    PRIVATE
        libs
        core
)
