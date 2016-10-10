#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>

namespace sixfive {

using Word = uint8_t;
using SWord = int8_t;

struct Machine
{
	void setPC(const int16_t &pc);

	Machine();
	~Machine();
    Machine(Machine && op) noexcept;
    Machine& operator=(Machine && op) noexcept;

	void writeRam(uint16_t org, const uint8_t *data, int size);
	void init();
	void run(uint32_t cycles);
	void set_break(uint16_t pc, std::function<void(Machine &m)> f);

	class Impl;
private:
	std::unique_ptr<Impl> impl;
};

enum AdressingMode {BAD, NONE, ACC, SIZE2, IMM, REL, ZP, ZPX, ZPY, INDX, INDY, SIZE3, IND, ABS, ABSX, ABSY };

using OpFunc = void(*)(Machine::Impl&);

struct Opcode {
	Opcode() {}
	Opcode(Word code, int cycles, AdressingMode mode, OpFunc op) : code(code), cycles(cycles), mode(mode), op(op) {}
	Word code;
	int cycles;
//	const char *name;
	AdressingMode mode;
	OpFunc op;
};

struct Instruction {
	Instruction(const std::string &name, std::vector<Opcode> ov) : name(name), opcodes(ov) {}
	const std::string name;
	std::vector<Opcode> opcodes;
};


extern std::vector<Instruction> instructionTable;
} // namespace
