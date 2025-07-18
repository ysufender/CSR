## TODO

## Codebase

- [x] Convert all pointers to std::xxx_ptr<T>
    - NOTE: VM::asmIds is left as a raw pointer because it doesn't need to deallocate.

## Runtime

- [x] Basic Runtime Structure
    - [x] VM
        - [x] Dispatch Messages
    - [x] Assembly
        - [x] Dispatch Messages
        - [x] ROM
    - [x] Board
        - [x] RAM
        - [x] CPU
        - [x] Process

## Ideas

- VM should check for the same named dll/so/dylib file and load it, then call the initialization
function for it to set up the syscall handlers and such.
- Finish MakeFunctionHandler
