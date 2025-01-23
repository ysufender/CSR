add_library(bytemode
    STATIC
        assembly.cpp        
        board.cpp
        cpu.cpp
)

target_link_libraries(bytemode
    PRIVATE
        libs
)
