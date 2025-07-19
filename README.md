# CSR - Common Script Runtime 

CSR is the runtime of JASM bytecode. The purpose of CSR is to give life to JASM bytecode and make it 
useful. After all, a bytecode that can't be run doesn't really mean anything. In addition, the future plans 
for this project includes adding JIT compilation support for JASM bytecode.

CSR is a part of CSLB project. The CSLB project (which stands for Common Scripting Language Backend, pronunced as SeezleBee)
consists of three parts: Assembler, Linker and Runtime. The runtime part is CSR itself, as can be understood from the name.
The assembler and linker make up JASM and it has its [own repo](https://github.com/ysufender/JASM).

## Quickstart

### Installation

You can either grab the compiled binaries from the release section (if there is any), or build CSR from source. I recommend
building from source since it's pretty easy.

#### Building From The Source

See [BUILD.md](docs/BUILD.md)

### Basic CLI Usage

Here is the helper text from the current version of CSR:

```

Common Script Runtime (CSR)
        Description: Common Script Runtime for JASM Bytecode
        Version: 0.1.0
        Enable JIT: Unavailable

Available Flags:
        --help , -h : Print this help text.
        --version , -v : Print version.

        --no-new , -n : Do not create a new instance of CSR, use an already running one.
        --no-strict-messages , -nsm : Don't strictly verify messages in each checkpoint when dispatching.

        --exe <..params..>, -e : Executable files to execute.

        --unsafe , -u : Load extender dll of each executable.

        --step , -s : Run the VM once every input.
```

### CSR Documentation 

If you want to know more about how CSR works you can start by reading the [docs](docs/DOCUMENTATION.md)

## Footnotes

The licenses, readmes and citations for every library used lies within its own directory
under `lib`.
