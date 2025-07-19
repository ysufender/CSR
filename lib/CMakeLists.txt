add_library(libs INTERFACE)

# Add subdirectories here
add_subdirectory(CLIParser)
add_subdirectory(libstdjasm)
# end_sub

# Add postbuild commands herer
set(LIBSTDJASM_OUTPUT_DIR ${LIBSTDJASM_OUTPUT_DIR} PARENT_SCOPE)
#end_command

# Link subdirectories here
target_link_libraries(libs
    INTERFACE
        CLIParser
)
# end_link
