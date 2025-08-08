add_library(bytemode
    STATIC
        syscall.cpp
        stdlib.cpp
        assemblyinfo.cpp
)

#Add subdirectories here
if (BUILD_FLAT)
    message(STATUS "Building FlatVM...")
    add_subdirectory(flat)
    target_link_libraries(bytemode PRIVATE flat)
endif (BUILD_FLAT)

if (BUILD_STRUCTURED)
    message(STATUS "Building Structured VM...")
    add_subdirectory(structured)
    target_link_libraries(bytemode PRIVATE structured)
endif (BUILD_STRUCTURED)
#end_addsubdir

target_link_libraries(bytemode
    PRIVATE
        libs
        core
)
