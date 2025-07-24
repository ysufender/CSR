// There are probably some bad practices
// even when considering I am writing unsafe code in rust.
// That is because I don't know rust. My apollocheese, but
// you'll have to make do.

use std::ffi::c_void;

unsafe fn integer_from_btyes(bytes: *const i8) -> u32
{
    if u16::from_ne_bytes([1, 0]) != 1 {
        return *(bytes as *mut u32)
    }
    let mut ures: u32 = 0;
    for i in 0..4 {
        ures <<= 8;
        ures |= (*(bytes.wrapping_add(i))) as u32;
    }

    return ures;
}


// Function pointer types
type BindT = unsafe extern "C" fn(*mut c_void, u32, unsafe extern "C" fn(*const i8) -> *const i8) -> i8;
type UnbindT = unsafe extern "C" fn(*mut c_void, u32) -> i8;

// Handler
unsafe extern "C" fn print_line(params: *const i8) -> *const i8
{
    let size = integer_from_btyes(params) as usize; 
    let bytes = std::slice::from_raw_parts((params as *const u8).wrapping_add(4), size);
    let text : String = bytes.iter().map(|&b| b as char).collect();
    println!("\n{}, printed in Rust!", text);
    return std::ptr::null();
}

// Extender initializer
#[no_mangle]
pub unsafe extern "C" fn InitExtender(handler: *mut c_void, binder: BindT, _unbinder: UnbindT) -> i8
{
    binder(handler, 13, print_line);
    return 0;
}
