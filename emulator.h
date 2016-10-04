#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

namespace sixfive {

using Word = uint8_t;
using SWord = int8_t;

struct Machine
{
	Word a;
	Word x;
	Word y;
	Word sr;
	Word sp;

	union {
		Word *pcp;
		uintptr_t pc;
	};

	Word lastWord;
	Word *stack;
	Word mem[65536];
	uint32_t cycles;
};

enum {BAD, NONE, ACC, SIZE2, IMM, REL, ZP, ZPX, ZPY, INDX, INDY, SIZE3, IND, ABS, ABSX, ABSY };
enum AdressingMode
{
	Illegal,
	None,
	Acc,

	Size2,

	Imm,
	Rel,

	Zp,
	Zp_x,
	Zp_y,
	Ind_x,
	Ind_y,

	Size3,

	Ind,
	Abs,
	Abs_x,
	Abs_y,
};

using OpFunc = void(*)(Machine&);

struct Opcode {
	Opcode() {}
	Opcode(Word code, int cycles, AdressingMode mode, OpFunc op) : code(code), cycles(cycles), mode(mode), op(op) {}
	Word code;
	int cycles;
	const char *name;
	AdressingMode mode;
	OpFunc op;
};

struct Instruction {
	Instruction(const std::string &name, std::vector<Opcode> ov) : name(name), opcodes(ov) {}
	const std::string name;
	std::vector<Opcode> opcodes;
};


class run_exception : public std::exception
{
public:
	run_exception(const std::string &m = "RUN Exception") : msg(m) {}
	virtual const char *what() const throw() { return msg.c_str(); }

private:
	std::string msg;
};

void init(Machine &m);
void run(Machine &m, uint32_t cycles);

extern std::vector<Instruction> instructionTable;
} // namespace
