

#include <bbsutils/console.h>
#include "emulator.h"
#include "assembler.h"
#include "parser.h"

namespace sixfive {

std::string opmode() {
	return "";
}

void disasm(uint16_t org, uint8_t *mem) {
	for(const auto &ins : Machine<>::getInstructions()) {
		for(const auto &op : ins.opcodes) {
			if(op.code == *mem) {

			}
		}
	}
};

template <typename POLICY> void monitor(Machine<POLICY>& m) {
	using namespace bbs;
	using namespace utils;
	auto *console = Console::createLocalConsole();
	uint16_t start = 0;
	int size = 16;
	std::string prefix = "";
	auto print = [=](const std::string &fmt, auto... params) {
		auto s = utils::format(fmt, params...);
		console->write(s);
	};

	MonParser parser;

	while(true) {
		auto line = console->getLine(">>");
		auto cmd = parser.parseLine(line);
		if(!cmd) {
			print("?SYNTAX  ERROR\n");
			continue;
		}
		if(cmd.name == "g") {
			m.pc = cmd.args[0];
			m.run();
		} else
		if(cmd.name == "a") {
			uint8_t output[4];
			if(cmd.args.size() > 0)
				start = cmd.args[0];
			int len = assemble(start, output, cmd.strarg);
			if(len > 0) {
				m.writeRam(start, output, len);
				start += len;
				prefix = utils::format("a %04x ", start);
			}
		} else
		if(cmd.name == "m") {
			if(cmd.args.size() > 0)
				start = cmd.args[0];
			if(cmd.args.size() > 1)
				size = cmd.args[1];
			print("%04x : ", start);
			for(int i=0; i<size; i++)
				print("%02x ", m.readMem(start+i));
			print("\n");
		} else
		if(cmd.name == "r") {
			print("PC %04x A %02x X %02x Y %02x SR %02x SP %02x\n", m.pc, m.a, m.x, m.y,m.sr, m.sp);
		}
	}
};

} // namespace
