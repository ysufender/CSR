add_library(libs INTERFACE)

# Add subdirectories here
add_subdirectory(CLIParser)
add_subdirectory(libstdjasm)
# end_sub

# Add postbuild commands herer
set(LIBSTDJASM_OUTPUT_DIR ${LIBSTDJASM_OUTPUT_DIR} PARENT_SCOPE)
#end_command

set_target_properties(stdjasm
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/${OUTPUT_PATH}/"
)

# Link subdirectories here
target_link_libraries(libs
    INTERFACE
        CLIParser
)
# end_link
