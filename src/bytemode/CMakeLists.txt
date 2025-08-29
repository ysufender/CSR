add_library(_csr_bytemode
    STATIC
        syscall.cpp
        stdlib.cpp
        assemblyinfo.cpp
)
add_library(csr::bytemode ALIAS _csr_bytemode)

if (ENABLE_JIT)
    target_sources(_csr_bytemode PRIVATE jit.cpp)
endif (ENABLE_JIT)

#Add subdirectories here
if (BUILD_FLAT)
    message(STATUS "Building FlatVM...")
    add_subdirectory(flat)
    target_link_libraries(_csr_bytemode PRIVATE csr::bytemode::flat)
endif (BUILD_FLAT)

if (BUILD_STRUCTURED)
    message(STATUS "Building Structured VM...")
    add_subdirectory(structured)
    target_link_libraries(_csr_bytemode PRIVATE csr::bytemode::structured)
endif (BUILD_STRUCTURED)
#end_addsubdir

target_link_libraries(_csr_bytemode
    PRIVATE
        csr::libs
        csr::core
)
