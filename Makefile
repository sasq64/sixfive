
all :
	clang++ -lbenchmark -Wl,-map,sixfive.map -O2 -g -I../apone/mods -std=c++11 sixfive.cpp ../apone/mods/coreutils/file.cpp -osixfive

