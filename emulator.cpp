#include <vector>

#include "emulator.h"

namespace sixfive {

// Registers
enum { NOREG, A, X, Y, SP };
// Adressing modes

// Condition codes
enum { EQ, NE, PL, MI, CC, CS, VC, VS };

constexpr bool AlignReads = false;
constexpr bool UsePCPointer = false;
constexpr bool StatusOpt = false;

// PROGRAM COUNTER

template <bool ALIGN = AlignReads, bool PCP = UsePCPointer> inline uint16_t ReadPCW(Machine &m);
template <bool PCP = UsePCPointer> inline Word& ReadPC(Machine&);
template <bool PCP = UsePCPointer> inline SWord& ReadSPC(Machine&);
template <bool PCP = UsePCPointer> inline uint16_t GetPC(Machine&);
template <bool PCP = UsePCPointer> inline void SetPC(Machine&, uint16_t);
  
template <> inline Word& ReadPC<true>(Machine &m) { return *m.pcp++; }
template <> inline Word& ReadPC<false>(Machine &m) { return m.mem[m.pc++]; }

template <> inline SWord& ReadSPC<true>(Machine &m) { return *(SWord*)m.pcp++; }
template <> inline SWord& ReadSPC<false>(Machine &m) { return ((SWord*)m.mem)[m.pc++]; }
  
template <> inline uint16_t ReadPCW<true, true>(Machine &m) {  m.pcp += 2; return m.pcp[-2] | (m.pcp[-1]<<8); }
template <> inline uint16_t ReadPCW<false, true>(Machine &m) { return *(uint16_t*)(m.pcp += 2); }

template <> inline uint16_t ReadPCW<true, false>(Machine &m) {  m.pc += 2; return m.mem[m.pc - 2] | (m.mem[m.pc-1]<<8); }
template <> inline uint16_t ReadPCW<false, false>(Machine &m) { return *(uint16_t*)&m.mem[m.pc += 2]; }

template<> inline uint16_t GetPC<false>(Machine &m) {
	return m.pc;
}

template <> inline void SetPC<false>(Machine &m, uint16_t pc) {
	m.pc = pc;
}

template<> inline uint16_t GetPC<true>(Machine &m) {
	return m.pcp - m.mem;
}

template <> inline void SetPC<true>(Machine &m, uint16_t pc) {
	m.pcp = m.mem + pc;
}

/// MEMORY

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


template <> struct flags<false> {
	inline static const Word& get(Machine &m) {
		return m.sr;
	}
   // S V - b d i Z C
   // 1 1 0 0 0 0 1 1

   template <int REG> inline static void set_SZ(Machine &m) {
		m.sr = (m.sr & 0x7d) | (Reg<REG>(m) & 0x80) | (!Reg<REG>(m) << 1);
	}

	template <int REG> inline static void set_SZC(Machine &m, int res)
	{
		m.sr = (m.sr & 0x7c) | (Reg<REG>(m) & 0x80) | (!Reg<REG>(m) << 1) | (res>>8);
	}

	template <int REG> inline static void set_SZCV(Machine &m, int res, int arg) {
		m.sr = (m.sr & 0xbc) | (Reg<REG>(m) & 0x80) | (!Reg<REG>(m) << 1) | (res>>8) | ((~(m.a ^ arg) & (m.a ^ res) & 0x80)>>1);
	}

};

template <> struct flags<true> {
	template <int REG> inline static void set_SZ(Machine &m) {
		m.lastWord = Reg<REG>(m);
	}

	template <int REG> inline static void set_SZC(Machine &m, int res)
	{
		m.lastWord = Reg<REG>(m);
		m.sr = (m.sr & 0x7c) | (res>>8);
	}

	template <int REG> inline static void set_SZCV(Machine &m, int res, int arg) {
		m.lastWord = Reg<REG>(m);
		m.sr = (m.sr & 0xbc) | (res>>8) | ((~(m.a ^ arg) & (m.a ^ res) & 0x80)>>1);
	}

	inline static Word get(Machine &m) {
		return m.sr | (m.lastWord & 0x80) | (!m.lastWord << 1);
	}

};

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

// Return a reference to the byte referenced by the opcode argument
template <int MODE, bool ALIGN = AlignReads> Word &Mem(Machine &m);

template <> inline Word &Mem<IMM>(Machine &m) {
	return ReadPC(m);
}

template <> inline Word &Mem<ZP>(Machine &m) {
	return m.mem[ReadPC(m)];
}

template <> inline Word &Mem<ZPX>(Machine &m) {
	return m.mem[ReadPC(m) + m.x];
}

template <> inline Word &Mem<ZPY>(Machine &m) {
	return m.mem[ReadPC(m) + m.y];
}

template <> inline Word &Mem<ABS>(Machine &m) {
	return m.mem[ReadPCW(m)];
}

template <> inline Word &Mem<ABSX>(Machine &m) {
	return m.mem[ReadPCW(m) + m.x];
}

template <> inline Word &Mem<ABSY>(Machine &m) {
	return m.mem[ReadPCW(m) + m.y];
}

template <> inline Word &Mem<INDX>(Machine &m) {
	return m.mem[Read16(m, ReadPC(m) + m.x)];
}

template <> inline Word &Mem<INDY>(Machine &m) {
	return m.mem[Read16(m, ReadPC(m)) + m.y];
}

///
///
///   OPCODES
///
///


template <int REG, int MODE> void Store(Machine &m) {
	Mem<MODE>(m) = Reg<REG>(m);
}

template <int REG, int MODE> void Load(Machine &m) {
	Reg<REG>(m) = Mem<MODE>(m);
	flags<>::set_SZ<REG>(m);
}

template <int COND> void Branch(Machine &m) {
	auto diff = ReadSPC(m);
	int d = check<COND>(m);
	m.cycles += d;
	m.pc += (diff * d);
}

void Php(Machine &m) {
	m.stack[m.sp--] = flags<>::get(m);
}

template<int MODE> void Adc(Machine &m) {
	auto z = Mem<MODE>(m);

	int rc = m.a + z + (m.sr & 1);
	printf("## ADC %x + %x + %x = %x\n", m.a, z, m.sr & 1, rc);
	flags<>::set_SZCV<A>(m, rc, z);
	m.a = rc & 0xff;

}
std::vector<Instruction> instructionTable  {

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
		{ 0x8d, 4, Abs, Store<A,ABS>},
		{ 0x9d, 4, Abs_x, Store<A,ABSX>},
		{ 0x99, 4, Abs_y, Store<A,ABSY>},
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
		{ 0x69, 2, Imm, Adc<IMM> },
		{ 0x6d, 4, Abs, Adc<ABS> },
	} },

	{ "sec", { { 0x38, 2, None, [](Machine& m) { m.sr |= 0x1; } } } },
	{ "clc", { { 0x18, 2, None, [](Machine& m) { m.sr &= ~0x1; } } } },
	{ "rts", {
		{ 0x60, 2, None, [](Machine &m) {  
			SetPC(m, m.stack[m.sp+1] | (m.stack[m.sp+2]<<8));
			m.sp += 2;
		}}
	} },

	{ "jsr", {
		{ 0x60, 3, Abs, [](Machine &m) {
			auto pc = GetPC(m);
		m.stack[m.sp-- & 0xff] = pc & 0xff;
		m.stack[m.sp-- & 0xff] = pc >> 8; 
		SetPC(m, ReadPCW(m));
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
	if(om.pc != m.pc)
		printf("PC := %04x\n", (unsigned)m.pc);
	if(om.a != m.a)
		printf("A := %02x\n", m.a);
	if(om.x != m.x)
		printf("X := %02x\n", m.x);
	if(om.y != m.y)
		printf("Y := %02x\n", m.y);
	if(om.sr != m.sr)
		printf("SR := %02x\n", m.sr);
	for(int i=0; i<65536; i++)
		if(m.mem[i] != om.mem[i]) {
			printf("%04x := %02x\n", i, m.mem[i]);
			om.mem[i] = m.mem[i];
		}
	om.a = m.a;
	om.x = m.x;
	om.y = m.y;
	om.sp = m.sp;
	om.sr = m.sr;
	om.pc = m.pc;

}

void run(Machine &m, uint32_t cycles) {

	auto toCycles = m.cycles + cycles;
	while(m.cycles < toCycles) {

		checkEffect(m);

		auto code = ReadPC(m);
		if(code == 0x60 && m.sp == 0xff)
			return;
		auto &op = jumpTable[code];
		printf("OPCODE: %s\n", opNames[code]);
		op.op(m);
		m.cycles += op.cycles;
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
