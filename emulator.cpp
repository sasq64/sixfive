#include <vector>
#include <unordered_map>

#include "emulator.h"

namespace sixfive {

// Registers
enum REGNAME { NOREG, A, X, Y, SP };
// Adressing modes

// Condition codes
enum CONDCODE { EQ, NE, PL, MI, CC, CS, VC, VS };

constexpr bool AlignReads = false;
constexpr bool StatusOpt = false;

inline Word& ReadPC(Machine&m) { return m.mem[m.pc++]; }
template <bool ALIGN = AlignReads> inline uint16_t ReadPCW(Machine &m);
template <> inline uint16_t ReadPCW<true>(Machine &m) {  m.pc += 2; return m.mem[m.pc - 2] | (m.mem[m.pc-1]<<8); }
template <> inline uint16_t ReadPCW<false>(Machine &m) { m.pc += 2; return *(uint16_t*)&m.mem[m.pc - 2]; }

inline uint16_t Read16(Machine &m, uint16_t offs) {
	return m.mem[offs] | (m.mem[offs+1]<<8);
}

template <int REG> inline Word &Reg(Machine &);
template <> inline Word &Reg<A>(Machine &m) { return m.a; }
template <> inline Word &Reg<X>(Machine &m) { return m.x; }
template <> inline Word &Reg<Y>(Machine &m) { return m.y; }

///
/// CONDITIONS AND THE STATUS REGISTER
///

template <bool OPTSR = StatusOpt> struct flags {
   	//static void call(Machine &m);
};


template <> struct flags<false>
{
	inline static const Word& get(Machine &m) {
		return m.sr;
	}
   // S V - b d i Z C
   // 1 1 0 0 0 0 1 1

   template <int REG> inline static void set_SZ(Machine &m) {
		m.sr = (m.sr & 0x7d) | (Reg<REG>(m) & 0x80) | (!Reg<REG>(m) << 1);
	}

   template inline static void set_SZ(Machine &m, int res) {
		m.sr = (m.sr & 0x7d) | (res & 0x80) | (!res << 1);
	}

	inline static void set_SZC(Machine &m, int res) {
		printf("RES %x SR %x\n", res, m.sr);
		m.sr = (m.sr & 0x7c) | (res & 0x80) | (!(res & 0xff) << 1) | ((res>>8)&1);
		printf("RES %x SR %x\n", res, m.sr);
	}

	template inline static void set_SZCV(Machine &m, int res, int arg) {
		m.sr = (m.sr & 0x3c) | (res & 0x80) | (!res << 1) | ((res>>8)&1) | ((~(m.a ^ arg) & (m.a ^ res) & 0x80)>>1);
	}
};

template <> struct flags<true>
{
	template <int REG> inline static void set_SZ(Machine &m) {
		m.lastWord = Reg<REG>(m);
	}

	inline static void set_SZC(Machine &m, int res)
	{
		m.lastWord = res & 0xff;
		m.sr = (m.sr & 0x7c) | (res>>8);
	}

	template <int REG> inline static void set_SZCV(Machine &m, int res, int arg) {
		m.lastWord = Reg<REG>(m);
		m.sr = (m.sr & 0x3c) | (res>>8) | ((~(m.a ^ arg) & (m.a ^ res) & 0x80)>>1);
	}

	inline static Word get(Machine &m) {
		return m.sr | (m.lastWord & 0x80) | (!m.lastWord << 1);
	}
};

enum STATUSFLAGS { CARRY, ZERO, IRQ, DECIMAL, BRK, xXx, OVER, SIGN };

template <int FLAG, bool v> void Set(Machine &m) {
	m.sr = (m.sr & ~(1<<FLAG)) | (v<<FLAG);
}


template <int COND, bool OPTSR = StatusOpt> bool check(Machine &m);

// COMMON

template <> inline bool check<CC>(Machine &m) {
	return !(m.sr & 0x01);
}

template <> inline bool check<CS>(Machine &m) {
	return m.sr & 0x01;
}

template <> inline bool check<VC>(Machine &m) {
	return (m.sr & 0x40);
}

template <> inline bool check<VS>(Machine &m) {
	return m.sr & 0x40;
}

// NON OPT

template <> inline bool check<MI, false>(Machine &m) {
	return m.sr & 0x80;
}

template <> inline bool check<PL, false>(Machine &m) {
	return !(m.sr & 0x80);
}

template <> inline bool check<EQ, false>(Machine &m) {
	return m.sr & 0x2;
}

template <> inline bool check<NE, false>(Machine &m) {
	return !(m.sr & 0x2);
}

// OPT

template <> inline bool check<MI, true>(Machine &m) {
	return m.lastWord & 0x80;
}

template <> inline bool check<PL, true>(Machine &m) {
	return !(m.lastWord & 0x80);
}

template <> inline bool check<EQ, true>(Machine &m) {
	return m.lastWord == 0;
}

template <> inline bool check<NE, true>(Machine &m) {
	return m.lastWord != 0;
}

//
//
// MEMORY ACCESS
//
//


static constexpr uint16_t IOMASK = 0xff00;
static constexpr uint16_t IOBANK = 0xd000;
Word get_io(Machine &m, uint16_t addr) {
	return getchar();
};

void put_io(Machine &m, uint16_t addr, uint8_t v) {
	putchar(v);
};


// FETCH ADDRESS

template <int MODE> uint16_t Address(Machine &m);

template <> inline uint16_t Address<ZP>(Machine &m) {
	return ReadPC(m);
}

template <> inline uint16_t Address<ZPX>(Machine &m) {
	return ReadPC(m) + m.x;
}

template <> inline uint16_t Address<ZPY>(Machine &m) {
	return ReadPC(m) + m.y;
}

template <> inline uint16_t Address<ABS>(Machine &m) {
	return ReadPCW(m);
}

template <> inline uint16_t Address<ABSX>(Machine &m) {
	return ReadPCW(m) + m.x;
}

template <> inline uint16_t Address<ABSY>(Machine &m) {
	return ReadPCW(m) + m.y;
}

template <> inline uint16_t Address<INDX>(Machine &m) {
	return Read16(m, ReadPC(m) + m.x);
}

template <> inline uint16_t Address<INDY>(Machine &m) {
	return Read16(m, ReadPC(m)) + m.y;
}

// WRITE MEMORY

template<bool ALIGN = AlignReads> inline void Write(Machine &m, uint16_t adr, Word v) {
	if((adr & IOMASK) == IOBANK)
		put_io(m, adr, v);
	else
		m.mem[adr] = v;
}

template <int MODE, bool ALIGN = AlignReads> inline void Write(Machine &m, Word v) {
	uint16_t adr = Address<MODE>(m);
	if((adr & IOMASK) == IOBANK)
		put_io(m, adr, v);
	else
		m.mem[adr] = v;
}

// READ MEMORY

template <bool ALIGN = AlignReads> Word Read(Machine &m, uint16_t adr) {
	if((adr & IOMASK) == IOBANK)
		return get_io(m, adr);
	else
		return m.mem[adr];
}

template <int MODE, bool ALIGN = AlignReads> Word Read(Machine &m) {
	uint16_t adr = Address<MODE>(m);
	if((adr & IOMASK) == IOBANK)
		return get_io(m, adr);
	else
		return m.mem[adr];
}

template <> inline  Word Read<IMM>(Machine &m) {
	return ReadPC(m);
}

///
///
///   OPCODES
///
///

template <int REG, int MODE> void Store(Machine &m) {
	Write<MODE>(m, Reg<REG>(m));
}

template <int REG, int MODE> void Load(Machine &m) {
	Reg<REG>(m) = Read<MODE>(m);
	flags<>::set_SZ<REG>(m);
}

template <int COND> void Branch(Machine &m) {
	auto diff = ((int8_t*)m.mem)[m.pc++];
	int d = check<COND>(m);
	m.cycles += d;
	m.pc += (diff * d);
}

void Php(Machine &m) {
	m.stack[m.sp--] = flags<>::get(m);
}

template<int MODE, int inc> void Inc(Machine &m) {
	auto adr = Address<MODE>(m);
	auto rc = Read(m, adr) + inc;
	Write(m, adr, rc);
	flags<>::set_SZ(m, rc);
}

// === COMPARE, ADD & SUBTRACT

template<int REG, int MODE> void Cmp(Machine &m) {
	Word z = ~Read<MODE>(m);
	int rc = Reg<REG>(m) + z + 1;
	flags<>::set_SZC(m, rc);
}

template<int MODE> void Sbc(Machine &m) {
	Word z = ~(Read<MODE>(m));
	int rc = m.a + z + (m.sr & 1);
	flags<>::set_SZCV(m, rc, z);
	m.a = rc & 0xff;
}

template<int MODE> void Adc(Machine &m) {
	auto z = Read<MODE>(m);

	int rc = m.a + z + (m.sr & 1);
	flags<>::set_SZCV(m, rc, z);
	m.a = rc & 0xff;
}


template<int MODE> void And(Machine &m) {
	m.a &= Read<MODE>(m);
	flags<>::set_SZ<A>(m);
}

template<int MODE> void Ora(Machine &m) {
	m.a |= Read<MODE>(m);
	flags<>::set_SZ<A>(m);
}

template<int MODE> void Eor(Machine &m) {
	m.a ^= Read<MODE>(m);
	flags<>::set_SZ<A>(m);
}

// === SHIFTS & ROTATES

template<int MODE> void Asl(Machine &m) {
	auto adr = Address<MODE>(m);
	int rc = Read(m, adr) << 1;
	flags<>::set_SZC(m, rc);
	Write(m, adr, rc & 0xff);
}

template<> void Asl<ACC>(Machine &m) {
	int rc = m.a << 1;
	flags<>::set_SZC(m, rc);
	m.a = rc & 0xff;
}

template<int MODE> void Lsr(Machine &m) {
	auto adr = Address<MODE>(m);
	int rc = Read(m, adr);
	m.sr = (m.sr & 0x7f) | ( rc & 1);
	Write(m, adr, (rc >> 1) & 0xff);
	flags<>::set_SZ(m, rc);
}

template<> void Lsr<ACC>(Machine &m) {
	m.sr = (m.sr & 0x7f) | ( m.a & 1);
	m.a >>= 1;
	flags<>::set_SZ<A>(m);
}

template<int MODE> void Ror(Machine &m) {
	int rc = Read<MODE>(m) | (m.sr << 8);
	m.sr = (m.sr & 0x7f) | ( rc & 1);
	Read<MODE>(m) = (rc >> 1) & 0xff;
	flags<>::set_SZ(m, rc);
}

template<int MODE> void Rol(Machine &m) {
	int rc = (Read<MODE>(m) << 1) | (m.sr & 1);
	flags<>::set_SZC(m, rc);
	Read<MODE>(m) = rc & 0xff;
}

std::vector<Instruction> instructionTable  {

	{"nop", {{ 0xea, 2, None, [](Machine &m) {} }} },

    {"lda", {
		{ 0xa9, 2, Imm, Load<A, IMM>},
		{ 0xa5, 2, Zp, Load<A, ZP>},
		{ 0xb5, 4, Zp_x, Load<A, ZPX>},
		{ 0xad, 4, Abs, Load<A, ABS>},
		{ 0xbd, 4, Abs_x, Load<A, ABSX>},
		{ 0xb9, 4, Abs_y, Load<A, ABSY>},
		{ 0xa1, 6, Ind_x, Load<A, INDX>},
		{ 0xb1, 5, Ind_y, Load<A, INDY>},
	} },

    {"ldx", {
		{ 0xb2, 2, Imm, Load<X, IMM>},
		{ 0xbe, 4, Abs, Load<X, ABS>},
		{ 0xbe, 4, Abs_y, Load<X, ABSY>},
		{ 0xa6, 2, Zp, Load<X, ZP>},
		{ 0xbd, 4, Zp_y, Load<X, ZPY>},
	} },

    {"ldy", {
		{ 0xa2, 2, Imm, Load<Y, IMM>},
		{ 0xae, 4, Abs, Load<Y, ABS>},
		{ 0xbe, 4, Abs_y, Load<Y, ABSY>},
		{ 0xa6, 2, Zp, Load<Y, ZP>},
		{ 0xbd, 4, Zp_y, Load<Y, ZPY>},
	} },

    {"sta", {
		{ 0x85, 3, Zp, Store<A, ZP>},
		{ 0x95, 4, Zp_x, Store<A, ZPX>},
		{ 0x8d, 4, Abs, Store<A, ABS>},
		{ 0x9d, 4, Abs_x, Store<A, ABSX>},
		{ 0x99, 4, Abs_y, Store<A, ABSY>},
		{ 0x81, 6, Ind_x, Store<A, INDX>},
		{ 0x91, 5, Ind_y, Store<A, INDY>},
	} },

    {"stx", {
		{ 0x86, 3, Zp, Store<X, ZP>},
		{ 0x96, 4, Zp_y, Store<X, ZPY>},
		{ 0x8e, 4, Abs, Store<X, ABS>},
	} },

    {"sty", {
		{ 0x84, 3, Zp, Store<Y, ZP>},
		{ 0x94, 4, Zp_x, Store<Y, ZPX>},
		{ 0x8c, 4, Abs, Store<Y, ABS>},
	} },

    {"dec", {
		{ 0xc6, 5, Zp, Inc<ZP, -1>},
		{ 0xd6, 6, Zp_x, Inc<ZPX, -1>},
		{ 0xce, 6, Abs, Inc<ABS, -1>},
		{ 0xde, 7, Abs_x, Inc<ABSX, -1>},
	} },

    {"inc", {
		{ 0xe6, 5, Zp, Inc<ZP, 1>},
		{ 0xf6, 6, Zp_x, Inc<ZPX, 1>},
		{ 0xee, 6, Abs, Inc<ABS, 1>},
		{ 0xfe, 7, Abs_x, Inc<ABSX, 1>},
	} },

	{ "tax", { { 0xaa, 2, None, [](Machine& m) { m.x = m.a ; flags<>::set_SZ<X>(m); } } } },
	{ "txa", { { 0x8a, 2, None, [](Machine& m) { m.a = m.x ; flags<>::set_SZ<A>(m); } } } },
	{ "dex", { { 0xca, 2, None, [](Machine& m) { m.x-- ; flags<>::set_SZ<X>(m); } } } },
	{ "inx", { { 0xe8, 2, None, [](Machine& m) { m.x++ ; flags<>::set_SZ<X>(m); } } } },
	{ "tay", { { 0xa8, 2, None, [](Machine& m) { m.y = m.a ; flags<>::set_SZ<Y>(m); } } } },
	{ "tya", { { 0x98, 2, None, [](Machine& m) { m.a = m.y ; flags<>::set_SZ<A>(m); } } } },
	{ "dey", { { 0x88, 2, None, [](Machine& m) { m.y-- ; flags<>::set_SZ<Y>(m); } } } },
	{ "iny", { { 0xc8, 2, None, [](Machine& m) { m.y++ ; flags<>::set_SZ<Y>(m); } } } },

	{ "bcc", { { 0x90, 2, Rel, Branch<CC> }, } },
	{ "bcs", { { 0xb0, 2, Rel, Branch<CS> }, } },
	{ "bne", { { 0xd0, 2, Rel, Branch<NE> }, } },
	{ "beq", { { 0xf0, 2, Rel, Branch<EQ> }, } },
	{ "bpl", { { 0x10, 2, Rel, Branch<PL> }, } },
	{ "bmi", { { 0x30, 2, Rel, Branch<MI> }, } },
	{ "bvc", { { 0x50, 2, Rel, Branch<VC> }, } },
	{ "bvs", { { 0x70, 2, Rel, Branch<VS> }, } },

	{ "adc", {
		{ 0x69, 2, Imm, Adc<IMM>},
		{ 0x65, 3, Zp, Adc<ZP>},
		{ 0x75, 4, Zp_x, Adc<ZPX>},
		{ 0x6d, 4, Abs, Adc<ABS>},
		{ 0x7d, 4, Abs_x, Adc<ABSX>},
		{ 0x79, 4, Abs_y, Adc<ABSY>},
		{ 0x61, 6, Ind_x, Adc<INDX>},
		{ 0x71, 5, Ind_y, Adc<INDY>},
	} },

	{ "sbc", {
		{ 0xe9, 2, Imm, Sbc<IMM>},
		{ 0xe5, 3, Zp, Sbc<ZP>},
		{ 0xf5, 4, Zp_x, Sbc<ZPX>},
		{ 0xed, 4, Abs, Sbc<ABS>},
		{ 0xfd, 4, Abs_x, Sbc<ABSX>},
		{ 0xf9, 4, Abs_y, Sbc<ABSY>},
		{ 0xe1, 6, Ind_x, Sbc<INDX>},
		{ 0xf1, 5, Ind_y, Sbc<INDY>},
	} },

	{ "cmp", {
		{ 0xc9, 2, Imm, Cmp<A, IMM>},
		{ 0xc5, 3, Zp, Cmp<A, ZP>},
		{ 0xd5, 4, Zp_x, Cmp<A, ZPX>},
		{ 0xcd, 4, Abs, Cmp<A, ABS>},
		{ 0xdd, 4, Abs_x, Cmp<A, ABSX>},
		{ 0xd9, 4, Abs_y, Cmp<A, ABSY>},
		{ 0xc1, 6, Ind_x, Cmp<A, INDX>},
		{ 0xd1, 5, Ind_y, Cmp<A, INDY>},
	} },

	{ "cpx", {
		{ 0xe0, 2, Imm, Cmp<X, IMM>},
		{ 0xe4, 3, Zp, Cmp<X, ZP>},
		{ 0xec, 4, Abs, Cmp<X, ABS>},
	} },

	{ "cpy", {
		{ 0xc0, 2, Imm, Cmp<Y, IMM>},
		{ 0xc4, 3, Zp, Cmp<Y, ZP>},
		{ 0xcc, 4, Abs, Cmp<Y, ABS>},
	} },

	{ "and", {
		{ 0x29, 2, Imm, And<IMM>},
		{ 0x25, 3, Zp, And<ZP>},
		{ 0x35, 4, Zp_x, And<ZPX>},
		{ 0x2d, 4, Abs, And<ABS>},
		{ 0x3d, 4, Abs_x, And<ABSX>},
		{ 0x39, 4, Abs_y, And<ABSY>},
		{ 0x21, 6, Ind_x, And<INDX>},
		{ 0x31, 5, Ind_y, And<INDY>},
	} },

	{ "eor", {
		{ 0x49, 2, Imm, Eor<IMM>},
		{ 0x45, 3, Zp, Eor<ZP>},
		{ 0x55, 4, Zp_x, Eor<ZPX>},
		{ 0x4d, 4, Abs, Eor<ABS>},
		{ 0x5d, 4, Abs_x, Eor<ABSX>},
		{ 0x59, 4, Abs_y, Eor<ABSY>},
		{ 0x41, 6, Ind_x, Eor<INDX>},
		{ 0x51, 5, Ind_y, Eor<INDY>},
	} },

	{ "ora", {
		{ 0x09, 2, Imm, Ora<IMM>},
		{ 0x05, 3, Zp, Ora<ZP>},
		{ 0x15, 4, Zp_x, Ora<ZPX>},
		{ 0x0d, 4, Abs, Ora<ABS>},
		{ 0x1d, 4, Abs_x, Ora<ABSX>},
		{ 0x19, 4, Abs_y, Ora<ABSY>},
		{ 0x01, 6, Ind_x, Ora<INDX>},
		{ 0x11, 5, Ind_y, Ora<INDY>},
	} },

	{ "sec", { { 0x38, 2, None, Set<CARRY, true> } } },
	{ "clc", { { 0x18, 2, None, Set<CARRY, false> } } },
	{ "sei", { { 0x58, 2, None, Set<IRQ, true> } } },
	{ "cli", { { 0x78, 2, None, Set<IRQ, false> } } },
	{ "sed", { { 0xf8, 2, None, Set<DECIMAL, true> } } },
	{ "cld", { { 0xd8, 2, None, Set<DECIMAL, false> } } },
	{ "clv", { { 0xb8, 2, None, Set<OVER, false> } } },

	{ "lsr", { 
		{ 0x4a, 2, None, Lsr<ACC>},
		{ 0x4a, 2, Acc, Lsr<ACC>},
		{ 0x46, 5, Zp, Lsr<ZP>},
		{ 0x56, 6, Zp_x, Lsr<ZPX>},
		{ 0x4e, 6, Abs, Lsr<ABS>},
		{ 0x5e, 7, Abs_x, Lsr<ABSX>},
	} },

	{ "rts", {
		{ 0x60, 2, None, [](Machine &m) {  
			m.pc = (m.stack[m.sp+2] | (m.stack[m.sp+1]<<8))+1;
			m.sp += 2;
		}}
	} },

	{ "jsr", {
		{ 0x20, 3, Abs, [](Machine &m) {
			m.stack[m.sp-- & 0xff] = (m.pc+1) & 0xff;
			m.stack[m.sp-- & 0xff] = (m.pc+1) >> 8; 
			m.pc = ReadPCW(m);
	   }}
   	} },

};

static Opcode jumpTable[256];
static const char* opNames[256];
void init(Machine &m) {
	memset(&m, 0, sizeof(Machine));
	m.sp = 0xff;
	m.stack = &m.mem[0x100];
	for(const auto &i : instructionTable) {
		for(const auto &o : i.opcodes) {
			jumpTable[o.code].op = o.op;
			opNames[o.code] = i.name.c_str();
		}
	}

}

void checkEffect(Machine &m) {
	static Machine om;
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
			printf("%04x := %02x ", i, m.mem[i]);
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

std::unordered_map<uint16_t, std::function<void(Machine &m)>> breaks;

void set_break(uint16_t pc, std::function<void(Machine &m)> f) {
	breaks[pc] = f;
}

void run(Machine &m, uint32_t cycles) {

	auto toCycles = m.cycles + cycles;
	while(m.cycles < toCycles) {

		checkEffect(m);

		auto code = ReadPC(m);
		if(code == 0x60 && m.sp == 0xff)
			return;
		auto &op = jumpTable[code];
		//printf("OPCODE: %s\n", opNames[code]);
		op.op(m);
		m.cycles += op.cycles;
		if(breaks[m.pc])
			breaks[m.pc](m);
	}
}

} // namespace

#ifdef TEST

int main(int argc, char **argv) {
	sixfive::Machine m;
	sixfive::ops[0].opcodes[argc].op(m);
	return 0;
}

#endif
