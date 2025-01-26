#!/bin/bash

set -e

refresh=false
generate=false
windows=false
memtest=false

while test $# -gt 0; do
    case "$1" in
        -r|--refresh)
            refresh=true
            shift
            ;;
        -g|--generate)
            generate=true
            shift
            ;;
        -w|--windows)
            windows=true
            shift
            ;;
        -m|--memtest)
            memtest=true
            shift
            ;;
        *)
            break
    esac
done

if [[ $refresh == true && -d build ]]; then
    echo "[BUILD_SCRIPT] Cleaning the build directory..."
    rm -rf build
fi

preset="Debug"

if [ $windows == true ]; then
    echo "[BUILD_SCRIPT] Building for Windows"
    preset="Debug-MinGW" 

    if [ $memtest == true ]; then
        echo "[BUILD_SCRIPT] MemTest is only supported for linux builds"
        exit 1
    fi
fi

if [ $memtest == true ]; then
    preset="MemTest" 
fi

if [ -d build/Debug ]; then
    echo "[BUILD_SCRIPT] Already generated."
else
    echo "[BUILD_SCRIPT] Generating build files."
    cmake --preset $preset 
fi

if [ $generate == true ]; then
    echo "[BUILD_SCRIPT] Generate-only mode"
    exit
fi

cmake --build build --preset $preset "$@"
