// There are probably some bad practices here.
// That is because I don't know Odin, I somehow
// googled my way through. My apollocheese, but
// you'll have to make do. 

package test

import "base:runtime"
import "core:strings"
import "core:fmt"

IntegerFromBytes :: proc(bytes: [^]byte) -> u32 {
    endian_test: union {
        u16,
        [2]byte,
    } = 0x0100

    i   : u16 = 0x0102;
    ptr := &i;
    first_byte := (cast(^u8)ptr)^;

    if first_byte == 0x01 {
        return (cast(^u32)(bytes))^
    }

    result: u32 = 0
    for i in 0..<4 {
        result <<= 8
        result |= cast(u32)(bytes[i])
    }

    return result
}

// Function pointer types
bind_t :: proc "cdecl" (handler: rawptr, id: u32, callback: proc "cdecl" ([^]byte) -> [^]byte) -> byte
unbind_t :: proc "cdecl" (handler: rawptr, id: u32) -> byte

// Handler
Print :: proc "cdecl" (params: [^]byte) -> [^]byte {
    context = runtime.default_context()
    size := cast(int)(IntegerFromBytes(params))
    byte_ptr := cast(cstring)(&params[4])

    fmt.println()
    fmt.println(strings.clone_from_cstring_bounded(byte_ptr, size), "printed in Odin!")
    return nil
}

// Extender Initializer
@(export)
InitExtender :: proc "cdecl" (handler: rawptr, binder: bind_t, unbinder: unbind_t) -> byte {
    context = runtime.default_context()
    binder(handler, 13, Print)
    return 0
}
