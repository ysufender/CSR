add_library(bytemode
    STATIC
        assembly.cpp        
)

target_link_libraries(bytemode
    PRIVATE
        libs
)
