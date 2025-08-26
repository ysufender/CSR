add_library(libs INTERFACE)

# Add prebuild commands here
if (ENABLE_JIT)
    set(ASMJIT_STATIC true)
    set(ASMJIT_NO_FOREIGN true)
    set(ASMJIT_NO_BUILDER true)
endif (ENABLE_JIT)
# end_pre

# Add subdirectories here
add_subdirectory(CLIParser)

if (ENABLE_JIT)
    message(STATUS "Entering asmjit...")
    add_subdirectory(asmjit)
endif (ENABLE_JIT)

if (BUILD_TEST)
    message(STATUS "Entering catch")
    add_subdirectory(catch2)
endif (BUILD_TEST)
# end_sub

# Add postbuild commands here
# end_command

# Link subdirectories here
if (ENABLE_JIT)
    target_link_libraries(libs
        INTERFACE
            asmjit::asmjit
    )
endif (ENABLE_JIT)

if (BUILD_TEST)
    target_link_libraries(libs
        INTERFACE
            Catch2::Catch2WithMain
    )
endif (BUILD_TEST)

target_link_libraries(libs
    INTERFACE
        CLIParser
)
# end_link
