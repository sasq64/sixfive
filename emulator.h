#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace sixfive {

// Adressing modes
enum AdressingMode {
	BAD,
	NONE,
	ACC,
	SIZE2,
	IMM,
	REL,
	ZP,
	ZPX,
	ZPY,
	INDX,
	INDY,
	SIZE3,
	IND,
	ABS,
	ABSX,
	ABSY
};

struct BaseM {};

template <typename POLICY> struct Machine;

struct HILO {
	HILO() {}
	HILO(uint8_t lo, uint8_t hi) : lo(lo), hi(hi) {}
	HILO(uint16_t a) : lo(a&0xff), hi(a>>8) {}
	HILO(const HILO &h) : lo(h.lo), hi(h.hi) {}
	HILO(HILO &&op) noexcept = default;
	HILO &operator=(HILO &&op) noexcept = default;
	HILO &operator=(const HILO &h) noexcept {
		return *this = h;
	}

	uint8_t lo; uint8_t hi;
	operator uint16_t() const { return lo | (hi<<8); }
	HILO& operator++() {
		lo++;
		hi += !lo;
		return *this;
	}

	HILO& operator+=(int x) {
		hi += (lo + x)>>8;
		lo += x;
		return *this;
	};
};


struct DefaultPolicy
{
	static constexpr uint16_t IOMASK = 0; // 0xff;
	static constexpr uint16_t IOBANK = 0xd0;

	static constexpr bool BankedMemory = true;
	// Must be convertable and constructable from uin16_t
	// lo() and hi() functions must extract low and high byte
	typedef uint16_t AdrType;
	static constexpr bool AlignReads = false || BankedMemory;

	static constexpr bool Debug = false;
	static constexpr bool StatusOpt = false;
	static constexpr int MemSize = 65536;

	static inline constexpr void writeIO(BaseM&, uint16_t adr, uint8_t v) {}
	static inline constexpr uint8_t readIO(BaseM&, uint16_t adr) { return 0; }

	static inline constexpr bool eachOp(BaseM&) { return false; }
};



