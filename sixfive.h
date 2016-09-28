#pragma once
#include <stdexcept>
#include <string>
#include <vector>

class run_exception : public std::exception {
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

struct Machine {
	uint8_t *mem;
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint16_t pc;
	uint8_t sr;
	uint8_t sp;
};

struct OpVariant {
	OpVariant() {}
	OpVariant(uint8_t code, uint8_t cycles, AdressingMode mode) : code(code), cycles(cycles), mode(mode) {}
	uint8_t code;
	int cycles;
	AdressingMode mode;
	void (*op)(Machine&, uint8_t*);
};

struct Instruction {
	Instruction(const std::string &name, std::vector<OpVariant> ov, void (*op)(Machine&, uint8_t*)) : name(name), opcodes(ov), op(op) {}
	const std::string name;
	std::vector<OpVariant> opcodes;
	void (*op)(Machine&, uint8_t*);
};

