add_library(_csr_libs INTERFACE)
add_library(csr::libs ALIAS _csr_libs)
set(libraries "")

# Add prebuild commands here
if (ENABLE_JIT)
    set(ASMJIT_STATIC true)
    set(ASMJIT_NO_FOREIGN true)
    set(ASMJIT_NO_BUILDER true)
endif (ENABLE_JIT)
# end_pre

# Add subdirectories here
if (NOT TOOLCHAIN_MODE)
    message(STATUS "Entering CLIParser...")
    add_subdirectory(CLIParser)
    list(APPEND libraries "CLIParser")
endif (NOT TOOLCHAIN_MODE)

if (ENABLE_JIT)
    message(STATUS "Entering asmjit...")
    add_subdirectory(asmjit)
    list(APPEND libraries "asmjit::asmjit")
endif (ENABLE_JIT)
# end_sub

# Add postbuild commands here
# end_command

# Link subdirectories here
target_link_libraries(_csr_libs
    INTERFACE
        ${libraries}
)
# end_link
