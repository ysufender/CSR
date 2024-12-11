param(
    [switch]$refresh,
    [switch]$generate
)

$ErrorActionPreference='Stop'

if ($refresh) {
    echo "[BUILD_SCRIPT] Cleaning the build directory..."
    rm -r -Force build
}

if (Test-Path "build/Debug") {
    echo "[BUILD_SCRIPT] Already generated."
} else {
    echo "[BUILD_SCRIPT] Generating build files..."
    cmake --preset Debug
}

if ($generate) {
    echo "[BUILD_SCRIPT] Generate-only mode."
    exit
}

cmake --build build --preset Debug
