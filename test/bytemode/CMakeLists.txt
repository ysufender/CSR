add_library(bytemode_test INTERFACE)
set(test_modules "")

if (ENABLE_JIT)
    add_subdirectory(jit)
    list(APPEND test_modules jit_test)
endif (ENABLE_JIT)

if (BUILD_FLAT)
    add_subdirectory(flat)
    list(APPEND test_modules flat_vm_test)
endif (BUILD_FLAT)

if (BUILD_STRUCTURED)
    add_subdirectory(structured)
    list(APPEND test_modules structured_vm_test)
endif (BUILD_STRUCTURED)

if (TOOLCHAIN_MODE)
    add_subdirectory(toolchain)
    list(APPEND test_modules toolchain_test)
endif (TOOLCHAIN_MODE)

target_link_libraries(bytemode_test
    INTERFACE
        ${test_modules}
)
