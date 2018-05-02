all : boost/spirit.hpp
	make -C build

build/bulld.ninja :
	mkdir -p build
	(cd build ; cmake -G Ninja ..)

boost/spirit.hpp :
	unzip -q boost.zip


