## TODO

## Codebase

- [x] Convert all pointers to std::xxx_ptr<T>
    - NOTE: VM::asmIds is left as a raw pointer because it doesn't need to deallocate.

## Runtime

- [ ] Basic Runtime Structure
    - [ ] VM
        - [ ] Dispatch Messages
    - [ ] Assembly
        - [ ] Dispatch Messages
        - [ ] ROM
    - [ ] Board
        - [ ] RAM
            - [ ] Read byte
            - [ ] Read bytes
            - [ ]
        - [ ] CPU
        - [ ] Process
            - [ ] Dispatch Messages
