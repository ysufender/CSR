#!/bin/bash

set -e

refresh=false
generate=false
windows=false
memtest=false
release=false

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
        -R|--release)
            release=true
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

if [ $windows == true ]; then
    echo "[BUILD_SCRIPT] Building for Windows"
    preset="Debug-MinGW" 

    if [ $memtest == true ]; then
        echo "[BUILD_SCRIPT] MemTest is only supported for linux builds"
        exit 1
    fi
fi

preset="Debug"
if [ $memtest == true ]; then
    preset="MemTest" 
elif [ $release == true ]; then
    preset="Release"
fi

echo "[BUILD_SCRIPT] Generating build files."
cmake --preset $preset 

if [ $generate == true ]; then
    echo "[BUILD_SCRIPT] Generate-only mode"
    exit
fi

cmake --build --preset $preset "$@"
