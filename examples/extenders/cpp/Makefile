.PHONY: clean all build

all: clean build
	g++ -c -fpic extender.cpp -std=c++20
	g++ -shared -o libextender.so extender.o
	clear
	../../csr -e extender.jef -u

clean:
	rm -f *.so
	rm -f *.jef
	cd ../../ && make clean

build:
	cd ../../ && make prepare
	../../jasm -I ../extender.jasm
	cp ../extender.jef .
