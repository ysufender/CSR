include(ExternalProject)

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

message(STATUS "Entering libffi")
ExternalProject_Add(libffi_ext
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/libffi
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/lib/libffi ./autogen.sh && ./configure --prefix=${CMAKE_BINARY_DIR}/lib/libffi --includedir=${CMAKE_BINARY_DIR}/include/libffi
  BUILD_COMMAND make
  INSTALL_COMMAND make install
  BUILD_IN_SOURCE 1
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/lib/libffi/lib/libffi.a
)

add_library(libffi STATIC IMPORTED GLOBAL)
add_library(libffi::libffi ALIAS libffi)

set_target_properties(libffi PROPERTIES
  IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/libffi/lib/libffi.a
  INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}/include/libffi
)

add_dependencies(csr libffi_ext)
target_link_libraries(csr PRIVATE libffi::libffi)
list(APPEND libraries "libffi::libffi")
# end_sub

# Add postbuild commands here
# end_command

# Link subdirectories here
target_link_libraries(_csr_libs
    INTERFACE
        ${libraries}
)
# end_link
