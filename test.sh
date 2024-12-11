set -e

run=false

while test $# -gt 0; do
    case "$1" in
        -r|--run)
            run=true
            shift
            ;;
        *)
            shift
            break
            ;;
    esac
done

if [ $run == false ]; then
    ./build.sh
    echo
fi
echo "-----------------------------------------"
echo "                RUNNING                  "
echo "-----------------------------------------"
echo
build/bin/Debug/csr
