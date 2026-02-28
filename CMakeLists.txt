cmake_minimum_required(VERSION 3.30)

project(CSR
    VERSION 0.2.1
    DESCRIPTION "Common Script Runtime for JASM Bytecode"
    LANGUAGES CXX
)

if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)")
    message(
        FATAL_ERROR 
        "Sorry this project is meant to be compiled with a GNU/Clang compiler.
        since it uses features supported only by those two. I never tested with MSVC
        of any other though, if you really want to give it a shot, you can remove this
        check in the main CMakeLists.txt file."
    )
endif (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)")

set(TARGET_NAME "csr")

#
# Link libstdc++ statically if compiling with mingw. because it
# requires the dlls otherwise.
#
message(STATUS "Checking CXX compiler.")
get_filename_component(CXX_COMPILER_NAME ${CMAKE_CXX_COMPILER} NAME)
if("${CXX_COMPILER_NAME}" STREQUAL "x86_64-w64-mingw32-g++")
    message(STATUS "Using ${CXX_COMPILER_NAME}, linking libstdc++ statically.")
    add_link_options(
        "-static" 
        "-static-libstdc++" 
        "-static-libgcc"
    )
endif("${CXX_COMPILER_NAME}" STREQUAL "x86_64-w64-mingw32-g++")

#
# CLI Arguments 
# 
message(STATUS "BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "ENABLE_JIT: ${ENABLE_JIT}")
message(STATUS "OUTPUT_PATH: ${OUTPUT_PATH}")
message(STATUS "TOOLCHAIN_MODE: ${TOOLCHAIN_MODE}")

#
# File Configures
#
configure_file(
    ${PROJECT_SOURCE_DIR}/CSRConfig.hpp.in
    ${PROJECT_BINARY_DIR}/include/CSRConfig.hpp
    @ONLY
)

if (TOOLCHAIN_MODE)
    add_library(${TARGET_NAME}
        src/main.cpp
    )
else()
    add_executable(${TARGET_NAME}
        src/main.cpp
    )
endif (TOOLCHAIN_MODE)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/${OUTPUT_PATH}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin//${OUTPUT_PATH}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/${OUTPUT_PATH}/"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
)

if (TOOLCHAIN_MODE)
    target_include_directories(${TARGET_NAME}
        PUBLIC
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_BINARY_DIR}/include           
    )
endif (TOOLCHAIN_MODE)

# After the include call so everything includes those above
add_subdirectory(lib)
add_subdirectory(src)

# Add postbuild commands here
#add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#            "${LIBSTDJASM_OUTPUT_DIR}"
#            "${PROJECT_BINARY_DIR}/bin/${OUTPUT_PATH}/"
#)
#end_command

target_link_libraries(${TARGET_NAME}
    PUBLIC 
        csr::libs
        csr::src
)
