.PHONY: clean all build

all: clean build
	gcc -c -fpic extender.c
	gcc -shared -o libextender.so extender.o
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
