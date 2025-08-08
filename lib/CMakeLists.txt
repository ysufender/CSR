add_library(libs INTERFACE)

# Add prebuild commands here
set(ASMJIT_EMBED true)
# end_pre

# Add subdirectories here
add_subdirectory(CLIParser)
if (ENABLE_JIT)
    add_subdirectory(asmjit)
endif (ENABLE_JIT)
# end_sub

# Add postbuild commands here
# end_command

# Link subdirectories here
target_link_libraries(libs
    INTERFACE
        CLIParser
)
# end_link
