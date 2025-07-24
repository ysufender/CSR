.PHONY: clean all build

all: clean build
	rustc --crate-type=cdylib extender.rs
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
