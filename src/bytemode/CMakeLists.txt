add_library(bytemode
    STATIC
        syscall.cpp
        stdlib.cpp
)

#Add subdirectories here
add_subdirectory(flat)
add_subdirectory(structured)
#end_addsubdir

target_link_libraries(bytemode
    PRIVATE
        libs
        core
        structured
        flat
)
