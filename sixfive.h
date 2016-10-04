#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

class run_exception : public std::exception
{
public:
	run_exception(const std::string &m = "RUN Exception") : msg(m) {}
	virtual const char *what() const throw() { return msg.c_str(); }

private:
	std::string msg;
};

/*
 * Assembly line format:
 * [label:] opcode arg,arg
 */

enum AdressingMode {
	ILLEGAL,
	NONE,
	ACC,

	SIZE2,

	IMM,
	REL,
// Zero page
	ZP,
	ZP_X,
	ZP_Y,
	IND_X,
	IND_Y,

	SIZE3,

	IND,
	ABS,
	ABS_X,
	ABS_Y,
};

const static std::string modeNames[] = { 
	"ILLEGAL",
	"NONE",
	"ACC",

	"SIZE2",

	"#IMM",
	"REL",
// Zero page
	"$ZP",
	"$ZP,X",
	"$ZP,Y",
	"$(ZP,X)",
	"$(ZP),Y",

	"SIZE3",

	"($IND)",
	"$ABS",
	"$ABS,X",
	"$ABS,Y",
};

enum SR {
	SR_C = 0x1,
	SR_Z = 0x2,
	SR_I = 0x4,
	SR_D = 0x8,
	SR_B = 0x10,
	SR_x5 = 0x20,
	SR_V = 0x40,
	SR_S = 0x80
};

//constexpr inline int mask(int l) { return 1<<l; }

struct Machine
{
	uint8_t *mem;
	uint8_t *stack;
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint8_t sr;
	uint16_t pc;
	uint8_t sp;
	uint8_t irq;
	uint32_t cycles;
	uint32_t memsize;

	Machine() {
		memset(this, 0, sizeof(Machine));
		memsize = 65536;
		stack = new uint8_t [256];
		mem = new uint8_t [memsize];
	}

	Machine clone() {
		Machine m2;
		memcpy(&m2, this, sizeof(Machine));
		m2.mem = new uint8_t [memsize];
		m2.stack = new uint8_t [256];
		memcpy(m2.mem, mem, memsize);
		memcpy(m2.stack, stack, 256);
		return m2;
	}

};

struct OpVariant {
	OpVariant() {}
	OpVariant(uint8_t code, uint8_t cycles, AdressingMode mode) : code(code), cycles(cycles), mode(mode) {}
	uint8_t code;
	int cycles;
	const char *name;
	AdressingMode mode;
	void (*op)(Machine&, uint8_t*);
};

struct Instruction {
	Instruction(const std::string &name, std::vector<OpVariant> ov, void (*op)(Machine&, uint8_t*)) : name(name), opcodes(ov), op(op) {}
	const std::string name;
	std::vector<OpVariant> opcodes;
	void (*op)(Machine&, uint8_t*);
};
