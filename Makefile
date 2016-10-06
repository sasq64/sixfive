all : boost/spirit.hpp build/Makefile
	make -C build

build/Makefile :
	mkdir -p build
	(cd build ; cmake ..)

boost/spirit.hpp :
	unzip -q boost.zip