template <typename POLICY = DefaultPolicy> struct Machine : public BaseM
{
	enum REGNAME { NOREG, A, X, Y, SP };
	enum STATUSFLAGS { CARRY, ZERO, IRQ, DECIMAL, BRK, xXx, OVER, SIGN };

	using Adr = typename POLICY::AdrType;

	using OpFunc = void (*)(Machine &);

	struct Opcode
	{
		Opcode() {}
		Opcode(int code, int cycles, AdressingMode mode, OpFunc op)
			: code(code), cycles(cycles), mode(mode), op(op) {}
		uint8_t code;
		int cycles;
		AdressingMode mode;
		OpFunc op;
	};

	struct Instruction
	{
		Instruction(const std::string &name, std::vector<Opcode> ov)
			: name(name), opcodes(ov) {}
		const std::string name;
		std::vector<Opcode> opcodes;
	};

	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint8_t sr;
	uint8_t sp;
	Adr pc;

	uint8_t *stack;
	uint8_t ram[POLICY::MemSize];
	uint8_t *rbank[POLICY::MemSize / 256];
	uint8_t *wbank[POLICY::MemSize / 256];

	uint32_t cycles;
	Opcode jumpTable[256];
	const char* opNames[256];


	Machine() { init(); }
	~Machine() = default;
	Machine(Machine &&op) noexcept = default;
	Machine &operator=(Machine &&op) noexcept = default;

	POLICY& policy() {
		static POLICY policy;
		return policy;
	}

	void writeRam(uint16_t org, const uint8_t data) { ram[org] = data; }

	void writeRam(uint16_t org, const uint8_t *data, int size) {
		for(int i=0; i<size; i++)
			ram[org+i] = data[i];
	}

	void readRam(uint16_t org, uint8_t *data, int size) {
		for(int i=0; i<size; i++)
			data[i] = ram[org+i];
	}

	uint8_t readRam(uint16_t org) { return ram[org]; }

	uint8_t readMem(uint16_t org) { 
		if(POLICY::BankedMemory)
			return rbank[org>>8][org&0xff];
		else
			return ram[org];
	}

	void readMem(uint16_t org, uint8_t *data, int size) {
		for(int i=0; i<size; i++)
			data[i] = readMem(org+i);
	}

	void mapRom(uint8_t bank, uint8_t*data, int len) {
		auto end = data + len;
		while(data < end) {
			rbank[bank++] = data;
			data += 256;
		}
	}

	uint8_t regA() { return a; }
	uint8_t regX() { return x; }
	uint8_t regY() { return y; }
	uint8_t regSR() { return sr; }

	void setPC(const int16_t &p) { pc = p; }

	uint32_t run(uint32_t runc = 0x01000000) {

		auto toCycles = cycles + runc;
		uint32_t opcodes = 0;
		while(cycles < toCycles) {

			if(POLICY::eachOp(*this))
				break;

			uint8_t code = ReadPC();
			if(code == 0x60 && (uint8_t)sp == 0xff)
				return opcodes;
			//printf("%04x\n", (uint16_t)pc);
			auto &op = jumpTable[code];
			op.op(*this);
			cycles += op.cycles;
			opcodes++;
		}
		return opcodes;
	}

private:

	void init() {
		sp = 0xff;
		stack = &ram[0x100];
		memset(&ram[0], 0, sizeof(ram));
		cycles = 0;
		a = x = y = 0;
		sr = 0x30;
		if(POLICY::BankedMemory) {
			for(int i=0; i<256; i++)
				rbank[i] = wbank[i] = &ram[i*256];
		}
		for(const auto &i : getInstructions()) {
			for(const auto &o : i.opcodes) {
				jumpTable[o.code] = o;
				opNames[o.code] = i.name.c_str();
			}
		}
	}
	


	// 16bit address representation

	Adr* tADR = nullptr;

	uint8_t lo(uint16_t a) const { return a & 0xff; }
	uint8_t hi(uint16_t a) const { return a >> 8; }
	uint8_t lo(const HILO &a) const { return a.lo; }
	uint8_t hi(const HILO &a) const { return a.hi; }

	// Create an Adress from lo and hi byte
	uint16_t make(uint16_t*, uint8_t lo, uint8_t hi = 0, int offs = 0) const {
		return (lo | (hi<<8)) + offs;
	}
	HILO make(HILO*, uint8_t lo, uint8_t hi = 0, int offs = 0) const {
		int a = lo + offs;
		return HILO(a, hi + (a>>8));
	}

	Adr makeAdr(uint8_t lo, uint8_t hi, int offs = 0) const {
		return make(tADR, lo, hi, offs);
	}

	// Read/write memory

	const uint8_t& Mem(const HILO &a) const {
		if(POLICY::BankedMemory)
			return rbank[hi(a)][lo(a)];
		else
			return ram[(uint16_t)a];
	}

	const uint8_t& Mem(const uint16_t &a) const {
		if(POLICY::BankedMemory)
			return rbank[hi(a)][lo(a)];
		else
			return ram[a];
	}

	const uint8_t& Mem(const uint8_t &lo, const uint8_t &hi) const {
		if(POLICY::BankedMemory)
			return rbank[hi][lo];
		else
			return ram[lo | (hi<<8)];
	}

	uint8_t& Mem(const HILO &a) {
		if(POLICY::BankedMemory)
			return wbank[hi(a)][lo(a)];
		else
			return ram[(uint16_t)a];
	}

	uint8_t& Mem(const uint16_t &a) {
		if(POLICY::BankedMemory)
			return wbank[hi(a)][lo(a)];
		else
			return ram[a];
	}

	uint8_t& Mem(const uint8_t &lo, const uint8_t &hi) {
		if(POLICY::BankedMemory)
			return wbank[hi][lo];
		else
			return ram[lo | (hi<<8)];
	}

	// TODO: Add optional support for IO reads by PC
	inline uint8_t ReadPC() { uint8_t r =  Mem(pc); ++pc; return r; }

	// Read Address from PC, Increment PC

	// TODO: These PC conversion + READ is expensive in opcode count

	inline Adr ReadPC8(int offs = 0) { 
		uint8_t r = Mem(pc); ++pc;
		return make(tADR, r + offs); 
	}

	Adr ReadPC16(uint8_t offs = 0) {
		//pc += 2;
		if(POLICY::AlignReads) {
			uint8_t lo = Mem(pc);
			uint8_t hi = Mem(++pc);
			++pc;
			//return make(tADR, Mem(pc), Mem(++pc - ), offs);
			return make(tADR, lo, hi, offs);
		} else {
			pc += 2;
			return *(uint16_t *)&ram[pc - 2] + offs;
		}
	}

	template <int REG> uint8_t &Reg() {
		switch(REG) {
		case A: return a;
		case X: return x;
		case Y: return y;
		}
	}

	/////////////////////////////////////////////////////////////////////////
	///
	/// THE STATUS REGISTER
	///
	/////////////////////////////////////////////////////////////////////////

	// S V - b d i Z C
	// 1 1 0 0 0 0 1 1

	uint8_t get_SR() { return sr; }
	void set_SR(uint8_t s) { sr = s | 0x30; }
	template <int REG> void set_SZ() {
		sr = (sr & 0x7d) | (Reg<REG>() & 0x80) | (!Reg<REG>() << 1);
	}

	void set_SZ(uint8_t res) { sr = (sr & 0x7d) | (res & 0x80) | (!res << 1); }

	void set_SZC(int res) {
		sr = (sr & 0x7c) | (res & 0x80) | (!(res & 0xff) << 1) |
			 ((res >> 8) & 1);
	}

	void set_SZCV(int res, int arg) {
		sr = (sr & 0x3c) | (res & 0x80) | (!(res&0xff) << 1) | ((res >> 8) & 1) |
			 ((~(a ^ arg) & (a ^ res) & 0x80) >> 1);
	}

	template <int FLAG> uint8_t mask() {
		return sr & (1<<FLAG);
	}

	static constexpr bool SET = true;
	static constexpr bool CLEAR = false;

	template <int FLAG, bool v> bool check() { return (bool)(sr & (1 << FLAG)) == v; }

	/////////////////////////////////////////////////////////////////////////
	///
	/// MEMORY ACCESS
	///
	/////////////////////////////////////////////////////////////////////////

	// Read a new address from the given address
	inline Adr Read16(uint16_t a, int offs = 0) {
		if(POLICY::AlignReads)
			return make(tADR, Mem(a), Mem(a + 1), offs);
		else
			return *(uint16_t*)&ram[a] + offs;
	}

	inline Adr Read16ZP(uint8_t a, int offs = 0) {
		if(POLICY::AlignReads)
			return make(tADR, Mem(a), Mem( (a + 1) & 0xff), offs);
		else
			return *(uint16_t*)&ram[a] + offs;
	}

	inline Adr Read16(HILO a, int offs = 0) {
		if(POLICY::AlignReads) {
			return make(tADR, Mem(a), Mem(a.lo + 1, a.hi + !(~a.lo & 0xff)));
		} else
			return *(uint16_t*)&ram[a] + offs;
	}

	inline Adr Read16ZP(HILO a, int offs = 0) {
		if(POLICY::AlignReads) {
			return make(tADR, Mem(a.lo, 0), Mem((a.lo + 1) & 0xff, 0), offs);
		} else
			return *(uint16_t*)&ram[a] + offs;
	}


	uint8_t Read(uint16_t adr) {
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			return POLICY::readIO(*this, adr);
		else
			return Mem(adr);
	}

	inline void Write(uint16_t adr, uint8_t v) {
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			POLICY::writeIO(*this, adr, v);
		else
			Mem(adr) = v;
	}

	uint8_t Read(HILO adr) {
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			return POLICY::readIO(*this, adr);
		else
			return Mem(adr);
	}

	inline void Write(HILO adr, uint8_t v) {
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			POLICY::writeIO(*this, adr, v);
		else
			Mem(adr) = v;
	}

	template <int MODE> Adr Address() {
		switch(MODE) {
		case ZP: return ReadPC8();
		case ZPX: return ReadPC8(x);
		case ZPY: return ReadPC8(y);
		case ABS: return ReadPC16();
		case ABSX: return ReadPC16(x);
		case ABSY: return ReadPC16(y);
		case INDX: return Read16ZP(ReadPC8(x));
		case INDY: return Read16ZP(ReadPC8(), y);
		case IND: return Read16(ReadPC16()); // TODO: ZP wrap?
		default: throw std::exception();
		}
	}

	// WRITE MEMORY

	template <int MODE> inline void Write(uint8_t v) {
		auto adr = Address<MODE>();
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			POLICY::writeIO(*this, adr, v);
		else
			Mem(adr) = v;
	}

	// READ MEMORY

	template <int MODE> uint8_t Read() {
		if(MODE == IMM)
			return ReadPC();
		Adr adr = Address<MODE>();
		if((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
			return POLICY::readIO(*this, adr);
		else
			return Mem(adr);
	}

	/////////////////////////////////////////////////////////////////////////
	///
	///   OPCODES
	///
	/////////////////////////////////////////////////////////////////////////

	template <int FLAG, bool v> static void Set(Machine &m) {
		m.sr = (m.sr & ~(1 << FLAG)) | (v << FLAG);
	}

	template <int REG, int MODE> static void Store(Machine &m) {
		m.Write<MODE>(m.Reg<REG>());
	}

	template <int REG, int MODE> static void Load(Machine &m) {

		m.Reg<REG>() = m.Read<MODE>();
		//if(MODE == ABSX && m.x > 0) {
		//	printf("Read %x + %x => %x\n", m.ram[m.pc-2] | (m.ram[m.pc-1]<<8), m.x, m.Reg<REG>());
		//}
		m.set_SZ<REG>();
	}

	template <int FLAG, bool v> static void Branch(Machine &m) {
		int8_t diff = m.ReadPC();
		int d = m.check<FLAG, v>();
		m.cycles += d;
		m.pc += (diff * d);
	}

	void Php(Machine &m) {
		m.stack[m.sp--] = m.get_SR();
		;
	}

	template <int MODE, int inc> static void Inc(Machine &m) {
		auto adr = m.Address<MODE>();
		auto rc = (m.Read(adr) + inc);
		m.Write(adr, rc);
		m.set_SZ(rc);
	}

	// === COMPARE, ADD & SUBTRACT

	template <int REG, int MODE> static void Bit(Machine &m) {
		uint8_t z = m.Read<MODE>();
		m.sr = (m.sr & 0x3d) | (z & 0xc0) | (!(z & m.a) << 1);
	}

	template <int REG, int MODE> static void Cmp(Machine &m) {
		uint8_t z = ~m.Read<MODE>();
		int rc = m.Reg<REG>() + z + 1;
		m.set_SZC(rc);
	}

	template <int MODE> static void Sbc(Machine &m) {
		uint8_t z = (~m.Read<MODE>());
		int rc = m.a + z + m.mask<CARRY>(); //(m.sr & 1);
		m.set_SZCV(rc, z);
		m.a = rc;
	}

	template <int MODE> static void Adc(Machine &m) {
		auto z = m.Read<MODE>();

		int rc = m.a + z + m.mask<CARRY>();
		m.set_SZCV(rc, z);
		m.a = rc;
	}

	template <int MODE> static void And(Machine &m) {
		m.a &= m.Read<MODE>();
		m.set_SZ<A>();
	}

	template <int MODE> static void Ora(Machine &m) {
		m.a |= m.Read<MODE>();
		m.set_SZ<A>();
	}

	template <int MODE> static void Eor(Machine &m) {
		m.a ^= m.Read<MODE>();
		m.set_SZ<A>();
	}

	// === SHIFTS & ROTATES

	template <int MODE> static void Asl(Machine &m) {
		if(MODE == ACC) {
			int rc = m.a << 1;
			m.set_SZC(rc);
			m.a = rc;
			return;
		}

		auto adr = m.Address<MODE>();
		int rc = m.Read(adr) << 1;
		m.set_SZC(rc);
		m.Write(adr, rc);
	}

	template <int MODE> static void Lsr(Machine &m) {
		if(MODE == ACC) {
			m.sr = (m.sr & 0xfe) | (m.a & 1);
			m.a >>= 1;
			m.set_SZ<A>();
			return;
		}
		auto adr = m.Address<MODE>();
		int rc = m.Read(adr);
		m.sr = (m.sr & 0xfe) | (rc & 1);
		rc >>= 1;
		m.Write(adr, rc);
		m.set_SZ(rc);
	}

	template <int MODE> static void Ror(Machine &m) {
		auto adr = m.Address<MODE>();
		int rc = m.Read(adr) | (m.sr << 8);
		m.sr = (m.sr & 0xfe) | (rc & 1);
		rc = (rc>>1);
		m.Write(adr, rc);
		m.set_SZ(rc);
	}

	static void RorA(Machine &m) {
		int rc = ((m.sr << 8) | m.a) >> 1;
		m.sr = (m.sr & 0xfe) | (m.a & 1);
		m.a = rc;
		m.set_SZ<A>();
	}

	template <int MODE> static void Rol(Machine &m) {
		auto adr = m.Address<MODE>();
		int rc = (m.Read(adr) << 1) | m.mask<CARRY>();
		m.Write(adr, rc);
		m.set_SZC(rc);
	}

	static void RolA(Machine &m) {
		int rc = (m.a << 1) | m.mask<CARRY>();
		m.set_SZC(rc);
		m.a = rc;
	}

public:

	static const std::vector<Instruction> &getInstructions() {
		static const std::vector<Instruction> instructionTable = {

	{"nop", {{ 0xea, 2, NONE, [](Machine &) {} }} },

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

	{ "tax", { { 0xaa, 2, NONE, [](Machine& m) { m.x = m.a ; m.set_SZ<X>(); } } } },
	{ "txa", { { 0x8a, 2, NONE, [](Machine& m) { m.a = m.x ; m.set_SZ<A>(); } } } },
	{ "dex", { { 0xca, 2, NONE, [](Machine& m) { m.x-- ; m.set_SZ<X>(); } } } },
	{ "inx", { { 0xe8, 2, NONE, [](Machine& m) { m.x++ ; m.set_SZ<X>(); } } } },
	{ "tay", { { 0xa8, 2, NONE, [](Machine& m) { m.y = m.a ; m.set_SZ<Y>(); } } } },
	{ "tya", { { 0x98, 2, NONE, [](Machine& m) { m.a = m.y ; m.set_SZ<A>(); } } } },
	{ "dey", { { 0x88, 2, NONE, [](Machine& m) { m.y-- ; m.set_SZ<Y>(); } } } },
	{ "iny", { { 0xc8, 2, NONE, [](Machine& m) { m.y++ ; m.set_SZ<Y>(); } } } },

	{ "txs", { { 0x9a, 2, NONE, [](Machine& m) { m.sp = m.x; } } } },
	{ "tsx", { { 0xba, 2, NONE, [](Machine& m) { m.x = m.sp; m.set_SZ<X>(); } } } },

	{ "pha", { { 0x48, 3, NONE, [](Machine& m) {
		m.stack[m.sp--] = m.a;
	} } } },

	{ "pla", { { 0x68, 4, NONE, [](Machine& m) {
		m.a = m.stack[++m.sp];
	} } } },

	{ "php", { { 0x08, 3, NONE, [](Machine& m) {
		m.stack[m.sp--] = m.get_SR();
	} } } },

	{ "plp", { { 0x28, 4, NONE, [](Machine& m) {
		m.set_SR(m.stack[++m.sp]);
	} } } },

	{ "bcc", { { 0x90, 2, REL, Branch<CARRY, CLEAR> }, } },
	{ "bcs", { { 0xb0, 2, REL, Branch<CARRY, SET> }, } },
	{ "bne", { { 0xd0, 2, REL, Branch<ZERO, CLEAR> }, } },
	{ "beq", { { 0xf0, 2, REL, Branch<ZERO, SET> }, } },
	{ "bpl", { { 0x10, 2, REL, Branch<SIGN, CLEAR> }, } },
	{ "bmi", { { 0x30, 2, REL, Branch<SIGN, SET> }, } },
	{ "bvc", { { 0x50, 2, REL, Branch<OVER, CLEAR> }, } },
	{ "bvs", { { 0x70, 2, REL, Branch<OVER, SET> }, } },

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
		{ 0x6a, 2, NONE, RorA},
		{ 0x6a, 2, ACC, RorA},
		{ 0x66, 5, ZP, Ror<ZP>},
		{ 0x76, 6, ZPX, Ror<ZPX>},
		{ 0x6e, 6, ABS, Ror<ABS>},
		{ 0x7e, 7, ABSX, Ror<ABSX>},
	} },

	{ "rol", { 
		{ 0x2a, 2, NONE, RolA},
		{ 0x2a, 2, ACC, RolA},
		{ 0x26, 5, ZP, Rol<ZP>},
		{ 0x36, 6, ZPX, Rol<ZPX>},
		{ 0x2e, 6, ABS, Rol<ABS>},
		{ 0x3e, 7, ABSX, Rol<ABSX>},
	} },

	{ "bit", {
		{ 0x24, 3, ZP, Bit<X, ZP>},
		{ 0x2c, 4, ABS, Bit<X, ABS>},
	} },

	{ "rti", {
		{ 0x40, 6, NONE, [](Machine &m) { 
			m.set_SR(m.stack[++m.sp]);// & !(1<<BRK));
			m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8));
			m.sp += 2;
		} }
	} },

	{ "brk", {
		{ 0x00, 7, NONE, [](Machine &m) {  
			m.ReadPC();
			m.stack[m.sp--] = m.pc >> 8;
			m.stack[m.sp--] = m.pc & 0xff; 
			m.stack[m.sp--] = m.get_SR();// | (1<<BRK);
			//m.pc = m.Read16((uint16_t)0xfffe);
			m.pc = m.Read16(m.makeAdr(0xfe, 0xff));
		} }
	} },

	{ "rts", {
		{ 0x60, 6, NONE, [](Machine &m) {  
			m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1;
			m.sp += 2;
		} }
	} },

	{ "jmp", {
		{ 0x4c, 3, ABS, [](Machine &m) {
			m.pc = m.ReadPC16();
		} },
		{ 0x6c, 5, IND, [](Machine &m) {
			auto a = m.ReadPC16();
			if((uint16_t)a != 0)
				printf("READ %04x\n", (uint16_t)a);
			m.pc = m.Read16(a);
		} }
	} },

	{ "jsr", {
		{ 0x20, 6, ABS, [](Machine &m) {
			m.stack[m.sp--] = (m.pc+1) >> 8;
			m.stack[m.sp--] = (m.pc+1) & 0xff; 
			m.pc = m.ReadPC16();
		} }
	} },

	};
		return instructionTable;
	}
};

} // namespace
