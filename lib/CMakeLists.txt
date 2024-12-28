add_library(libs INTERFACE)

# Add subdirectories here
add_subdirectory(CLIParser)
# end_sub

# Link subdirectories here
target_link_libraries(libs
    INTERFACE
        CLIParser
)
# end_link
