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
building from source since it's pretty easy. See [BUILD.md](./BUILD.md).

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
        - [Calling Native Functions](#calling-native-functions)
        - [Extenders](#extenders)
        - [ISysCallHandler](#isyscallhandler)
        - [About Parameters](#about-parameters)
    - [End](#end)

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

CSR is a small runtime. It is basic and is meant to be that way too. But I tried to design
the project structure and the existing runtimes to be easily extendable. The standard runtime
is the bytecode runtime, any other runtime might not be complete or even present at this point.

The bytecode runtime is extendable as in you can easily add native callbacks to your scripts
by placing a dynamic library in the same directory and with the same name (without extension)
as the script file. The [native callbacks](#native-callbacks) section covers this topic.

As for the project structure, any future runtimes will be added under the `src` directory
with their respective names. For example if a JIT runtime is added, it'll be under 
`src/jit/`. A few tweak to the CMake scripts will enable a modular build process too.

For detailed information about how the bytecode runtime works, see the [CSR VM](#csr-vm)
section below.

## CSR VM

CSR VM is the general name of the bytecode runtime. The bytecode runtime (although not complete
at the moment) is designed to easily handle concurrent processes and interprocess communication.
The messaging system implemented in each key node of the runtime tree enable interprocess,
interboard and interassembly communication, therefore enabling wide scripting capabilities
when combined with native callbacks and extenders.

The runtime tree is made up of four important elements: VM, Assemblies, Boards and Processes.
A running instance of CSR can only have one VM, which holds at least as many Assemblies as
the executable scripts that has been passed to CSR, which hold multiple Boards that hold multiple
Processes to enable:

1- Illusional concurrency via interrupting Processes that are connected to the same Board, and
2- True concurrency via duplicating Assemblies or Assemblies having multiple Boards.

Note that since each board has its own RAM, it is perfectly fine to run them on different threads
without the fear of deadlocks, since they won't be accessing the same resource and depending
on one another.

### VM

VM is the godfather of the runtime. Its responsibilities are:

1- Managing Assemblies
    - Adding assemblies and loading the standard library and the extenders for them
    - Removing assemblies when they send the shutdown signals
    - Handling any possible unhandled error that is thrown and isn't handled within
    the Assemblies
2- Managin interassembly communication by being a checkpoint for assemblies, and from time
to time, handling the messages directly without passing them anywhere.
3- Running the assemblies.

As for now, only the 1st and 3rd responsibilities are done and the 2nd is partly done.
VM acts as a checkpoint for messages, passes messagess between Assemblies and handles
the messages sent to it by assemblies, such as the shutdown signals. I hope I won't forget
to update this part when I add the remaining too.

### Assembly

Assembly is the firstborn of VM. It loads the given executable script and handles it
as a ROM, as well as handling the multiple (optionally) Boards and their communications
by acting as a checkpoint just like the VM.

Each important element of the runtime tree implements the interface IMessageObject. And
each one of them only handles the basic parts of the messaging such as the ones VM handle.
As a part of the important elements, Assemblies too can only handle basic messaging.

An Assembly can be of two types: Executable and Library. A Library Assembly is a `.shd` file
(a shared library). It doesn't have any boards or processes since it is only there as a
library and not an executing target. When a process tries to call a function or retrieve a value
from it, it should do it so by making a syscall to access the shared library.

#### ROM

ROM handles the bytecode as a readonly series of bytes. To prevent the whole VM from crashing,
it does boundary and safety checks each time someone tries to access something from it, and
returns the appropriate ErrorCode. But generally you'd want to wrap the ROM in a try/catch
block and handle the exceptions it throws, because sadly indexers can't return multiple values
and I don't want to use structs or tuples.

ROM holds the readonly data as a smart pointer, and gets destructed when its parent Assembly
gets destructed, when nobody needs to access the ROM. So it is safe in terms of memory management.

### Board

Board is actually what runs the script under the hood. It accesses its parent Assembly's ROM
and executes instructions from it with its CPU. It is also a checkpoint and inherits the
IMessageObject interface. Board also handles the interrupts of Processes by checking the interrupt
messages sent to it by Processes. It cycles between them to create the illusion of concurrency.

Boards are the brain of the runtime. They each hold a CPU and RAM that are shared between a Board's
child Processes. 

A Board's RAM is allocated beforehand when the Board is created. It is safe in terms
of boundaries and deadlocks because no Process can try to access it at the same time. And since
both the stack and the heap are allocated beforehand, no extra allocation is done at runtime.

A Board's CPU is a small class in terms of stored data. It only holds the State of the CPU.
A State is just the registers and their values. Other than that, CPU holds all instruction
functions and calls the correct function for each instruction.
 
#### CPU

A CPU consists of a State and instruction functions. A State contains all the registers present
on the CPU. For more information about the registers, check the 
[JASM Documentation](https://github.com/ysufender/JASM/blob/master/docs/DOCUMENTATION.md#registers).


When the VM runs the next iteration, it calls the Assemblies to do the same. Each Assembly
calls their Boards and Each board calls their Processes. When `Process::Cycle` is called, the
process calls `CPU::Cycle` after doing some checks. When a CPU is in a cycle, it fetches the
next instruction from its parent Assembly and gets the op. Then calls the instruction function
associated with that op, if there is no such instruction, it returns `InvalidInstruction`.

Although its not a best practice to use `friend class`es, because a CPU has to access to its Board
but a Board's contents must be isolated from the Assembly the CPU must be a `friend` of Board. Same
goes for a Process, since it needs to access to the CPU, which is done by accessing the Board.
This whole circular accessing is probably because of my ill formed code but whatever.

#### RAM

A RAM is a handler for a preallocated memory chunk. It allocates the memory when it is constructed
using a smart pointer, and does many boundary checkings when someone tries to access something.
So it is safe in terms of memory leaks and indexing.

A RAM is made up of two parts: stack and heap. Stack size and heap size must be known at startup,
an as for now there is no memory reallocations in RAM class so heap is also limited. This prevents runtime
allocations since everything is preallocated in a bulk and deallocated in a bulk too. Keep in mind
that RAM holds an allocation map to keep track of which cell of heap is allocated and which is not.
Due to the design of this mapping system (keeping track of each cell by assigning a bit to it), heap 
size must be a multiple of 8. Since JASM Bytecode is meant to be generated by compilers and not written
by hand, heap size check is only done in Debug builds.

### Process

Process might be the simplest one among the other important elements of the runtime. It only
holds a CPU State along with implementing the IMessageObject. When `Process::Cycle` is called,
a Process checks if the `Program Counter` is reached to the end of the ROM or not. If so
it sends a shutdown signal to its parent Board. If not, thenit checks if the current instruciton
creates/destroys a callstack or not. If so then it sends a message to its parent Board, indicating
that it is time to change the Executing Process. Then it calls the `CPU::Cycle` to execute
the instruction regardless. If there happens an exception inside CPU that is fatal or can't be
recovered from, the Process logs the error and sends a shutdown signal.

And this is everything that a Process is responsible of.

## The JASM Bytecode

JASM bytecode is a primal bytecode format that is compact and, as far as I tested, fast enough
to execute. It contains 124 instructions, with most of them being the same instruction with different
executions styles or operands. For example `addi, addf, addb, addri, addrf, addrb, addsi, addsf` and `addsb`
are all just different types of `add`. Bytecode is not checked for any "semantic" errors during or before the
execution since it is expected to be correctly formed before being passed to the runtime. If a call is made to
the wrong index of the program, then CSR will just do what it is told and jump to that index. However there is
no security problems with this since ROM is readonly and execution is only done by reading the ROM.

As far as I know, endianness is not a problem with CSR bytecode. The same bytecode will be formed on every
system when JASM is ran, and the same output will be given on every system when CSR is ran. If you want to know more
about the instructions themselves, you should check the
[JASM Documentation](https://github.com/ysufender/JASM/blob/master/docs/DOCUMENTATION.md#list-of-instructions).

## Native Callbacks

Native callbacks were the sole reason I wanted to do this project. Story time now. Once upon a time, I wanted to make
a game. I am an Elder Scrolls fan, and especially a fan of the modding capabilities of Skyrim. So as a dumb and young
man I decided to create an immensely moddable game. So I started searchin around and found out that I needed to use
scripting languages. I tried to use Lua but due to my inexperience I failed to do so. Then I gave up on making a moddable
game. Then I wanted to create a dialogue plugin for [Godot Engine](https://github.com/godotengine), so I decided to create
a DSL in C# to allow the DSL to interop with .NET. Anyway, that lead me to creating [SlimScript](https://github.com/ysufender/SlimScript).
But since SlimScript is slow, ugly and stupid in design, I decided to give up and move on with my life.

But then, one day I learned about LLVM and was amazed by it. Then I asked myself, why there is no such thing for scripting
languages? If there was something like that, every scripting language could interact with:

1- Each other, and
2- Any language that can abide by C ABI and CSR VM extender standards.

So I started to form an IL and an assembler along with a linker for scripting languages to use as a backend. Then I made CSR to
give life to it. So now we're here, just because I wanteed to call some native functions from scripts.

Now, technically, as long as a dynamic library has the following qualifications:

1- Is in the same directory as the script file and has the same name. For example `proj/script.so` is the extender for `proj/script.jef`.
2- Has an `InitExtender` function following the signature `char __cdecl InitExtender(ISysCallHandler*)` that's name isn't mangled and is following C ABI.
3- Binds functions to ids using `ISysCallHandler::BindFunction(uint32_t, SysFunctionHandler)` where `SysFunctionHandler` is the
function signature `const char* const __cdecl FunctionName(const char* const)`.

it can bind functions to ids in an Assembly's SysCallHandler, which can then be called by the `cal` instruction. `cal`
instruction works the same for native calls, `bl` contains the parameter size in bytes and the top of the stack contains
the parameters of that size. The passed address must be the function id bound to that specific system call.

JASM Standard Library is literally just a bunch of bindings for native calls. The `libstdjasm` dynamic library resides beside
the `csr` executable and when an Assembly is added to the VM, the VM loads that library and calls it's `STDLibInit` function which
has the same signature as `InitExtender`s. The passed `ISysCallHandler*` is the pointer to the SysCallHandler of the added Assembly.
So standard function calls can be done using syscalls!

### Calling Native Functions

Calling native functions is the same as calling JASM functions. Just set `bl` to the parameter size
and keep the parameters at the top of the stack. Here is a little example, calling the standard library
to print "Hello World" to stdout, using a string from the stack.

```
.prep
    org main
    sts 32
    sth 0
.body
    main:
        stc %i 11 #size of the string to be printed#
        raw "Hello World" ; #string on stack#

        inc %b &flg 1 #the rightmost bit of the flg is syscall flag. initially set to zero# 
        mov 15 &bl #string size size + string size#

        cal 0x0 #printing from stack has the id 0#
.end

Output is:
    Hello World
```

As you can see, it is pretty simple. Since the syscall doesn't return anythin in this example,
nothing is pushed to the stack. But if it were such a syscall that returned a function, the return
value would be pushed to the stack just like it is pushed when calling JASM functions.

### Extenders

I explained what extenders are in the main header [Native Callbacks](#native-callbacks).
Now I want to give an example on how to create an extender using the `PrintLine` function of the
standard library.

```cpp
// libstdjasm.cpp

#if defined(_WIN32) || defined(__CYGWIN__)
#define API(type) extern "C" type __declspec(dllexport) __cdecl 
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
#define API(type) extern "C" type
#endif

API(char) STDLibInit(ISysCallHandler* handler)
{
    handler->BindFunction(0, &PrintLineHandler);
    return 0;
}
```

The `STDLibInit ` is literally the same as an `InitExtender` just with a different name. As you can see
it binds the function `PrintLineHandler` to id 0 for the given handler, which happens to be the handler
of the Assembly that is currently being added. Let's take a look at the `PrintLineHandler` function.

```cpp
// handlers.cpp

#if defined(_WIN32) || defined(__CYGWIN__)
#define HANDLER const char* const __cdecl 
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
#define HANDLER const char* const
#endif

HANDLER PrintLineHandler(const char* const params) noexcept
{
    try
    {
        sysbit_t size { IntegerFromBytes<sysbit_t>(params) };
        std::cout << std::string_view {params+4, size} << '\n';
        return nullptr;
    }
    catch (const std::exception&)
    {
        return new char[2] { 1, 0 };
    }
}
```

As you can see, the handler reads the size of the string from the first 4 bytes, then forms a
`string_view` using the rest of the bytes and the size. `return nullptr` indicates that the function
exited correctly and doesn't have a return type. The `new char[2] { 1, 0 }` is the return format of
handlers. The first byte is return code, `System::ErrorCode::Bad` in this case. The second byte is the
return size in bytes, since the function is void it's 0 in this case.

As you can see, the standard library is basically an extender for every Assembly the VM contains. If we
use the same `handlers.cpp` but use the code below instead of using `libstdjasm.cpp`

```cpp
// scriptextender.cpp

#if defined(_WIN32) || defined(__CYGWIN__)
#define API(type) extern "C" type __declspec(dllexport) __cdecl 
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
#define API(type) extern "C" type
#endif

API(char) InitExtender(ISysCallHandler* handler)
{
    handler->BindFunction(0, &PrintLineHandler);
    return 0;
}
```

Now we made it a proper extender! For a script named `somescript.jef`, if we compile the code to a file
name `somescript.so` (or `.dll` or `.dylib` depending on your OS) and run `csr` with `--unsafe` flag,
it'll extend the script!

### ISysCallHandler

Due to certain technical problems and ABI incompatibilities, sharing objects (especially
STL objects) is a problematic thing. Memory layouts of types on binaries built on different computers
might be different, especailly STL libraries since their memory layout can change drastically.
Because of that, if you're going to pass some object to a native call or retrieve from an object
call you should serialize it to bytes in such a way that it is precisely the same no matter
what. But as for now since Microsoft has the [COM](https://en.wikipedia.org/wiki/Component_Object_Model)
There is a pretty good consistency with vtables when using interfaces (pure virtual functions).
And it is pretty consistent on Unix based systems too. So, I created the pure virtual `ISysCallHandler`
interface to take advantage of that consistency and use it to pass the pointer. Since you won't need the
internal things of a `SysCallHandler` the interface gives a clean, well, interface too.

### About Parameters

Well, since the byte layout must be the same on everywhere for JASM Bytecode to work, I advise you
to use the serialization functions defined in [converters.hpp](../include/extensions/converters.hpp).
They're header only too (excluding the float ones) so it is easy to just copy paste them and use.

Probably you noticed the use of `IntegerFromBytes` under the section [Extenders](#extenders) anyway.

There are some neat serialization/deserialization functions in 
[JASM codebase](https://github.com/ysufender/JASM/blob/master/include/extensions/serialization.hpp)
too. They need a bit of tinkering since they're supposed to be used with streams rather than
buffers but it's not that hard methinks.

## End

So, this is it. This was the CSR documentation. I honest both enjoyed and hated this project.
Well, I managed to do a native callback system just now so I'm more on the enjoyed side but anyway.
Thank you, whoever you are, for reading this documentation. Or if not for even bothering
to look at the end of it. Let me know of any problems in the codebase. Be safe now.
