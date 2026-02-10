add_library(_csr_bytemode
    STATIC
        syscall.cpp
        stdlib.cpp
        assemblyinfo.cpp
)
add_library(csr::bytemode ALIAS _csr_bytemode)
set(modules "csr::libs;csr::core")

if (ENABLE_JIT)
    target_sources(_csr_bytemode PRIVATE jit.cpp)
endif (ENABLE_JIT)

#Add subdirectories here
message(STATUS "Building FlatVM...")
add_subdirectory(flat)
list(APPEND modules "csr::bytemode::flat")
#end_addsubdir

target_link_libraries(_csr_bytemode
    PRIVATE
        ${modules}
)
