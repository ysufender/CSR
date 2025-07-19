cmake_minimum_required(VERSION 3.30)

project(CSR
    VERSION 0.1.0
    DESCRIPTION "Common Script Runtime for JASM Bytecode"
    LANGUAGES CXX
)

if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(
        FATAL_ERROR 
        "Sorry this project is meant to be compiled with a GNU compiler.
        You can still compile it but I've never tested it with anything other
        than g++.
        
        If you still want to continue, you can remove this check in CSR/CMakeLists.txt"
    )
endif(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDART_REQUIRED TRUE)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    #add_compile_options("-O0")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "MemTest")
    message(STATUS "Building in MemTest mode.")
    add_compile_options(
        "-g"
        "-O0"
    )
endif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

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
option(ENABLE_JIT "Optional JIT " OFF)
set(OUTPUT_PATH "" CACHE STRING "")

#
# File Configures
#
configure_file(
    ${CMAKE_SOURCE_DIR}/CSRConfig.hpp.in
    ${CMAKE_BINARY_DIR}/include/CSRConfig.hpp
    @ONLY
)

add_executable(csr
    src/main.cpp
)

set_target_properties(csr
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${OUTPUT_PATH}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin//${OUTPUT_PATH}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${OUTPUT_PATH}/"
)

include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
)

# After the include call so everything includes those above
add_subdirectory(lib)
add_subdirectory(src)

target_link_libraries(csr
    PUBLIC 
        libs
        src
)
