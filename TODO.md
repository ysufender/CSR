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

- [ ] Advanced Runtime
    - [x] Standard library (loading part, the standard library itself is at [it's own repo](https://github.com/ysufender/libstdjasm))
    - [ ] C++ Callbacks
        - [x] Loading extender DL and calling their initializer functions to bind function handlers.
        - [x] Calling functions using cal/calr instructions
        - [x] Passing parameters to callbacks
        - [ ] Retrieving return values from callbacks (probably works but I didn't test them yet)
    - [ ] Proper Concurrency

## Notes

- If you see the mentions of "C++ callback" around the project, know that it isn't only C++. As long as you have a C ABI compatible binary and you follow
the VM regulations (correct Initializer/FunctionHandler signatures) you can call any language from the bytecode.

## Ideas
