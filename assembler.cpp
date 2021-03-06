
#include "assembler.h"
#include "emulator.h"
#include <coreutils/utils.h>
#include <regex>

namespace sixfive {


struct Arg {
	Arg(AdressingMode am = BAD, int val = -1) : mode(am), val(val) {}
	AdressingMode mode;
	int val;
};



Arg parse(const std::string &a) {
	const static std::regex arg_regex(R"(^\(?(#?)(\$?)(\w*)(,[xy])?(\)?)(,y)?)");
	std::smatch m;
	AdressingMode mode = BAD;
	int v = -1;
	if(std::regex_match(a, m, arg_regex)) {
		if(m[2] == "$")
			v = stol(m[3], nullptr, 16);
		else
			v = stol(m[3], nullptr, 10);

		if(m[5] == ")") {
			if(m[4] == ",x" && v < 256)
				mode = INDX;
			else if(m[6] == ",y" && v < 256)
				mode = INDY;
			else if(m[6] == "" && m[4] == "")
				mode = IND;
			else
				mode = BAD;	
		} else {
			if(m[3] == "a")
				mode = ACC;
			else if(m[1] == "#")
				mode = IMM;
			else if(m[4] == ",y")
				mode = v < 256 ? ZPY : ABSY;
			else if(m[4] == ",x")
				mode = v < 256 ? ZPX : ABSX;
			else
				mode = v < 256 ? ZP : ABS;
		}
	}

	return Arg(mode, v);
}

int assembleLine(const std::string &line, uint8_t *output, int pc) {
	std::regex line_regex(R"(^\s*(\w+)\s*(\S+)?$)");
	std::smatch matches;
	if(line == "") return 0;
	if(std::regex_match(line, matches, line_regex)) {
		Arg a;
		if(matches[2] != "") {
			a = parse(matches[2]);
			//printf("ARG is %s %x\n", modeNames[a.mode].c_str(), a.val);
		} else {
			a.mode = NONE;
		}

		for(const auto &ins : Machine<>::getInstructions()) {
			if(ins.name == matches[1]) {
				for(auto &op : ins.opcodes) {
					if(op.mode == ABSY && a.mode == ZPY) {
						a.mode = ABSY;
					}

					if(op.mode == ABSX && a.mode == ZPX) {
						a.mode = ABSX;
					}

					if(op.mode == ABS && a.mode == ZP) {
						// An opcode that requires Abs. We assume Zp versions are always
						// defined before Abs versions.
						a.mode = ABS;

					} else
					if(op.mode == REL && (a.mode == ABS || a.mode == ZP)) {
						printf("ABS %04x at PC %04x = REL %d", a.val, pc, a.val - pc - 2);
						a.val = (int)a.val - pc - 2;
						a.mode = REL;
					}
					if(op.mode == a.mode) {
						//printf("Matched %02x\n", op.code);
						auto saved = output;
						*output++ = op.code;
						if(opSize(a.mode) > 1)
							*output++ = a.val & 0xff;
						if(opSize(a.mode) > 2)
							*output++ = a.val >> 8;
						return output - saved;
					};
				}
			}
		}
	}
	return -1;
}

/// Assemble given code located at `pc` and write code to `output`.
/// Returns size in bytes.
/// (Only simple (MON like) assembly supported)
int assemble(int pc, uint8_t *output, const std::string &code) {
	int total = 0;
	for(const auto &line :  utils::split(code, "\n")) {
		int rc= assembleLine(line, output, pc);
		if(rc < 0)
			return -1;
		pc += rc;
		output += rc;
		total += rc;
	}
	return total;
}

} // namespace
