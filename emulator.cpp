#include <vector>
#include <unordered_map>
#include <cstring>
#include <vector>

#include "emulator.h"

#include "zyan-disassembler-engine/Zydis/Zydis.hpp"

namespace sixfive {

constexpr bool AlignReads = true;
constexpr bool StatusOpt = false;

// Registers
enum REGNAME { NOREG, A, X, Y, SP };
// Adressing modes

// Condition codes
enum CONDCODE { EQ, NE, PL, MI, CC, CS, VC, VS };

template <typename POLICY> struct Machine<POLICY>::Impl {

	void init() {
		sp = 0xff;
		stack = &mem[0x100];
		memset(&mem[0], 0, sizeof(mem));
		cycles = 0;
		for(const auto &i : instructionTable) {
			for(const auto &o : i.opcodes) {
				jumpTable[o.code].op = o.op;
			}
		}

	}

	void set_break(uint16_t pc, std::function<void(Machine &m)> f) {
		breaks[pc] = f;
	}
	void writeRam(uint16_t org, const Word data) {
		mem[org] = data;
	}
	void writeRam(uint16_t org, const Word *data, int size) {
		memcpy(&mem[org], data, size * sizeof(Word));
	}
	void readRam(uint16_t org, Word *data, int size) {
		memcpy(data, &mem[org], size * sizeof(Word));
	}
	Word readRam(uint16_t org) {
		return mem[org];
	}
	uint8_t regA() { return a; }
	uint8_t regX() { return x; }	
	uint8_t regY() { return y; }	
	uint8_t regSR() { return sr; }	

	void setPC(const int16_t &p) {
		pc = p;
	}

	uint32_t run(uint32_t runc) {

		auto toCycles = cycles + runc;
		uint32_t opcodes = 0;
		while(cycles < toCycles) {

			uint8_t code = ReadPC();
			if(code == 0x60 && (uint8_t)sp == 0xff)
				return opcodes;
			auto &op = jumpTable[code];
			op.op(*this);
			cycles += op.cycles;
			opcodes++;
		}
		return opcodes;
	}

	Word a;
	Word x;
	Word y;
	Word sr;
	Word sp;

	uint16_t pc;

	Word lastWord;
	Word *stack;
	Word mem[65536];
	uint32_t cycles;
	std::unordered_map<uint16_t, std::function<void(Machine &m)>> breaks;
	Opcode<POLICY> jumpTable[256];

	static std::vector<Instruction<POLICY>> instructionTable;


inline Word& ReadPC(Machine<POLICY>::Impl&m) { return m.mem[m.pc++]; }
template <bool ALIGN = AlignReads> inline uint16_t ReadPCW(Machine<POLICY>::Impl &m) {
	m.pc += 2;
	if(AlignReads)
		return m.mem[m.pc - 2] | (m.mem[m.pc-1]<<8);
	else
		return *(uint16_t*)&m.mem[m.pc - 2];
}
//template <> inline uint16_t ReadPCW<true>(Machine<POLICY>::Impl &m) {  m.pc += 2; return m.mem[m.pc - 2] | (m.mem[m.pc-1]<<8); }
//template <> inline uint16_t ReadPCW<false>(Machine<POLICY>::Impl &m) { m.pc += 2; return *(uint16_t*)&m.mem[m.pc - 2]; }

inline uint16_t Read16(Machine<POLICY>::Impl &m, uint16_t offs) {
	return m.mem[offs] | (m.mem[offs+1]<<8);
}

template <int REG> uint8_t& Reg() {
	switch(REG) {
        case A: return a;
        case X: return x;
        case Y: return y;
	}
}

///
/// CONDITIONS AND THE STATUS REGISTER
///


   // S V - b d i Z C
   // 1 1 0 0 0 0 1 1

	const Word& get_SR() {
		return sr;
	}
	void set_SR(Word s) {
		sr = s;
	}
   	template <int REG> void set_SZ() {
		sr = (sr & 0x7d) | (Reg<REG>() & 0x80) | (!Reg<REG>() << 1);
	}

   void set_SZ(int res) {
		sr = (sr & 0x7d) | (res & 0x80) | (!res << 1);
	}

	void set_SZC(int res) {
		sr = (sr & 0x7c) | (res & 0x80) | (!(res & 0xff) << 1) | ((res>>8)&1);
	}

	void set_SZCV(int res, int arg) {
		sr = (sr & 0x3c) | (res & 0x80) | (!res << 1) | ((res>>8)&1) | ((~(a ^ arg) & (a ^ res) & 0x80)>>1);
	}

enum STATUSFLAGS { CARRY, ZERO, IRQ, DECIMAL, BRK, xXx, OVER, SIGN };

template <int FLAG, bool v> static void Set() {
	sr = (sr & ~(1<<FLAG)) | (v<<FLAG);
}

static constexpr bool SET = true;
static constexpr bool CLEAR = false;

template <int FLAG, bool v> bool Check() {
	return (sr & (1<<FLAG)) == v; 
}


//template <int COND, bool OPTSR = StatusOpt> bool check(Machine<POLICY>::Impl &m);

// COMMON
/*
template <> inline bool check<CC>(Machine<POLICY>::Impl &m) {
	return !(m.sr & 0x01);
}

template <> inline bool check<CS>(Machine<POLICY>::Impl &m) {
	return m.sr & 0x01;
}

template <> inline bool check<VC>(Machine<POLICY>::Impl &m) {
	return (m.sr & 0x40);
}

template <> inline bool check<VS>(Machine<POLICY>::Impl &m) {
	return m.sr & 0x40;
}

// NON OPT

template <> inline bool check<MI, false>(Machine<POLICY>::Impl &m) {
	return m.sr & 0x80;
}

template <> inline bool check<PL, false>(Machine<POLICY>::Impl &m) {
	return !(m.sr & 0x80);
}

template <> inline bool check<EQ, false>(Machine<POLICY>::Impl &m) {
	return m.sr & 0x2;
}

template <> inline bool check<NE, false>(Machine<POLICY>::Impl &m) {
	return !(m.sr & 0x2);
}

// OPT

template <> inline bool check<MI, true>(Machine<POLICY>::Impl &m) {
	return m.lastWord & 0x80;
}

template <> inline bool check<PL, true>(Machine<POLICY>::Impl &m) {
	return !(m.lastWord & 0x80);
}

template <> inline bool check<EQ, true>(Machine<POLICY>::Impl &m) {
	return m.lastWord == 0;
}

template <> inline bool check<NE, true>(Machine<POLICY>::Impl &m) {
	return m.lastWord != 0;
}
*/

//
//
// MEMORY ACCESS
//
//


static constexpr uint16_t IOMASK = 0;//0xff00;
static constexpr uint16_t IOBANK = 0xd000;
Word get_io(uint16_t addr) {
	return getchar();
};

void put_io(uint16_t addr, Word v) {
	putchar(v);
};


// FETCH ADDRESS

template <int MODE> uint16_t Address() {
	switch(MODE) {
		case ZP: return ReadPC();
		case ZPX: return ReadPC() + x;
		case ZPY: return ReadPC() + y;
		case ABS: return ReadPCW();
		case ABSX: return ReadPCW() + x;
		case ABSY: return ReadPCW() + y;
		case INDX: return Read16(ReadPC() + x);
		case INDY: return Read16(ReadPC()) + y;
	};
}

// WRITE MEMORY

inline void Write(uint16_t adr, Word v) {
	if((adr & IOMASK) == IOBANK)
		put_io(adr, v);
	else
		mem[adr] = v;
}

template <int MODE> inline void Write(Word v) {
	uint16_t adr = Address<MODE>();
	if((adr & IOMASK) == IOBANK)
		put_io(adr, v);
	else
		mem[adr] = v;
}

// READ MEMORY

Word Read(uint16_t adr) {
	if((adr & IOMASK) == IOBANK)
		return get_io(adr);
	else
		return mem[adr];
}

template <int MODE> Word Read() {
	if(MODE == IMM)
		return ReadPC();
	uint16_t adr = Address<MODE>();
	if((adr & IOMASK) == IOBANK)
		return get_io(adr);
	else
		return mem[adr];
}


///
///
///   OPCODES
///
///

template <int REG, int MODE> static void Store(Machine<POLICY>::Impl &m) {
	m.Write<MODE>(m.Reg<REG>());
}

template <int REG, int MODE> static void Load(Machine<POLICY>::Impl &m) {
	m.Reg<REG>() = m.Read<MODE>();
	m.set_SZ<REG>();
}

template <int FLAG, bool v> static void Branch(Machine<POLICY>::Impl &m) {
	int8_t diff = (m.mem[m.pc++] & 0xff);
	int d = m.Check<FLAG, v>();
	m.cycles += d;
	m.pc += (diff * d);
}

void Php(Machine<POLICY>::Impl &m) {
	m.stack[m.sp--] = m.get_SR();;
}

template<int MODE, int inc> static void Inc(Machine<POLICY>::Impl &m) {
	auto adr = m.Address<MODE>(m);
	auto rc = m.Read(adr) + inc;
	m.Write(adr, rc);
	m.set_SZ(rc);
}

// === COMPARE, ADD & SUBTRACT

template<int REG, int MODE> static void Bit(Machine<POLICY>::Impl &m) {
	Word z = m.Read<MODE>();
   	m.sr = (m.sr & 0x3e) | (z & 0xc0) | (!(z & m.a)<<1);
}

template<int REG, int MODE> static void Cmp(Machine<POLICY>::Impl &m) {
	Word z = ~m.Read<MODE>() & 0xff;
	int rc = m.Reg<REG>() + z + 1;
	m.set_SZC(rc);
}

template<int MODE> static void Sbc(Machine<POLICY>::Impl &m) {
	Word z = (~m.Read<MODE>()) & 0xff;
	int rc = m.a + z + (m.sr & 1);
	m.set_SZCV(rc, z);
	m.a = rc & 0xff;
}

template<int MODE> static void Adc(Machine<POLICY>::Impl &m) {
	auto z = m.Read<MODE>();

	int rc = m.a + z + (m.sr & 1);
	m.set_SZCV(rc, z);
	m.a = rc & 0xff;
}


template<int MODE> static void And(Machine<POLICY>::Impl &m) {
	m.a &= m.Read<MODE>();
	m.set_SZ<A>();
}

template<int MODE> static void Ora(Machine<POLICY>::Impl &m) {
	m.a |= m.Read<MODE>();
	m.set_SZ<A>();
}

template<int MODE> static void Eor(Machine<POLICY>::Impl &m) {
	m.a ^= m.Read<MODE>();
	m.set_SZ<A>();
}

// === SHIFTS & ROTATES

template<int MODE> static void Asl(Machine<POLICY>::Impl &m) {
	if(MODE == ACC) {
		int rc = m.a << 1;
		m.set_SZC(rc);
		m.a = rc & 0xff;
		return;
	}

	auto adr = m.Address<MODE>();
	int rc = m.Read(adr) << 1;
	m.set_SZC(rc);
	m.Write(adr, rc & 0xff);
}

template<int MODE> static void Lsr(Machine<POLICY>::Impl &m) {
	if(MODE == ACC) {
		m.sr = (m.sr & 0x7f) | ( m.a & 1);
		m.a >>= 1;
		m.set_SZ<A>();
		return;
	}
	auto adr = m.Address<MODE>();
	int rc = m.Read(adr);
	m.sr = (m.sr & 0x7f) | ( rc & 1);
	m.Write(adr, (rc >> 1) & 0xff);
	m.set_SZ(rc);
}

template<int MODE> static void Ror(Machine<POLICY>::Impl &m) {
	auto adr = m.Address<MODE>();
	int rc = m.Read(m, adr) | (m.sr << 8);
	m.Write(adr, (rc >> 1) & 0xff);
	m.sr = (m.sr & 0x7f) | ( rc & 1);
	m.set_SZ(rc);
}

static void RorA(Machine<POLICY>::Impl &m) {
	int rc  = ((m.sr<<8) | m.a) >> 1;
	m.sr = (m.sr & 0x7f) | ( m.a & 1);
	m.a = rc;
	m.set_SZ<A>();
}

template<int MODE> static void Rol(Machine<POLICY>::Impl &m) {
	auto adr = m.Address<MODE>();
	int rc = (m.Read(adr) << 1) | (m.sr & 1);
	m.Write(adr, rc & 0xff);
	m.set_SZC(rc);
}

static void RolA(Machine<POLICY>::Impl &m) {
	int rc  = (m.a << 1) | (m.sr & 1);
	m.set_SZC(rc);
	m.a = rc;
}

};

template <typename POLICY>
std::vector<Instruction<POLICY>> Machine<POLICY>::Impl::instructionTable  {

	{"nop", {{ 0xea, 2, NONE, [](Machine::Impl &) {} }} },

    {"lda", {
		{ 0xa9, 2, IMM, Load<A, IMM>},
		{ 0xa5, 2, ZP, Load<A, ZP>},
		{ 0xb5, 4, ZPX, Load<A, ZPX>},
		{ 0xad, 4, ABS, Load<A, ABS>},
		{ 0xbd, 4, ABSX, Load<A, ABSX>},
		{ 0xb9, 4, ABSY, Load<A, ABSY>},
		{ 0xa1, 6, INDX, Load<A, INDX>},
		{ 0xb1, 5, INDY, Load<A, INDY>},
	} },

    {"ldx", {
		{ 0xa2, 2, IMM, Load<X, IMM>},
		{ 0xa6, 3, ZP, Load<X, ZP>},
		{ 0xb6, 4, ZPY, Load<X, ZPY>},
		{ 0xae, 4, ABS, Load<X, ABS>},
		{ 0xbe, 4, ABSY, Load<X, ABSY>},
	} },

    {"ldy", {
		{ 0xa0, 2, IMM, Load<Y, IMM>},
		{ 0xa4, 3, ZP, Load<Y, ZP>},
		{ 0xb4, 4, ZPX, Load<Y, ZPX>},
		{ 0xac, 4, ABS, Load<Y, ABS>},
		{ 0xbc, 4, ABSX, Load<Y, ABSX>},
	} },

    {"sta", {
		{ 0x85, 3, ZP, Store<A, ZP>},
		{ 0x95, 4, ZPX, Store<A, ZPX>},
		{ 0x8d, 4, ABS, Store<A, ABS>},
		{ 0x9d, 4, ABSX, Store<A, ABSX>},
		{ 0x99, 4, ABSY, Store<A, ABSY>},
		{ 0x81, 6, INDX, Store<A, INDX>},
		{ 0x91, 5, INDY, Store<A, INDY>},
	} },

    {"stx", {
		{ 0x86, 3, ZP, Store<X, ZP>},
		{ 0x96, 4, ZPY, Store<X, ZPY>},
		{ 0x8e, 4, ABS, Store<X, ABS>},
	} },

    {"sty", {
		{ 0x84, 3, ZP, Store<Y, ZP>},
		{ 0x94, 4, ZPX, Store<Y, ZPX>},
		{ 0x8c, 4, ABS, Store<Y, ABS>},
	} },

    {"dec", {
		{ 0xc6, 5, ZP, Inc<ZP, -1>},
		{ 0xd6, 6, ZPX, Inc<ZPX, -1>},
		{ 0xce, 6, ABS, Inc<ABS, -1>},
		{ 0xde, 7, ABSX, Inc<ABSX, -1>},
	} },

    {"inc", {
		{ 0xe6, 5, ZP, Inc<ZP, 1>},
		{ 0xf6, 6, ZPX, Inc<ZPX, 1>},
		{ 0xee, 6, ABS, Inc<ABS, 1>},
		{ 0xfe, 7, ABSX, Inc<ABSX, 1>},
	} },

	{ "tax", { { 0xaa, 2, NONE, [](Machine::Impl& m) { m.x = m.a ; m.set_SZ<X>(m); } } } },
	{ "txa", { { 0x8a, 2, NONE, [](Machine::Impl& m) { m.a = m.x ; m.set_SZ<A>(m); } } } },
	{ "dex", { { 0xca, 2, NONE, [](Machine::Impl& m) { m.x-- ; m.set_SZ<X>(m); } } } },
	{ "inx", { { 0xe8, 2, NONE, [](Machine::Impl& m) { m.x++ ; m.set_SZ<X>(m); } } } },
	{ "tay", { { 0xa8, 2, NONE, [](Machine::Impl& m) { m.y = m.a ; m.set_SZ<Y>(m); } } } },
	{ "tya", { { 0x98, 2, NONE, [](Machine::Impl& m) { m.a = m.y ; m.set_SZ<A>(m); } } } },
	{ "dey", { { 0x88, 2, NONE, [](Machine::Impl& m) { m.y-- ; m.set_SZ<Y>(m); } } } },
	{ "iny", { { 0xc8, 2, NONE, [](Machine::Impl& m) { m.y++ ; m.set_SZ<Y>(m); } } } },

	{ "txs", { { 0x9a, 2, NONE, [](Machine::Impl& m) { m.sp = m.x; } } } },
	{ "tsx", { { 0xba, 2, NONE, [](Machine::Impl& m) { m.sp = m.x; } } } },

	{ "pha", { { 0x48, 3, NONE, [](Machine::Impl& m) {
		m.stack[m.sp--] = m.a;
	} } } },

	{ "pla", { { 0x68, 4, NONE, [](Machine::Impl& m) {
		m.a = m.stack[++m.sp];
	} } } },

	{ "php", { { 0x08, 3, NONE, [](Machine::Impl& m) {
		m.stack[m.sp--] = m.setSR(m, m.stack[++m.sp]);
	} } } },

	{ "bcc", { { 0x90, 2, REL, Branch<CARRY, CLEAR> }, } },
	{ "bcs", { { 0xb0, 2, REL, Branch<CARRY, SET> }, } },
	{ "bne", { { 0xd0, 2, REL, Branch<ZERO, CLEAR> }, } },
	{ "beq", { { 0xf0, 2, REL, Branch<ZERO, SET> }, } },
	{ "bpl", { { 0x10, 2, REL, Branch<SIGN, CLEAR> }, } },
	{ "bmi", { { 0x30, 2, REL, Branch<SIGN, SET> }, } },
	{ "bvc", { { 0x50, 2, REL, Branch<OVERFLOW, CLEAR> }, } },
	{ "bvs", { { 0x70, 2, REL, Branch<OVERFLOW, SET> }, } },

	{ "adc", {
		{ 0x69, 2, IMM, Adc<IMM>},
		{ 0x65, 3, ZP, Adc<ZP>},
		{ 0x75, 4, ZPX, Adc<ZPX>},
		{ 0x6d, 4, ABS, Adc<ABS>},
		{ 0x7d, 4, ABSX, Adc<ABSX>},
		{ 0x79, 4, ABSY, Adc<ABSY>},
		{ 0x61, 6, INDX, Adc<INDX>},
		{ 0x71, 5, INDY, Adc<INDY>},
	} },

	{ "sbc", {
		{ 0xe9, 2, IMM, Sbc<IMM>},
		{ 0xe5, 3, ZP, Sbc<ZP>},
		{ 0xf5, 4, ZPX, Sbc<ZPX>},
		{ 0xed, 4, ABS, Sbc<ABS>},
		{ 0xfd, 4, ABSX, Sbc<ABSX>},
		{ 0xf9, 4, ABSY, Sbc<ABSY>},
		{ 0xe1, 6, INDX, Sbc<INDX>},
		{ 0xf1, 5, INDY, Sbc<INDY>},
	} },

	{ "cmp", {
		{ 0xc9, 2, IMM, Cmp<A, IMM>},
		{ 0xc5, 3, ZP, Cmp<A, ZP>},
		{ 0xd5, 4, ZPX, Cmp<A, ZPX>},
		{ 0xcd, 4, ABS, Cmp<A, ABS>},
		{ 0xdd, 4, ABSX, Cmp<A, ABSX>},
		{ 0xd9, 4, ABSY, Cmp<A, ABSY>},
		{ 0xc1, 6, INDX, Cmp<A, INDX>},
		{ 0xd1, 5, INDY, Cmp<A, INDY>},
	} },

	{ "cpx", {
		{ 0xe0, 2, IMM, Cmp<X, IMM>},
		{ 0xe4, 3, ZP, Cmp<X, ZP>},
		{ 0xec, 4, ABS, Cmp<X, ABS>},
	} },

	{ "cpy", {
		{ 0xc0, 2, IMM, Cmp<Y, IMM>},
		{ 0xc4, 3, ZP, Cmp<Y, ZP>},
		{ 0xcc, 4, ABS, Cmp<Y, ABS>},
	} },

	{ "and", {
		{ 0x29, 2, IMM, And<IMM>},
		{ 0x25, 3, ZP, And<ZP>},
		{ 0x35, 4, ZPX, And<ZPX>},
		{ 0x2d, 4, ABS, And<ABS>},
		{ 0x3d, 4, ABSX, And<ABSX>},
		{ 0x39, 4, ABSY, And<ABSY>},
		{ 0x21, 6, INDX, And<INDX>},
		{ 0x31, 5, INDY, And<INDY>},
	} },

	{ "eor", {
		{ 0x49, 2, IMM, Eor<IMM>},
		{ 0x45, 3, ZP, Eor<ZP>},
		{ 0x55, 4, ZPX, Eor<ZPX>},
		{ 0x4d, 4, ABS, Eor<ABS>},
		{ 0x5d, 4, ABSX, Eor<ABSX>},
		{ 0x59, 4, ABSY, Eor<ABSY>},
		{ 0x41, 6, INDX, Eor<INDX>},
		{ 0x51, 5, INDY, Eor<INDY>},
	} },

	{ "ora", {
		{ 0x09, 2, IMM, Ora<IMM>},
		{ 0x05, 3, ZP, Ora<ZP>},
		{ 0x15, 4, ZPX, Ora<ZPX>},
		{ 0x0d, 4, ABS, Ora<ABS>},
		{ 0x1d, 4, ABSX, Ora<ABSX>},
		{ 0x19, 4, ABSY, Ora<ABSY>},
		{ 0x01, 6, INDX, Ora<INDX>},
		{ 0x11, 5, INDY, Ora<INDY>},
	} },

	{ "sec", { { 0x38, 2, NONE, Set<CARRY, true> } } },
	{ "clc", { { 0x18, 2, NONE, Set<CARRY, false> } } },
	{ "sei", { { 0x58, 2, NONE, Set<IRQ, true> } } },
	{ "cli", { { 0x78, 2, NONE, Set<IRQ, false> } } },
	{ "sed", { { 0xf8, 2, NONE, Set<DECIMAL, true> } } },
	{ "cld", { { 0xd8, 2, NONE, Set<DECIMAL, false> } } },
	{ "clv", { { 0xb8, 2, NONE, Set<OVER, false> } } },

	{ "lsr", { 
		{ 0x4a, 2, NONE, Lsr<ACC>},
		{ 0x4a, 2, ACC, Lsr<ACC>},
		{ 0x46, 5, ZP, Lsr<ZP>},
		{ 0x56, 6, ZPX, Lsr<ZPX>},
		{ 0x4e, 6, ABS, Lsr<ABS>},
		{ 0x5e, 7, ABSX, Lsr<ABSX>},
	} },

	{ "asl", { 
		{ 0x0a, 2, NONE, Asl<ACC>},
		{ 0x0a, 2, ACC, Asl<ACC>},
		{ 0x06, 5, ZP, Asl<ZP>},
		{ 0x16, 6, ZPX, Asl<ZPX>},
		{ 0x0e, 6, ABS, Asl<ABS>},
		{ 0x1e, 7, ABSX, Asl<ABSX>},
	} },

	{ "ror", { 
		{ 0x6a, 2, NONE, Ror<ACC>},
		{ 0x6a, 2, ACC, Ror<ACC>},
		{ 0x66, 5, ZP, Ror<ZP>},
		{ 0x76, 6, ZPX, Ror<ZPX>},
		{ 0x6e, 6, ABS, Ror<ABS>},
		{ 0x7e, 7, ABSX, Ror<ABSX>},
	} },

	{ "rol", { 
		{ 0x2a, 2, NONE, Rol<ACC>},
		{ 0x2a, 2, ACC, Rol<ACC>},
		{ 0x26, 5, ZP, Rol<ZP>},
		{ 0x36, 6, ZPX, Rol<ZPX>},
		{ 0x2e, 6, ABS, Rol<ABS>},
		{ 0x3e, 7, ABSX, Rol<ABSX>},
	} },

	{ "bit", {
		{ 0x24, 3, ZP, Bit<X, ZP>},
		{ 0x2c, 4, ABS, Bit<X, ABS>},
	} },

	{ "rts", {
		{ 0x60, 2, NONE, [](Machine::Impl &m) {  
			m.pc = (m.stack[(m.sp&0xff)+2] | (m.stack[(m.sp&0xff)+1]<<8))+1;
			m.sp += 2;
		}}
	} },

	{ "jmp", {
		{ 0x4c, 3, ABS, [](Machine::Impl &m) {
			m.pc = m.ReadPCW();
	   }}
   	} },

	{ "jsr", {
		{ 0x20, 3, ABS, [](Machine::Impl &m) {
			m.stack[m.sp-- & 0xff] = (m.pc+1) & 0xff;
			m.stack[m.sp-- & 0xff] = (m.pc+1) >> 8; 
			m.pc = m.ReadPCW();
	   }}
   	} },

};


///
///
///

struct Result {
	int calls;
	int opcodes;
	int jumps;
	bool tooLong;
};

void disasm(void *ptr, struct Result &r) {

	using namespace Zydis;

	MemoryInput input(ptr, 0x200);
    InstructionInfo info;
    InstructionDecoder decoder;
    decoder.setDisassemblerMode(DisassemblerMode::M32BIT);
    decoder.setDataSource(&input);
    decoder.setInstructionPointer((uint64_t)ptr);
    IntelInstructionFormatter formatter;
	r.tooLong = false;
	r.calls = r.opcodes = r.jumps = 0;
	while (decoder.decodeInstruction(info))
    {
        if (info.flags & IF_ERROR_MASK)
        {
        } 
        else
        {
			printf("    %s\n", formatter.formatInstruction(info));
        }

		if(info.mnemonic >= InstructionMnemonic::JA && info.mnemonic <= InstructionMnemonic::JS)
			r.jumps++;

		switch(info.mnemonic) {
		case InstructionMnemonic::RET:
			return;
		case InstructionMnemonic::CALL:
			r.calls++;
			break;
		default:
			break;
		}
		r.opcodes++;
    }
	r.tooLong = true;
	return;

}

template <typename POLICY> void checkCode() {

	Result r;
	int jumps = 0;
	int count = 0;
	int calls = 0;
	int opcodes = 0;

	for(const auto &i : Machine<POLICY>::Impl::instructionTable) {
		for(const auto &o : i.opcodes) {
			disasm((void*)o.op, r);
			printf("%s (%d/%d/%d)\n", i.name.c_str(), r.opcodes, r.calls, r.jumps);
			jumps += r.jumps;
			calls += r.calls;
			opcodes += r.opcodes;
			count++;
		}
	}
	printf("### AVG OPCODES: %d TOTAL CALLS/JUMPS: %d/%d\n", opcodes / count, calls, jumps);
}

template <typename POLICY> void checkEffect(typename Machine<POLICY>::Impl &m) {
	static typename Machine<POLICY>::Impl om;
	printf("[ ");
	if(om.pc != m.pc)
		printf("PC := %04x ", (unsigned)m.pc);
	if(om.a != m.a)
		printf("A := %02x ", m.a);
	if(om.x != m.x)
		printf("X := %02x ", m.x);
	if(om.y != m.y)
		printf("Y := %02x ", m.y);
	if(om.sr != m.sr)
		printf("SR := %02x ", m.sr);
	if(om.sp != m.sp)
		printf("SP := %02x ", m.sp);
	printf("]\n");
	for(int i=0; i<65536; i++)
		if(m.mem[i] != om.mem[i]) {
			printf("%04x := %02x ", i, m.mem[i] & 0xff);
			om.mem[i] = m.mem[i];
		}
	puts("");

	om.a = m.a;
	om.x = m.x;
	om.y = m.y;
	om.sp = m.sp;
	om.sr = m.sr;
	om.pc = m.pc;
	om.sp = m.sp; 
}
/*
uint32_t Machine::runDebug(uint32_t runc) {

	auto toCycles = impl->cycles + runc;
	uint32_t opcodes = 0;
	while(impl->cycles < toCycles) {

		checkEffect(*impl);

		uint8_t code = ReadPC(*impl);
		printf("code %02x\n", code);
		if(code == 0x60 && (uint8_t)impl->sp == 0xff)
			return opcodes;
		auto &op = jumpTable[code];
		op.op(*impl);
		impl->cycles += op.cycles;
		if(impl->breaks.count(impl->pc) == 1)
			impl->breaks[impl->pc](*this);
		opcodes++;
	}
	return opcodes;
}
*/
} // namespace

