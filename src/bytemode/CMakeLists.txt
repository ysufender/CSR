add_library(bytemode
    STATIC
        process.cpp
        assembly.cpp        
        board.cpp
        cpu.cpp
        ram.cpp
)

target_link_libraries(bytemode
    PRIVATE
        libs
)
