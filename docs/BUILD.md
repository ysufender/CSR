# Building JASM

## Using Scripts

You can use the [build.sh](../build.sh) (or [build.ps1](../build.ps1)) script if you don't want to 
manually tinker with cmake. Be aware that these scripts build the project in debug mode. If you want
to build in release mode you'll have to deal with cmake directly.

### build.sh

`build.sh <-r|--refresh> <-g|--generate> <-w|--windows> <-m|--memtest> <-R|--release>`

`-r|--refresh` : Clean the build directory for a fresh start.
`-g|--generate`: Only generate build files. Do not build the project.
`-w|--windows`: If this flag is set, the script will invoke cmake with windows presets.
`-m|--memtest`: Build and run memory tests for leaks etc. Valgrind must be available on path.
`-R|--release`: Build in release mode.

### build.ps1

`build.ps1 <-refresh> <-generate>`

The flags work the same as the shell script.

Be aware that the script will also use `CMakePresets.json` since it only invokes cmake.
Go to [CMakePresets](#CMakePresets) section for more info.

## Building Manually

I advise you to use this file structure while building:

```
./
|_build
 |_ <cmake build files> 
 |_ bin
  |_ Debug
  |_ Release
```

As for now the `CMakePresets.json` only contains presets for debug build but I'll add presets for release builds too. So
you'll be able to use the presets.

## CMakePresets

The default presets are given below:

```json
"configurePresets": [
        {
            "name": "default",
            "hidden": true,
            "generator": "Ninja",
            "binaryDir": "build",
            "cacheVariables": {
                "CMAKE_CXX_COMPILER": "g++",
                "CMAKE_EXPORT_COMPILE_COMMANDS": true,
                "CMAKE_CXX_STANDARD": "20",
                "CMAKE_CXX_STANDARD_REQUIRED": true,

                "ENABLE_JIT": "OFF",
                "OUTPUT_PATH": "",
                "TOOLCHAIN_MODE": "OFF",
                "BYTEMODE": "ON"
            }
        },
        {
            "name": "Release",
            "inherits": "default",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "OUTPUT_PATH": "Release",
                "CMAKE_CXX_FLAGS": "-O3 -flto -DNDEBUG",
                "CMAKE_EXE_LINKER_FLAGS": "-flto"
            }
        }, 
        {
            "name": "Debug",
            "inherits": "default",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "OUTPUT_PATH": "Debug",
                "CMAKE_CXX_FLAGS": "-O0 -g"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "Debug",
            "configurePreset": "Debug"
        },
        {
            "name": "Release",
            "configurePreset": "Release"
        }
    ]
```

I don't advise changing the preset names, because you'll have to change the build scripts too
to match the new names. But feel free to change the variables I'll explain below (default
values are given between paranthesis).

```
default:
    generator (Ninja): As the name suggests, it's the Makefile generator you're using for cmake.
    binaryDir (build): Where the build files will be stored. Note that the resulting files will be under build/<name>/ and not build/
    CMAKE_CXX_COMPILER (g++): Pretty clear I suppose
    CMAKE_EXPORT_COMPILE_COMMANDS (true): For lsps (clangd) to work properly.
    ENABLE_JIT (OFF): Activate JIT support.
    TOOLCHAIN_MODE (OFF): Build as a library to be linked against.

Debug:
    OUTPUT_PATH (Debug): Place resulting bins to build/Debug/

Release:
    OUTPUT_PATH (Release): Place resulting bins to build/Release/
```
