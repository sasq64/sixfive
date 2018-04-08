#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

namespace sixfive {

// Adressing modes
enum AdressingMode
{
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

struct BaseM
{};

template <typename POLICY> struct Machine;

// The Policy defines the compile time settings for the emulator
struct DefaultPolicy
{
    static constexpr uint16_t IOMASK = 0; // 0xff;
    static constexpr uint16_t IOBANK = 0xd0;

    static constexpr bool BankedMemory = false;
    // Must be convertable and constructable from uin16_t
    // lo() and hi() functions must extract low and high byte
    using AdrType = uint16_t;
    static constexpr bool AlignReads = false; // BankedMemory;
    static constexpr bool ExitOnStackWrap = true;

    static constexpr bool Debug = false;
    static constexpr int MemSize = 65536;

    static inline constexpr void writeIO(BaseM&, uint16_t adr, uint8_t v) {}
    static inline constexpr uint8_t readIO(const BaseM&, uint16_t adr)
    {
        return 0;
    }

    static inline constexpr bool eachOp(BaseM&) { return false; }
};

template <typename POLICY = DefaultPolicy> struct Machine : public BaseM
{
    enum REGNAME
    {
        NOREG = 20,
        A,
        X,
        Y,
        SP,
        SR
    };

    enum STATUSFLAGS
    {
        CARRY,
        ZERO,
        IRQ,
        DECIMAL,
        BRK,
        xXx,
        OVER,
        SIGN
    };

    using Adr = typename POLICY::AdrType;
    using Word = uint8_t;
    using Flags = uint8_t;

    using OpFunc = void (*)(Machine&);

    struct Opcode
    {
        Opcode() = default;
        Opcode(int code, int cycles, AdressingMode mode, OpFunc op)
            : code(code), cycles(cycles), mode(mode), op(op)
        {}
        OpFunc op;
        uint8_t code;
        uint8_t cycles;
        uint8_t mode;
    };

    struct Instruction
    {
        Instruction(const char* name, std::vector<Opcode> ov)
            : name(name), opcodes(std::move(ov))
        {}
        const char* name;
        std::vector<Opcode> opcodes;
    };

    ~Machine() = default;
    Machine(Machine&& op) noexcept = default;
    Machine& operator=(Machine&& op) noexcept = default;

    Machine()
    {
        sp = 0xff;
        ram.fill(0);
        stack = &ram[0x100];
        cycles = 0;
        a = x = y = 0;
        sr = 0x30;
        toCycles = 0;
        if (POLICY::BankedMemory) {
            for (int i = 0; i < 256; i++)
                rbank[i] = wbank[i] = &ram[i * 256];
        }
        for (const auto& i : getInstructions()) {
            for (const auto& o : i.opcodes) {
                jumpTable[o.code] = o;
                opNames[o.code] = i.name;
            }
        }
    }

    POLICY& policy()
    {
        static POLICY policy;
        return policy;
    }

    // Access ram directly
    //

    const Word& Ram(const Adr& a) const { return ram[a]; }

    Word& Ram(const Adr& a) { return ram[a]; }

    const Word& Stack(const Word& a) const { return stack[a]; }

    void writeRam(uint16_t org, const Word data) { ram[org] = data; }

    void writeRam(uint16_t org, const void* data, int size)
    {
        auto* data8 = (uint8_t*)data;
        for (int i = 0; i < size; i++)
            ram[org + i] = data8[i];
    }

    void readRam(uint16_t org, void* data, int size) const
    {
        auto* data8 = (Word*)data;
        for (int i = 0; i < size; i++)
            data8[i] = ram[org + i];
    }

    Word readRam(uint16_t org) const { return ram[org]; }

    // Access memory through bank mapping

    Word readMem(uint16_t org) const
    {
        if (POLICY::BankedMemory) return rbank[org >> 8][org & 0xff];
        return ram[org];
    }

    void readMem(uint16_t org, void* data, int size) const
    {
        auto* data8 = (Word*)data;
        for (int i = 0; i < size; i++)
            data8[i] = readMem(org + i);
    }

    // Map ROM to a bank
    void mapRom(uint8_t bank, const Word* data, int len)
    {
        auto end = data + len;
        while (data < end) {
            rbank[bank++] = const_cast<Word*>(data);
            data += 256;
        }
    }

    auto regs() const { return std::make_tuple(a, x, y, sr, sp, pc); }
    auto regs() { return std::tie(a, x, y, sr, sp, pc); }

    Word regA() const { return a; }
    Word regX() const { return x; }
    Word regY() const { return y; }
    uint8_t regSP() const { return sp; }
    Adr regPC() const { return pc; }

    uint8_t regSR() const { return sr; }

    void setPC(const int16_t& p) { pc = p; }

    uint32_t run(uint32_t runc = 0x01000000)
    {

        toCycles = cycles + runc;
        uint32_t opcodes = 0;
        while (cycles < toCycles) {

            if (POLICY::eachOp(*this)) break;

            auto code = ReadPC();
            if constexpr (POLICY::ExitOnStackWrap) {
                if (code == 0x60 && (uint8_t)sp == 0xff) return opcodes;
            }
            auto& op = jumpTable[code];
            op.op(*this);
            cycles += op.cycles;
            opcodes++;
        }
        return opcodes;
    }

    static const std::vector<Instruction>& getInstructions()
    {
        return instructionTable;
    }

private:
    // The 6502 registers
    unsigned a;
    unsigned x;
    unsigned y;
    unsigned sr;
    uint8_t sp;
    Adr pc;

    // 6502 RAM
    std::array<Word, POLICY::MemSize> ram;

    // Stack normally points to ram[0x100];
    Word* stack;

    // Banks normally point to corresponding ram
    std::array<const Word*, POLICY::MemSize / 256> rbank;
    std::array<Word*, POLICY::MemSize / 256> wbank;

    uint32_t toCycles;
    uint32_t cycles;

    // All opcodes
    std::array<Opcode, 256> jumpTable;
    std::array<const char*, 256> opNames;

    static constexpr Word lo(Adr a) { return a & 0xff; }
    static constexpr Word hi(Adr a) { return a >> 8; }
    static constexpr Adr to_adr(Word lo, Word hi) { return (hi << 8) | lo; }

    // Access memory

    const Word& Mem(const Adr& a) const
    {
        if constexpr (POLICY::BankedMemory)
            return rbank[hi(a)][lo(a)];
        else
            return ram[a];
    }

    Word& Mem(const Adr& a)
    {
        if constexpr (POLICY::BankedMemory)
            return wbank[hi(a)][lo(a)];
        else
            return ram[a];
    }

    const Word& Mem(const uint8_t& lo, const uint8_t& hi) const
    {
        if constexpr (POLICY::BankedMemory)
            return rbank[hi][lo];
        else
            return ram[lo | (hi << 8)];
    }

    Word& Mem(const uint8_t& lo, const uint8_t& hi)
    {
        if constexpr (POLICY::BankedMemory)
            return wbank[hi][lo];
        else
            return ram[lo | (hi << 8)];
    }

    // TODO: Add optional support for IO reads by PC
    Word ReadPC()
    {
        Word r = Mem(pc);
        ++pc;
        return r;
    }

    // Read Address from PC, Increment PC

    // TODO: These PC conversion + READ is expensive in opcode count

    inline Adr ReadPC8(int offs = 0) { return (Mem(pc++) + offs) & 0xff; }

    Adr ReadPC16(uint8_t offs = 0)
    {
        if constexpr (POLICY::AlignReads) {
            uint8_t lo = Mem(pc);
            uint8_t hi = Mem(++pc);
            ++pc;
            return to_adr(lo, hi) + offs;
        } else {
            pc += 2;
            return *(uint16_t*)&ram[pc - 2] + offs;
        }
    }

    template <int REG> const auto& Reg() const
    {
        if constexpr (REG == A) return a;
        if constexpr (REG == X) return x;
        if constexpr (REG == Y) return y;
        if constexpr (REG == SP) return sp;
    }

    template <int REG> auto& Reg()
    {
        if constexpr (REG == A) return a;
        if constexpr (REG == X) return x;
        if constexpr (REG == Y) return y;
        if constexpr (REG == SP) return sp;
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    /// THE STATUS REGISTER
    ///
    /////////////////////////////////////////////////////////////////////////

    // S V - b d i Z C
    // 1 1 0 0 0 0 1 1

    enum STATUS_BITS
    {
        C = 0x1,
        Z = 0x2,
        V = 0x40,
        S = 0x80,
    };

    static constexpr auto SZ = S | Z;
    static constexpr auto SZC = S | Z | C;
    static constexpr auto SZCV = S | Z | C | V;

    uint8_t get_SR() const { return sr; }

    void set_SR(uint8_t s) { sr = s | 0x30; }

    template <int BITS> void set(int res, int arg = 0)
    {
        sr &= ~BITS;

        if constexpr ((BITS & S) != 0) sr |= (res & 0x80); // Apply signed bit
        if constexpr ((BITS & Z) != 0) sr |= (!(res & 0xff) << 1); // Apply zero
        if constexpr ((BITS & C) != 0) sr |= ((res >> 8) & 1); // Apply carry
        if constexpr ((BITS & V) != 0)
            sr |= ((~(a ^ arg) & (a ^ res) & 0x80) >> 1); // Apply overflow
    }

    template <int REG> void set_SZ()
    {
        sr = (sr & ~SZ) | (Reg<REG>() & 0x80) | (!Reg<REG>() << 1);
    }

    template <int FLAG> int check() const { return sr & (1 << FLAG); }

    static constexpr bool SET = true;
    static constexpr bool CLEAR = false;

    template <int FLAG, bool v> bool check() const
    {
        return (bool)(sr & (1 << FLAG)) == v;
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    /// MEMORY ACCESS
    ///
    /////////////////////////////////////////////////////////////////////////

    // Read a new address from the given address
    inline Adr Read16(uint16_t a, int offs = 0) const
    {
        if constexpr (POLICY::AlignReads)
            return to_adr(Mem(a), Mem(a + 1)) + offs;
        return *(uint16_t*)&ram[a] + offs;
    }

    Adr Read16ZP(uint8_t a, int offs = 0) const
    {
        if constexpr (POLICY::AlignReads)
            return to_adr(Mem(a), Mem((a + 1) & 0xff)) + offs;
        return *(uint16_t*)&ram[a] + offs;
    }

    Adr Read16ZP(uint16_t a, int offs = 0) const
    {
        if constexpr (POLICY::AlignReads)
            return to_adr(Mem(lo(a), 0), Mem((lo(a) + 1) & 0xff, 0)) + offs;
        return *(uint16_t*)&ram[a] + offs;
    }

    Word Read(uint16_t adr) const
    {
        if ((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
            return POLICY::readIO(*this, adr);
        return Mem(adr);
    }

    void Write(uint16_t adr, Word v)
    {
        if ((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
            POLICY::writeIO(*this, adr, v);
        Mem(adr) = v;
    }

    // Read operand from PC and create effective adress depeding on 'MODE'
    template <int MODE> Adr ReadEA()
    {
        if constexpr (MODE == ZP) return ReadPC8();
        if constexpr (MODE == ZPX) return ReadPC8(x);
        if constexpr (MODE == ZPY) return ReadPC8(y);
        if constexpr (MODE == ABS) return ReadPC16();
        if constexpr (MODE == ABSX) return ReadPC16(x);
        if constexpr (MODE == ABSY) return ReadPC16(y);
        if constexpr (MODE == INDX) return Read16ZP(ReadPC8(x));
        if constexpr (MODE == INDY) return Read16ZP(ReadPC8(), y);
        if constexpr (MODE == IND) return Read16(ReadPC16()); // TODO: ZP wrap?
    }

    template <int MODE> inline void StoreEA(Word v)
    {
        auto adr = ReadEA<MODE>();
        if ((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
            POLICY::writeIO(*this, adr, v);
        else
            Mem(adr) = v;
    }

    template <int MODE> Word LoadEA()
    {
        if constexpr (MODE == IMM)
            return ReadPC();
        else {
            Adr adr = ReadEA<MODE>();
            if ((hi(adr) & POLICY::IOMASK) == POLICY::IOBANK)
                return POLICY::readIO(*this, adr);
            return Mem(adr);
        }
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   OPCODES
    ///
    /////////////////////////////////////////////////////////////////////////

    template <int FLAG, bool v> static constexpr void Set(Machine& m)
    {
        m.sr = (m.sr & ~(1 << FLAG)) | (v << FLAG);
    }

    template <int REG, int MODE> static constexpr void Store(Machine& m)
    {
        m.StoreEA<MODE>(m.Reg<REG>());
    }

    template <int REG, int MODE> static constexpr void Load(Machine& m)
    {
        m.Reg<REG>() = m.LoadEA<MODE>();
        m.set_SZ<REG>();
    }

    template <int FLAG, bool v> static constexpr void Branch(Machine& m)
    {
        int8_t diff = m.ReadPC();
        int d = m.check<FLAG, v>();
        m.cycles += d;
        m.pc += (diff * d);
    }

    template <int MODE, int inc> static constexpr void Inc(Machine& m)
    {
        if constexpr(MODE >= A) {
            m.Reg<MODE>() = (m.Reg<MODE>() + inc) & 0xff;
            m.set_SZ<MODE>();
        } else {
            auto adr = m.ReadEA<MODE>();
            auto rc = (m.Read(adr) + inc);
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    // === COMPARE, ADD & SUBTRACT

    template <int REG, int MODE> static constexpr void Bit(Machine& m)
    {
        Word z = m.LoadEA<MODE>();
        m.set_SR((m.get_SR() & 0x3d) | (z & 0xc0) | (!(z & m.a) << 1));
    }

    template <int REG, int MODE> static constexpr void Cmp(Machine& m)
    {
        Word z = (~m.LoadEA<MODE>()) & 0xff;
        unsigned rc = m.Reg<REG>() + z + 1;
        m.set<SZC>(rc);
    }

    template <int MODE> static constexpr void Sbc(Machine& m)
    {
        Word z = (~m.LoadEA<MODE>()) & 0xff;
        int rc = m.a + z + m.check<CARRY>();
        m.set<SZCV>(rc, z);
        m.a = rc & 0xff;
    }

    template <int MODE> static constexpr void Adc(Machine& m)
    {
        auto z = m.LoadEA<MODE>();
        int rc = m.a + z + m.check<CARRY>();
        m.set<SZCV>(rc, z);
        m.a = rc & 0xff;
    }

    template <int MODE> static constexpr void And(Machine& m)
    {
        m.a &= m.LoadEA<MODE>();
        m.set_SZ<A>();
    }

    template <int MODE> static constexpr void Ora(Machine& m)
    {
        m.a |= m.LoadEA<MODE>();
        m.set_SZ<A>();
    }

    template <int MODE> static constexpr void Eor(Machine& m)
    {
        m.a ^= m.LoadEA<MODE>();
        m.set_SZ<A>();
    }

    // === SHIFTS & ROTATES

    template <int MODE> static constexpr void Asl(Machine& m)
    {
        if constexpr (MODE == ACC) {
            int rc = m.a << 1;
            m.set<SZC>(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA<MODE>();
            int rc = m.Read(adr) << 1;
            m.set<SZC>(rc);
            m.Write(adr, rc);
        }
    }

    template <int MODE> static constexpr void Lsr(Machine& m)
    {
        if constexpr (MODE == ACC) {
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a >>= 1;
            m.set_SZ<A>();
        } else {
            auto adr = m.ReadEA<MODE>();
            int rc = m.Read(adr);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    template <int MODE> static constexpr void Ror(Machine& m)
    {
        auto adr = m.ReadEA<MODE>();
        unsigned rc = m.Read(adr) | (m.sr << 8);
        m.sr = (m.sr & 0xfe) | (rc & 1);
        rc >>= 1;
        m.Write(adr, rc);
        m.set<SZ>(rc);
    }

    static constexpr void RorA(Machine& m)
    {
        unsigned rc = ((m.sr << 8) | m.a) >> 1;
        m.sr = (m.sr & 0xfe) | (m.a & 1);
        m.a = rc & 0xff;
        m.set_SZ<A>();
    }

    template <int MODE> static constexpr void Rol(Machine& m)
    {
        auto adr = m.ReadEA<MODE>();
        unsigned rc = (m.Read(adr) << 1) | m.check<CARRY>();
        m.Write(adr, rc);
        m.set<SZC>(rc);
    }

    static constexpr void RolA(Machine& m)
    {
        unsigned rc = (m.a << 1) | m.check<CARRY>();
        m.set<SZC>(rc);
        m.a = rc & 0xff;
    }

    template <int FROM, int TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr(TO != SP) m.set_SZ<TO>();
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   INSTRUCTION TABLE
    ///
    /////////////////////////////////////////////////////////////////////////

    static const inline std::vector<Instruction> instructionTable = {

        {"nop", {{ 0xea, 2, NONE, [](Machine& ) {} }} },

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

        { "tax", { { 0xaa, 2, NONE, Transfer<A, X> } } },
        { "txa", { { 0x8a, 2, NONE, Transfer<X, A> } } },
        { "tay", { { 0xa8, 2, NONE, Transfer<A, Y> } } },
        { "tya", { { 0x98, 2, NONE, Transfer<Y, A> } } },
        { "txs", { { 0x9a, 2, NONE, Transfer<X, SP> } } },
        { "tsx", { { 0xba, 2, NONE, Transfer<SP, X> } } },

        { "dex", { { 0xca, 2, NONE, Inc<X, -1> } } },
        { "inx", { { 0xe8, 2, NONE, Inc<X, 1> } } },
        { "dey", { { 0x88, 2, NONE, Inc<Y, -1> } } },
        { "iny", { { 0xc8, 2, NONE, Inc<Y, 1> } } },

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
            { 0x40, 6, NONE, [](Machine& m) {
                m.set_SR(m.stack[++m.sp]);// & !(1<<BRK));
                m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8));
                m.sp += 2;
            } }
        } },

        { "brk", {
            { 0x00, 7, NONE, [](Machine& m) {
                m.ReadPC();
                m.stack[m.sp--] = m.pc >> 8;
                m.stack[m.sp--] = m.pc & 0xff;
                m.stack[m.sp--] = m.get_SR();// | (1<<BRK);
                m.pc = m.Read16(m.to_adr(0xfe, 0xff));
            } }
        } },

        { "rts", {
            { 0x60, 6, NONE, [](Machine& m) {
                m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1;
                m.sp += 2;
            } }
        } },

        { "jmp", {
            { 0x4c, 3, ABS, [](Machine& m) {
                m.pc = m.ReadPC16();
            } },
            { 0x6c, 5, IND, [](Machine& m) {
                auto a = m.ReadPC16();
                m.pc = m.Read16(a);
            } }
        } },

        { "jsr", {
            { 0x20, 6, ABS, [](Machine& m) {
                m.stack[m.sp--] = (m.pc+1U) >> 8;
                m.stack[m.sp--] = (m.pc+1) & 0xffU;
                m.pc = m.ReadPC16();
            } }
        } },

	};
};

} // namespace sixfive
