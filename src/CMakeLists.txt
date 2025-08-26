add_library(src INTERFACE)

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
target_link_libraries(src
    INTERFACE
        core
        extensions
)
# end_link
