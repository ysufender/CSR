add_library(test INTERFACE)
set(test_modules "")

if (BYTEMODE)
    add_subdirectory(bytemode)
    list(APPEND test_modules bytemode_test)
endif (BYTEMODE)

target_link_libraries(test
    INTERFACE
        ${test_modules}
)
