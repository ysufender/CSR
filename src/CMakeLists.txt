add_library(_csr_src INTERFACE)
add_library(csr::src ALIAS _csr_src)

# Add subdirectories here
add_subdirectory(core)
add_subdirectory(extensions)

if (BYTEMODE)
    add_subdirectory(bytemode)
else ()
    message(FATAL_ERROR
        "CSR only has bytemode for now. Sorry.")
endif (BYTEMODE)
# end_sub


# Link subdirectories here
target_link_libraries(_csr_src
    INTERFACE
        csr::core
        csr::extensions
)

if (BYTEMODE)
    target_link_libraries(_csr_src
        INTERFACE
            csr::bytemode
    )
endif (BYTEMODE)
# end_link
