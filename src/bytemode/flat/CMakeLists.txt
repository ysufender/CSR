add_library(_csr_flat
    STATIC
        flatvm.cpp
        flatram.cpp
)
add_library(csr::bytemode::flat ALIAS _csr_flat)

target_link_libraries(_csr_flat
    PRIVATE
        csr::libs
        csr::core
)
