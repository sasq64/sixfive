

#include <bbsutils/console.h>
#include "emulator.h"
#include "assembler.h"

namespace sixfive {
template <typename POLICY> void monitor(Machine<POLICY>& m) {
	using namespace bbs;
	using namespace utils;
	auto *console = Console::createLocalConsole();
	uint16_t start = 0;

	auto print = [=](const std::string &fmt, auto... params) {
		auto s = utils::format(fmt, params...);
		console->write(s);
	};

	while(true) {
		auto line = console->getLine(">>");
		auto args = split(line, " ");
		if(args[0] == "m") {
			start = stol(args[1], nullptr, 16);
			print("%04x : ", start);
			for(int i=0; i<16; i++)
				print("%02x ", m.readMem(start+i));
			print("\n");
		} else if(args[0] == "r") {
			print("PC %04x A %02x X %02x Y %02x SR %02x SP %02x\n", m.pc, m.a, m.x, m.y,m.sr, m.sp);
		} else if(args[0] == "a") {
			uint8_t output[4];
			start = stol(args[1], nullptr, 16);
			auto code = args[2];
			int len = assemble(start, output, code);
			if(len > 0) {
				m.writeRam(start, output, len);
			}
		}

	}
};

} // namespace