#include <benchmark/benchmark.h>

static void Bench_sort(benchmark::State &state) {

	static const uint8_t sortCode[] =
	{
		0xa0, 0x00, 0x84, 0x32, 0xb1, 0x30, 0xaa, 0xc8,
		0xca, 0xb1, 0x30, 0xc8, 0xd1, 0x30, 0x90, 0x10,
		0xf0, 0x0e, 0x48, 0xb1, 0x30, 0x88, 0x91, 0x30,
		0x68, 0xc8, 0x91, 0x30, 0xa9, 0xff, 0x85, 0x32,
		0xca, 0xd0, 0xe6, 0x24, 0x32, 0x30, 0xd9, 0x60
	};

	static const uint8_t data[] = {
		0,
		19,73,2,54,97,21,45,66,13,139,56,220,50,30,20,67,111,109,175,4,66,100,
		19,73,2,54,97,21,45,66,13,139,56,220,50,30,20,67,111,109,175,4,66,100,
		19,73,2,54,97,21,45,66,13,139,56,220,50,30,20,67,111,109,175,4,66,100,
		19,73,2,54,97,21,45,66,13,139,56,220,50,30,20,67,111,109,175,4,66,100,
		19,73,2,54,97,21,45,66,13,139,56,220,50,30,20,67,111,109,175,4,66,100,
	};


	sixfive::Machine<> m;
	for(int i=0; i<(int)sizeof(data); i++)
		m.writeRam(0x2000 + i, data[i]);
	for(int i=0; i<(int)sizeof(sortCode); i++)
		m.writeRam(0x1000 + i, sortCode[i]);
	m.writeRam(0x30, 0x00);
	m.writeRam(0x31, 0x20);
	m.writeRam(0x2000, sizeof(data)-1);
	m.setPC(0x1000);
	printf("Opcodes %d\n", m.run(5000));
	while(state.KeepRunning())
	{
		m.setPC(0x1000);
		m.run(5000);
	}

}
BENCHMARK(Bench_sort);

