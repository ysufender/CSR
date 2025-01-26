add_library(core
    STATIC
        csr.cpp
        system.cpp
        vm.cpp
        message.cpp
)

target_link_libraries(core
    PRIVATE
        libs
        extensions
)
