#!/bin/bash

set -e

refresh=false
generate=false

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
        *)
            shift
            break
            ;;
    esac
done

if [[ $refresh == true && -d build ]]; then
    echo "[BUILD_SCRIPT] Cleaning the build directory..."
    rm -rf build
fi

if [ -d build/Debug ]; then
    echo "[BUILD_SCRIPT] Already generated."
else
    echo "[BUILD_SCRIPT] Generating build files."
    cmake --preset Debug
fi

if [ $generate == true ]; then
    echo "[BUILD_SCRIPT] Generate-only mode"
    exit
fi

cmake --build build --preset Debug
