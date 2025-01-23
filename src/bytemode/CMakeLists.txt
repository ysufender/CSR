add_library(bytemode
    STATIC
        assembly.cpp        
        board.cpp
)

target_link_libraries(bytemode
    PRIVATE
        libs
)
