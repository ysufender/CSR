set -e

clear
echo
echo "Test script is:"
echo
cat extender.jasm
sleep 4
clear
echo
echo "Caller script is:"
echo
cat alltests.sh
sleep 4

for i in $(ls -d */) 
do
    cd $i
    make
    cd ..
done
clear
for i in $(ls -d */) 
do
    cd $i
    echo
    echo "In $(basename $i)"
    ../../csr -e extender.jef -u
    echo
    echo
    cd ..
done

echo
echo "All tests passed."
echo