static void Bench_emulate(benchmark::State &state) {

	static const unsigned char WEEK[] =
	{
		0xa0, 0x74, 0xa2, 0x0a, 0xa9, 0x07, 0x20, 0x0a,
		0x10, 0x60, 0xe0, 0x03, 0xb0, 0x01, 0x88, 0x49,
		0x7f, 0xc0, 0xc8, 0x7d, 0x2a, 0x10, 0x85, 0x06,
		0x98, 0x20, 0x26, 0x10, 0xe5, 0x06, 0x85, 0x06,
		0x98, 0x4a, 0x4a, 0x18, 0x65, 0x06, 0x69, 0x07,
		0x90, 0xfc, 0x60, 0x01, 0x05, 0x06, 0x03, 0x01,
		0x05, 0x03, 0x00, 0x04, 0x02, 0x06, 0x04
	};

	sixfive::Machine<> m;
	for(int i=0; i<(int)sizeof(WEEK); i++)
		m.writeRam(0x1000 + i, WEEK[i]);
	m.setPC(0x1000);
	printf("Opcodes %d\n", m.run(5000));
	while(state.KeepRunning())
	{
		m.setPC(0x1000);
		m.run(5000);
	}
};

BENCHMARK(Bench_emulate);

static void Bench_allops(benchmark::State &state) {
	sixfive::Machine<> m;
	m.setPC(0x1000);
	while(state.KeepRunning()) {
		m.setPC(0x1000);
		for(const auto &i : sixfive::Machine<>::Impl::instructionTable) {
			for(const auto &o : i.opcodes) {
				o.op(*m.impl);
			}
		}
	}

}
BENCHMARK(Bench_allops);
