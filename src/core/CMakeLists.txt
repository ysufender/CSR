add_library(core
    STATIC
        csr.cpp
        system.cpp
        vm.cpp
        message.cpp
        slice.cpp
)

target_link_libraries(core
    PRIVATE
        libs
        bytemode
        extensions
)
