#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>

namespace sixfive {

using Word = uint32_t;
enum AdressingMode {BAD, NONE, ACC, SIZE2, IMM, REL, ZP, ZPX, ZPY, INDX, INDY, SIZE3, IND, ABS, ABSX, ABSY };


struct NOPOLICY;

template <typename POLICY = NOPOLICY> struct Machine
{
	void setPC(const int16_t &pc);

	Machine() {
		//impl = std::make_unique<Impl>();
		init();
	}
	~Machine() = default;
    Machine(Machine && op) noexcept = default;
    Machine& operator=(Machine && op) noexcept = default;


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

template <typename POLICY> using OpFunc = void(*)(typename Machine<POLICY>::Impl&);

template <typename POLICY> struct Opcode {
	Opcode() {}
	Opcode(int code, int cycles, AdressingMode mode, OpFunc<POLICY> op) : code(code), cycles(cycles), mode(mode), op(op) {}
	uint8_t code;
	int cycles;
	AdressingMode mode;
	OpFunc<POLICY> op;
};

template <typename POLICY> struct Instruction {
	Instruction(const std::string &name, std::vector<Opcode<POLICY>> ov) : name(name), opcodes(ov) {}
	const std::string name;
	std::vector<Opcode<POLICY>> opcodes;
};


void checkCode();
void checkSpeed();

} // namespace
