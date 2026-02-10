add_library(_csr_src INTERFACE)
add_library(csr::src ALIAS _csr_src)
set(modules "csr::core;csr::extensions")

# Add subdirectories here
add_subdirectory(core)
add_subdirectory(extensions)

if (BYTEMODE)
    add_subdirectory(bytemode)
    list(APPEND modules "csr::bytemode")
else ()
    message(FATAL_ERROR
        "CSR only has bytemode for now. Sorry.")
endif (BYTEMODE)
# end_sub


# Link subdirectories here
if(TOOLCHAIN_MODE)
    foreach(mod ${modules})
        target_sources(_csr_src INTERFACE $<TARGET_OBJECTS:${mod}>)
    endforeach()
endif(TOOLCHAIN_MODE)

target_link_libraries(_csr_src
    INTERFACE
        ${modules}
)
# end_link
