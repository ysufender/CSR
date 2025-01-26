set -e

run=false
memtest=false

while test $# -gt 0; do
    case "$1" in
        -r|--run)
            run=true
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

prog=build/bin/Debug/csr

if [ $run == false ]; then
    if [ $memtest == true ]; then
        ./build.sh -m
        prog=build/bin/MemTest/csr
    else
        ./build.sh
    fi
    echo
fi

if  [ ! -e "$prog" ]; then
    echo "[TEST_SCRIPT] Program at $prog doesn't exist."
    exit 1
fi

echo "-----------------------------------------"
echo "                RUNNING                  "
if [ $memtest == true ]; then
    echo "                MEMTEST                  "
    echo "-----------------------------------------"
    echo
    valgrind --leak-check=yes "$prog" "$@"  
else
    echo "-----------------------------------------"
    echo
    build/bin/Debug/csr "$@"
fi
