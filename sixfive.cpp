#include <array>
#include <coreutils/file.h>
#include <regex>

#include "instructions.h"
#include "sixfive.h"

struct Emulator
{
	Emulator() {
		m.x = m.y = m.a = 0;
		m.sr = 0xff;
		m.pc = 0;
		for(auto &i : instructions) {
			for(auto &op : i.opcodes) {
				opCodes[op.code] = op;
				opCodes[op.code].name = i.name.c_str();
				opCodes[op.code].op = i.op;
			}
		}
	}

	Machine m;
	Machine om;
	;
	std::array<OpVariant, 256> opCodes;

	void run(uint32_t cycles) {

		while(m.cycles < cycles) {

			uint8_t code = m.mem[m.pc++];
			if(code == 0x60 && m.sp == 0xff)
				return;

			auto opcode = opCodes[code];
			// printf("Opcode %02x = %s\n", code, opcode.name);

			uint8_t *ea = nullptr;
			int o;
			auto mode = opcode.mode;
			// printf("%04x : %02x %s %s\n", m.pc-1, code, opcode.name,
			// modeNames[mode].c_str());
			ea = m.mem;
			// ea = &m.mem[m.mem[m.pc] | m.mem[m.pc+1]<<8];

			switch(mode) {
			case NONE: break;
			case IMM:
			case REL: ea = &m.mem[m.pc]; break;
			case IND:
				// ea = &m.mem[m.mem[m.pc] | m.mem[m.pc+1]<<8];
				break;
			case ABS: ea = &m.mem[m.mem[m.pc] | m.mem[m.pc + 1] << 8]; break;
			case ABS_X:
				ea = &m.mem[m.mem[m.pc] | (m.mem[m.pc + 1] << 8) + m.x];
				break;
			case ABS_Y:
				ea = &m.mem[m.mem[m.pc] | (m.mem[m.pc + 1] << 8) + m.y];
				break;
			case IND_X:
				o = (m.mem[m.pc] + m.x) & 0xff;
				ea = &m.mem[m.mem[o] | (m.mem[o + 1] << 8)];
				break;
			case IND_Y:
				o = m.mem[m.pc];
				ea = &m.mem[m.mem[o] | (m.mem[o + 1] << 8) + m.y];
				break;
			case ZP_X: ea = &m.mem[(m.mem[m.pc] + m.x) & 0xff]; break;
			case ZP_Y: ea = &m.mem[(m.mem[m.pc] + m.y) & 0xff]; break;
			case ZP: ea = &m.mem[m.mem[m.pc]]; break;
			default:
				break;
				// printf("PC %04x, op %02x\n", m.pc, code);
				// throw new run_exception("Illegal opcode");
			}
			m.pc += (mode > SIZE3 ? 2 : (mode > SIZE2 ? 1 : 0));
			opcode.op(m, ea);
			m.cycles += opcode.cycles;
			// checkEffect();
		}
	}

	void checkEffect() {
		if(om.a != m.a)
			printf("A := %02x\n", m.a);
		if(om.x != m.x)
			printf("X := %02x\n", m.x);
		if(om.y != m.y)
			printf("Y := %02x\n", m.y);
		for(int i = 0; i < 65536; i++)
			if(m.mem[i] != om.mem[i]) {
				printf("%04x := %02x\n", i, m.mem[i]);
				om.mem[i] = m.mem[i];
			}
		if(om.sr != m.sr)
			printf("SR := %02x\n", m.sr);

		memcpy(&om.a, &m.a, 12);
	}
};

struct Assembler
{

	struct Arg
	{
		Arg(AdressingMode am = ILLEGAL, int val = -1) : mode(am), val(val) {}
		AdressingMode mode;
		int val;
	};

	Arg parse(const std::string &a) {
		const static std::regex arg_regex(
		    R"(^\(?(#?)(\$?)(\w*)(,[xy])?(\)?)(,y)?)");
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

	int assemble(const std::string &code, uint8_t *output, int pc) {
		int total = 0;
		for(const auto &line : utils::split(code, "\n")) {
			int rc = assembleLine(line, output, pc);
			if(rc < 0)
				throw run_exception("Unknown opcode " + line);
			printf("asm to %p %x = %d\n", output, pc, rc);
			pc += rc;
			output += rc;
			total += rc;
		}
		return total;
	}

	int assembleLine(const std::string &line, uint8_t *output, int pc) {
		std::regex line_regex(R"(^(\w+:?)?\s*((\w+)\s*(\S+)?)?\s*(;.*)?$)");
		std::smatch matches;
		if(line == "")
			return 0;
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
						if(op.mode == REL && a.mode == ABS) {
							int d = (int)a.val - pc - 2;
							if(d <= 0x7f && d >= -0x80) {
								a.val = d;
								a.mode = REL;
							}
						}
						if(op.mode == a.mode) {
							printf("Matched %02x\n", op.code);
							auto saved = output;
							*output++ = op.code;
							if(a.mode > SIZE2)
								*output++ = a.val & 0xff;
							if(a.mode > SIZE3)
								*output++ = a.val >> 8;
							return output - saved;
						};
					}
				}
			}
		}
		return -1;
	}
};

#define SPEED_TEST

#ifdef SPEED_TEST

#include <benchmark/benchmark.h>

static void Bench_emulate(benchmark::State &state) {

	Emulator emu;
	Assembler ass;
	ass.assemble(R"(
	lda $1000,x
	sta $2000,x
	inx
	bne $500
	beq $500
)",
	             &emu.m.mem[0x500], 0x500);
	while(state.KeepRunning()) {
		emu.m.cycles = 0;
		emu.m.pc = 0x500;
		emu.run(3500);
	}
};

BENCHMARK(Bench_emulate);

BENCHMARK_MAIN();

#else

int main(int argc, char **argv) {

	using utils::File;
	Assembler ass;
	Emulator e;
	int pc = 0x100;

	auto text = File(argv[1]).getLines();
	for(const auto &t : text) {
		puts(t.c_str());
		pc += ass.assembleLine(t, &e.m.mem[pc], pc);
	}

	printf("Running\n");
	e.m.pc = 0x100;
	e.om = e.m.clone();
	e.run(100);

	printf("\n0x00:");
	for(int i = 0; i < 0x10; i++)
		printf(" %02x", e.m.mem[i]);
	printf("\n0xc000:");
	for(int i = 0; i < 0x10; i++)
		printf(" %02x", e.m.mem[0xc000 + i]);
	printf("\n");

	return 0;
}

#endif
