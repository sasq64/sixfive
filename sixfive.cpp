#include <coreutils/file.h>
#include <regex>
#include <array>

#include "sixfive.h"
#include "instructions.h"

struct Emulator
{
	Emulator() {
		m.x = m.y = m.a = 0;
		m.sr = 0xff;
		m.pc = 0;
		for(auto &i : instructions) {
			for(auto &op : i.opcodes) {
				opCodes[op.code] = op;
				opCodes[op.code].op = i.op;
			}
		}
	}


	Machine m;
	std::array<OpVariant,256> opCodes;

	void run(int cycles) {

		while(cycles > 0) {
			printf("PC %04x\n", m.pc);

			uint8_t code = m.mem[m.pc++];
			if(code == 0x60)
				return;

			auto opcode = opCodes[code];

			uint8_t *ea = nullptr;
			int o;
			auto mode = opcode.mode;
			switch(mode) {
			case NONE:
				break;
			case IMM:
				ea = &m.mem[m.pc];
				break;
			case ABS:
				ea = &m.mem[m.mem[m.pc] | m.mem[m.pc+1]<<8];
				break;
			case ABS_X:
				ea = &m.mem[m.mem[m.pc] | (m.mem[m.pc+1]<<8) + m.x];
				break;
			case ABS_Y:
				ea = &m.mem[m.mem[m.pc] | (m.mem[m.pc+1]<<8) + m.y];
				break;
			case IND_X:
				o = (m.mem[m.pc] + m.x) & 0xff;
				ea = &m.mem[ m.mem[o] | (m.mem[o+1]<<8) ];
				break;
			case IND_Y:
				o = m.mem[m.pc];
				ea = &m.mem[ m.mem[o] | (m.mem[o+1]<<8) + m.y];
				break;
			case ZP_X:
				ea = &m.mem[(m.mem[m.pc] + m.x) & 0xff];
				break;
			case ZP_Y:
				ea = &m.mem[(m.mem[m.pc] + m.y) & 0xff];
				break;
			case ZP:
				ea = &m.mem[ m.mem[m.pc] ];
				break;
			default:
				printf("PC %04x, op %02x\n", m.pc, code);
				throw new run_exception("Illegal opcode");
			}
			m.pc += (mode > SIZE3 ? 2 : (mode > SIZE2 ? 1 : 0));
			opcode.op(m, ea);
			cycles += opcode.cycles;
		}
	}

};


struct Arg {
	Arg(AdressingMode am = ILLEGAL, int val = -1) : mode(am), val(val) {}
	AdressingMode mode;
	int val;
};

Arg parse(const std::string &a) {
	const static std::regex arg_regex(R"(^\(?(#?)(\$?)(\w*)(,[xy])?(\)?)(,y)?)");
	std::smatch m;
	AdressingMode mode = ILLEGAL;
	int v = -1;
	if(std::regex_match(a, m, arg_regex)) {
		if(m[2] == "$")
			v = stol(m[3], nullptr, 16);
		else
			v = stol(m[3], nullptr, 10);

		if(m[5] == ")") {
			if(m[4] == ",x" && v < 256)
				mode = IND_X;
			else if(m[6] == ",y" && v < 256)
				mode = IND_Y;
			else if(m[6] == "" && m[4] == "")
				mode = IND;
			else
				mode = ILLEGAL;	
		} else {
			if(m[3] == "a")
				mode = ACC;
			else if(m[1] == "#")
				mode = IMM;
			else if(m[4] == ",y")
				mode = v < 256 ? ZP_Y : ABS_Y;
			else if(m[4] == ",x")
				mode = v < 256 ? ZP_X : ABS_X;
			else
				mode = v < 256 ? ZP : ABS;
		}
	}

	return Arg(mode, v);
}

struct Assembler {
	void assemble(const std::string &line, uint8_t* &output, int pc = -1)
	{
		std::regex line_regex(R"(^(\w+:?)?\s*((\w+)\s*(\S+)?)?\s*(;.*)?$)");
		std::smatch matches;
		if(std::regex_match(line, matches, line_regex)) {
			Arg a;
			if(matches[4] != "") {
				a = parse(matches[4]);
				printf("ARG is %s %x\n", modeNames[a.mode].c_str(), a.val);
			} else {
				a.mode = NONE;
			}

			for(auto &ins : instructions) {
				if(ins.name == matches[3]) {
					for(auto &op : ins.opcodes) {
						if(op.mode == a.mode) {
							printf("Matched %02x\n", op.code);
							*output++ = op.code;
							if(a.mode > SIZE2)
							   *output++ = a.val & 0xff;
							if(a.mode > SIZE3)
								*output++ = a.val >> 8;	
						};
					}
				}
			}
		}
	}

};




int main(int argc, char **argv) {


	using utils::File; 
	Assembler ass;
	Emulator e;
	e.m.mem = new uint8_t [65536];
	uint8_t *output = &e.m.mem[0x100];

	auto text = File(argv[1]).getLines(); 
	for(const auto &t : text) {
		puts(t.c_str());
		ass.assemble(t, output);
	}

	printf("Running\n");
	e.m.pc = 0x100;
	e.run(100);

	printf("\n0x00:");
	for(int i=0; i<0x10; i++)
		printf(" %02x", e.m.mem[i]);
	printf("\n0xc000:");
	for(int i=0; i<0x10; i++)
		printf(" %02x", e.m.mem[0xc000 + i]);
	printf("\n");

	return 0;
}
