.PHONY: clean all build

all: clean build
	odin build extender.odin -file -build-mode=shared
	mv extender.so libextender.so
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
