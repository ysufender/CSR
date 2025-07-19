# CSR Documentation

## Introduction

CSR is the runtime of JASM bytecode. The purpose of CSR is to give life to JASM bytecode and make it 
useful. After all, a bytecode that can't be run doesn't really mean anything. In addition, the future plans 
for this project includes adding JIT compilation support for JASM bytecode.

CSR is a part of CSLB project. The CSLB project (which stands for Common Scripting Language Backend, pronunced as SeezleBee)
consists of three parts: Assembler, Linker and Runtime. The runtime part is CSR itself, as can be understood from the name.
The assembler and linker make up JASM and it has its [own repo](https://github.com/ysufender/JASM).

## Installing CSR

You can either grab the compiled binaries from the release section (if there is any), or build CSR from source. I recommend
building from source since it's pretty easy.

## Table of Contents

- [CSR Documentation](#CSR-documentation)
        - [Introduction](#introduction)
        - [Installing CSR](#installing-csr)
        - [Table of Contents](#table-of-contents)
        - [The Project Structure](#the-project-structure)
            - [The CLI](#the-cli)
            - [The Runtime](#the-runtime)
        - [CSR VM](#csr-vm)
            - [VM](#vm)
            - [Assembly](#assembly)
                - [ROM](#rom)
            - [Board](#board)
                - [CPU](#cpu)
                - [RAM](#ram)
            - [Process](#process)
        - [The JASM Bytecode](#the-csr-bytecode)
        - [Native Callbacks](#native-callbacks) 

## The Project Structure

```
./
|_ test/ -----------------------> contains various files which I used to test the runtime. I left them there for funs.
|_ docs/ -----------------------> contains documentation `.md`s.
|_ include/
| |_ bytemode/
| |_ extensions/
|_ lib/ ------------------------> contains third-party libraries
|_ src/
  |_ bytemode/ -----------------> the project is designed to allow future improvements and additions like JIT
  |_ core/ ---------------------> the core functions that doesn't change depending on the target mode
  |_ extensions/ ---------------> various utility functions, like serialization of types to bytes.
```

### The CLI

The CLI is mostly handled by the [CLI Parser](https://github.com/ysufender/CLIParser), so if you want to know how it works you have to check its source.
As of the CLI usage, basic CLI usage is covered in [README.md](../README.md#basic-cli-usage). If you wan't to know more about the CLI parameters and flags,
this section covers that.

#### jit

`csr --jit`

This flag is only available when the binary is built with JIT support. Currently, it is unavailable.
But once it is, the runtime will mark the execution as JIT compatible and try to use it as much as it can.

#### no-new

`csr --no-new` or `csr -n`

By default, every time CSR is opened, it'll pop up a new instance and run the given files. If the
`no-new` flag is activated, then CSR will attach the given files to an already running instance and
execute them from there. If there is no such instance, a new instance will be created.

This flag is currently available, but doesn't do anything.

#### no--strict-messages

`csr --no-strict-messages` or `csr -nsm`

By default, CSR will do safety controls when dispatching messages in its messaging system. The strict
message checking might slow the execution a little bit. If this flag is enabled, CSR will do the checks
once for every message, instead of at every checkpoint.

#### exe

`csr --exe <..files..>` or `csr -e <..files..>`

The executables to be executed. They must be `.jef` files, otherwise the VM will complain
and terminate.

#### unsafe

`csr --unsafe` or `csr -u`

CSR allows native function callings via attaching dynamic libraries at runtime and binding
function pointers to `SysCallHandler`s that every Assembly possesses. To extend the capabilities
of an executable, the VM will load and initialize the dynamic library that resides within the same
directory with the executable file, which also has the same name.

Since this dynamic loading process is open to various vulnerabilities, CSR doesn't do that by default.
If you are sure of the DLs security, then enabling the `unsafe` flag will allow the VM to load the extender.

#### step

`csr --step` or `csr -s`

Only available in Debug builds.

If this flag is enabled, the VM will execute the given files one instruction at a time, and
will not continue unless a key is pressed.

### The Runtime

## CSR VM

### VM

### Assembly

#### ROM

### Board

#### CPU

#### RAM

### Process

## The JASM Bytecode

## Native Callbacks
