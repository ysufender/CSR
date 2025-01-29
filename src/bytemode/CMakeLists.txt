add_library(bytemode
    STATIC
        process.cpp
        assembly.cpp        
        board.cpp
        cpu.cpp
        ram.cpp
        slice.cpp
        rom.cpp
)

target_link_libraries(bytemode
    PRIVATE
        libs
)
