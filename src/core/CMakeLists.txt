add_library(core
    STATIC
        csr.cpp
        system.cpp
)

target_link_libraries(core
    PRIVATE
        libs
        extensions
)
