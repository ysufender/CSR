add_library(src INTERFACE)

# Add subdirectories here
add_subdirectory(core)
add_subdirectory(bytemode)
add_subdirectory(extensions)
# end_sub


# Link subdirectories here
target_link_libraries(src
    INTERFACE
        core
        extensions
        bytemode
)
# end_link
