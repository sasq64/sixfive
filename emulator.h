#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>

namespace sixfive {

using Word = uint32_t;

struct Machine
{
	void setPC(const int16_t &pc);

	Machine();
	~Machine();
    Machine(Machine && op) noexcept;
    Machine& operator=(Machine && op) noexcept;

	void writeRam(uint16_t org, const Word *data, int size);
	void writeRam(uint16_t org, const Word data);
	void readRam(uint16_t org, Word *data, int size);
	Word readRam(uint16_t org);

	uint8_t regA();	
	uint8_t regX();	
	uint8_t regY();	
	uint8_t regSR();	

	void init();
	uint32_t run(uint32_t cycles);
	uint32_t runDebug(uint32_t cycles);
	void set_break(uint16_t pc, std::function<void(Machine &m)> f);

	class Impl;
//private:
	std::unique_ptr<Impl> impl;
};

enum AdressingMode {BAD, NONE, ACC, SIZE2, IMM, REL, ZP, ZPX, ZPY, INDX, INDY, SIZE3, IND, ABS, ABSX, ABSY };

using OpFunc = void(*)(Machine::Impl&);

struct Opcode {
	Opcode() {}
	Opcode(int code, int cycles, AdressingMode mode, OpFunc op) : code(code), cycles(cycles), mode(mode), op(op) {}
	uint8_t code;
	int cycles;
	AdressingMode mode;
	OpFunc op;
};

struct Instruction {
	Instruction(const std::string &name, std::vector<Opcode> ov) : name(name), opcodes(ov) {}
	const std::string name;
	std::vector<Opcode> opcodes;
};

void checkCode();
void checkSpeed();

extern std::vector<Instruction> instructionTable;
} // namespace
