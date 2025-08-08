add_library(flat
    STATIC
        flatvm.cpp
        flatram.cpp
)

target_link_libraries(flat
    PRIVATE
        libs
        core
)
