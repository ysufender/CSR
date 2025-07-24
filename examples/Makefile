.PHONY: clean prepare

clean:
	rm -f *.jef
	rm -f *.jo
	rm -f csr
	rm -f jasm
	rm -f libstdjasm.so

prepare:
	cd ../ && ./build.sh
	cd ../../JASM && ./build.sh
	cp ../build/bin/Debug/csr ./
	cp ../../JASM/build/bin/Debug/jasm ./
	cp ../build/bin/Debug/libstdjasm.so ./
