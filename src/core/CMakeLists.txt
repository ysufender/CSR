add_library(core
    STATIC
        csr.cpp
        system.cpp
        platform.cpp
)

target_link_libraries(core
    PRIVATE
        libs
        bytemode
        extensions
)
