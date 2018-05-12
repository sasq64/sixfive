#pragma once

#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

namespace sixfive {

// Adressing modes
enum AdressingMode : uint8_t
{
    BAD,
    NONE,
    ACC,
    IMM,
    REL,
    ZP,
    ZPX,
    ZPY,
    INDX,
    INDY,
    IND,
    ABS,
    ABSX,
    ABSY,
};

static constexpr inline int opSize(AdressingMode am)
{
    return am >= IND ? 3 : (am >= IMM ? 2 : 1);
}

template <typename POLICY> struct Machine;

enum EmulatedMemoryAccess
{
    DIRECT,  // Access `ram` array directly; Means no bank switching, ROM areas
             // or IO areas
    BANKED,  // Access memory through `wbank` and `rbank`; Means no IO areas
    CALLBACK // Access memory via function pointer per bank
};

// The Policy defines the compile & runtime time settings for the emulator
struct DefaultPolicy
{
    DefaultPolicy() {}
    DefaultPolicy(Machine<DefaultPolicy>& m) {}

    static constexpr bool ExitOnStackWrap = true;

    // PC accesses does not normally need to work in IO areas
    static constexpr int PC_AccessMode = BANKED;

    // Generic reads and writes should normally not be direct
    static constexpr int Read_AccessMode = CALLBACK;
    static constexpr int Write_AccessMode = CALLBACK;

    static constexpr int MemSize = 65536;

    // This function is run after each opcode. Return true to stop emulation.
    static constexpr bool eachOp(DefaultPolicy&) { return false; }
};

template <typename POLICY = DefaultPolicy> struct Machine
{
    enum REGNAME
    {
        A = 20,
        X,
        Y,
        SP,
        SR
    };

    template <int MODE> constexpr static bool IsReg() { return MODE >= A; }

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

    using Adr = uint16_t;
    using Word = uint8_t;

    using OpFunc = void (*)(Machine&);

    struct Opcode
    {
        Opcode() = default;
        Opcode(int code, int cycles, AdressingMode mode, OpFunc op)
            : code(code), cycles(cycles), mode(mode), op(op)
        {}
        OpFunc op;
        uint8_t cycles;
        uint8_t code;
        AdressingMode mode;
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
        result = 0;
        toCycles = 0;
        for (int i = 0; i < 256; i++) {
            rbank[i] = wbank[i] = &ram[(i * 256) % POLICY::MemSize];
            rcallbacks[i] = &read_bank;
            wcallbacks[i] = &write_bank;
        }
        for (const auto& i : getInstructions<false>()) {
            for (const auto& o : i.opcodes)
                jumpTable_normal[o.code] = o;
        }
        for (const auto& i : getInstructions<true>()) {
            for (const auto& o : i.opcodes)
                jumpTable_bcd[o.code] = o;
        }
        jumpTable = &jumpTable_normal[0];
    }

    POLICY& policy()
    {
        static POLICY policy(*this);
        return policy;
    }

    // Access ram directly

    const Word& Ram(const Adr& a) const { return ram[a]; }
    Word& Ram(const Adr& a) { return ram[a]; }

    const Word& Stack(const Word& a) const { return stack[a]; }

    void writeRam(uint16_t org, const Word data) { ram[org] = data; }

    void writeRam(uint16_t org, const uint8_t* data, int size)
    {
        for (int i = 0; i < size; i++)
            ram[org + i] = data[i];
    }

    void readRam(uint16_t org, uint8_t* data, int size) const
    {
        for (int i = 0; i < size; i++)
            data[i] = ram[org + i];
    }

    uint8_t readRam(uint16_t org) const { return ram[org]; }

    // Access memory through bank mapping

    uint8_t readMem(uint16_t org) const { return rbank[org >> 8][org & 0xff]; }

    void readMem(uint16_t org, uint8_t* data, int size) const
    {
        for (int i = 0; i < size; i++)
            data[i] = readMem(org + i);
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

    void mapReadCallback(uint8_t bank, int len,
                         uint8_t (*cb)(const Machine&, uint16_t a))
    {
        while (len > 0) {
            rcallbacks[bank++] = cb;
            len -= 256;
        }
    }
    void mapWriteCallback(uint8_t bank, int len,
                          void (*cb)(Machine&, uint16_t a, uint8_t v))
    {
        while (len > 0) {
            wcallbacks[bank++] = cb;
            len -= 256;
        }
    }

    uint8_t regA() const { return a; }
    uint8_t regX() const { return x; }
    uint8_t regY() const { return y; }
    uint8_t regSP() const { return sp; }
    Adr regPC() const { return pc; }

    uint8_t regSR() const { return sr; }

    void setPC(const int16_t& p) { pc = p; }

    uint32_t run(uint32_t runc = 0x01000000)
    {
        auto& p = policy();

        toCycles = cycles + runc;
        while (cycles < toCycles) {

            if (POLICY::eachOp(p)) break;
            auto code = ReadPC();
            auto& op = jumpTable[code];
            op.op(*this);
            cycles += op.cycles;
        }
        return 0;
    }


    auto regs() const { return std::make_tuple(a, x, y, sr, sp, pc); }
    auto regs() { return std::tie(a, x, y, sr, sp, pc); }

private:
    // The 6502 registers
    unsigned pc;
    unsigned a;
    unsigned x;
    unsigned y;
    unsigned sr;

    unsigned result;

    uint8_t sp;

    // 6502 RAM
    std::array<Word, POLICY::MemSize> ram;

    // Stack normally points to ram[0x100];
    Word* stack;

    // Banks normally point to corresponding ram
    std::array<const Word*, 256> rbank;
    std::array<Word*, 256> wbank;

    std::array<Word (*)(const Machine&, uint16_t), 256> rcallbacks;
    std::array<void (*)(Machine&, uint16_t, Word), 256> wcallbacks;

    static void write_bank(Machine& m, uint16_t adr, Word v)
    {
        m.wbank[adr >> 8][adr & 0xff] = v & 0xff;
    }

    static Word read_bank(const Machine& m, uint16_t adr)
    {
        return m.rbank[adr >> 8][adr & 0xff];
    }

    uint64_t toCycles;
    uint64_t cycles;

    std::array<Opcode, 256> jumpTable_normal;
    std::array<Opcode, 256> jumpTable_bcd;
    // Current jumptable
    const Opcode* jumpTable;

    template <int REG> constexpr auto& Reg() const
    {
        if constexpr (REG == A) return a;
        if constexpr (REG == X) return x;
        if constexpr (REG == Y) return y;
        if constexpr (REG == SP) return sp;
    }

    template <int REG> constexpr auto& Reg()
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
        d_FLAG = 0x8,
        V = 0x40,
        S = 0x80,
    };

    static constexpr auto SZ = S | Z;
    static constexpr auto SZC = S | Z | C;
    static constexpr auto SZCV = S | Z | C | V;

    uint8_t get_SR() const { 
        return sr  | ((result | (result>>2) ) & 0x80) | (!(result & 0xff) << 1) ;
    }

    template <bool DEC> void setDec()
    {
        if constexpr (DEC)
            jumpTable = &jumpTable_bcd[0];
        else
            jumpTable = &jumpTable_normal[0];
    }

    void set_SR(uint8_t s)
    {
        if ((s ^ sr) & d_FLAG) { // D bit changed
            if (s & d_FLAG) {
                setDec<true>();
            } else {
                setDec<false>();
            }
        }
        result = ((s << 2) & 0x200) | !(s & Z);
        sr = (s & ~SZ) | 0x30;
    }

    template <int BITS> void set(int res, int arg = 0)
    {
        if constexpr((BITS & (C|V)) != 0)
            sr &= ~BITS;

        result = res;

        if constexpr ((BITS & C) != 0) sr |= ((res >> 8) & 1); // Apply carry
        if constexpr ((BITS & V) != 0)
            sr |= ((~(a ^ arg) & (a ^ res) & 0x80) >> 1); // Apply overflow
    }

    template <int REG> void set_SZ()
    {
        result = Reg<REG>();
    }

    static constexpr bool SET = true;
    static constexpr bool CLEAR = false;

    constexpr unsigned carry() const
    {
        return sr & 1;
    }

    template <int FLAG, bool v> constexpr bool check() const
    {
        if constexpr (FLAG == ZERO) return result & 0xff ? !v : v;
        if constexpr (FLAG == SIGN) return result & 0x280 ? v : !v;
        else return (bool)(sr & (1 << FLAG)) == v;
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    /// MEMORY ACCESS
    ///
    /////////////////////////////////////////////////////////////////////////

    static constexpr unsigned lo(unsigned a) { return a & 0xff; }
    static constexpr unsigned hi(unsigned a) { return a >> 8; }
    static constexpr unsigned to_adr(unsigned lo, unsigned hi)
    {
        return (hi << 8) | lo;
    }

    template <int ACCESS_MODE = POLICY::Read_AccessMode>
    unsigned Read(unsigned adr) const
    {
        if constexpr (ACCESS_MODE == DIRECT)
            return ram[adr];
        else if constexpr (ACCESS_MODE == BANKED)
            return rbank[hi(adr)][lo(adr)];
        else
            return rcallbacks[hi(adr)](*this, adr);
    }

    template <int ACCESS_MODE = POLICY::Write_AccessMode>
    void Write(unsigned adr, unsigned v)
    {
        if constexpr (ACCESS_MODE == DIRECT)
            ram[adr] = v;
        else if constexpr (ACCESS_MODE == BANKED)
            wbank[hi(adr)][lo(adr)] = v;
        else
            wcallbacks[hi(adr)](*this, adr, v);
    }

    unsigned ReadPC() { return Read<POLICY::PC_AccessMode>(pc++); }

    unsigned ReadPC8(unsigned offs = 0)
    {
        return (Read<POLICY::PC_AccessMode>(pc++) + offs) & 0xff;
    }

    unsigned ReadPC16(unsigned offs = 0)
    {
        auto adr = to_adr(Read<POLICY::PC_AccessMode>(pc),
                          Read<POLICY::PC_AccessMode>(pc + 1));
        pc += 2;
        return adr + offs;
    }

    unsigned Read16(unsigned a, unsigned offs = 0) const
    {
        return to_adr(Read(a), Read(a + 1)) + offs;
    }

    // Read operand from PC and create effective adress depeding on 'MODE'
    template <int MODE> unsigned ReadEA()
    {
        if constexpr (MODE == ZP) return ReadPC8();
        if constexpr (MODE == ZPX) return ReadPC8(x);
        if constexpr (MODE == ZPY) return ReadPC8(y);
        if constexpr (MODE == ABS) return ReadPC16();
        if constexpr (MODE == ABSX) return ReadPC16(x);
        if constexpr (MODE == ABSY) return ReadPC16(y);
        if constexpr (MODE == INDX) return Read16(ReadPC8(x));
        if constexpr (MODE == INDY) return Read16(ReadPC8(), y);
        if constexpr (MODE == IND) return Read16(ReadPC16());
    }

    template <int MODE> inline void StoreEA(unsigned v)
    {
        auto adr = ReadEA<MODE>();
        Write(adr, v);
    }

    template <int MODE> unsigned LoadEA()
    {
        if constexpr (MODE == IMM)
            return ReadPC();
        else {
            unsigned adr = ReadEA<MODE>();
            return Read(adr);
        }
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   OPCODES
    ///
    /////////////////////////////////////////////////////////////////////////

    template <int FLAG, bool ON> static constexpr void Set(Machine& m)
    {
        if constexpr (FLAG == DECIMAL) m.setDec<ON>();
        m.sr = (m.sr & ~(1 << FLAG)) | (ON << FLAG);
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

    template <int FLAG, bool ON> static constexpr void Branch(Machine& m)
    {
        int8_t diff = m.ReadPC();
        auto d = m.check<FLAG, ON>();
        m.cycles += d;
        m.pc += (diff * d);
    }

    template <int MODE, int INC> static constexpr void Inc(Machine& m)
    {
        if constexpr (IsReg<MODE>()) {
            m.Reg<MODE>() = (m.Reg<MODE>() + INC) & 0xff;
            m.set_SZ<MODE>();
        } else {
            auto adr = m.ReadEA<MODE>();
            auto rc = (m.Read(adr) + INC);
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    // === COMPARE, ADD & SUBTRACT

    template <int REG, int MODE> static constexpr void Bit(Machine& m)
    {
        unsigned z = m.LoadEA<MODE>();
        m.result = (z & m.a) | ((z & 0x80)<<2);
        m.set_SR((m.get_SR() & 0x3d) | (z & 0xc0) | (!(z & m.a) << 1));
    }

    template <int REG, int MODE> static constexpr void Cmp(Machine& m)
    {
        unsigned z = (~m.LoadEA<MODE>()) & 0xff;
        unsigned rc = m.Reg<REG>() + z + 1;
        m.set<SZC>(rc);
    }

    template <int MODE, bool DEC = false> static constexpr void Sbc(Machine& m)
    {
        if constexpr (DEC) {
            unsigned z = m.LoadEA<MODE>();
            auto al = (m.a & 0xf) - (z & 0xf) + (m.carry() - 1);
            auto ah = (m.a >> 4) - (z >> 4);
            if (al & 0x10) {
                al = (al - 6) & 0xf;
                ah--;
            }
            if (ah & 0x10) ah = (ah - 6) & 0xf;
            unsigned rc = m.a - z + (m.carry() - 1);
            m.set<SZCV>(rc ^ 0x100, z);
            m.a = al | (ah << 4);
        } else {
            unsigned z = (~m.LoadEA<MODE>()) & 0xff;
            unsigned rc = m.a + z + m.carry();
            m.set<SZCV>(rc, z);
            m.a = rc & 0xff;
        }
    }

    template <int MODE, bool DEC = false> static constexpr void Adc(Machine& m)
    {
        unsigned z = m.LoadEA<MODE>();
        unsigned rc = m.a + z + m.carry();
        if constexpr (DEC) {
            if (((m.a & 0xf) + (z & 0xf) + m.carry()) >= 10) rc += 6;
            if ((rc & 0xff0) > 0x90) rc += 0x60;
        }
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
        if constexpr (MODE == A) {
            int rc = m.a << 1;
            m.set<SZC>(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr) << 1;
            m.set<SZC>(rc);
            m.Write(adr, rc);
        }
    }

    template <int MODE> static constexpr void Lsr(Machine& m)
    {
        if constexpr (MODE == A) {
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a >>= 1;
            m.set_SZ<A>();
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    template <int MODE> static constexpr void Ror(Machine& m)
    {
        if constexpr (MODE == A) {
            unsigned rc = (((m.sr << 8) & 0x100) | m.a) >> 1;
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a = rc & 0xff;
            m.set_SZ<A>();
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr) | ((m.sr << 8) & 0x100);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    template <int MODE> static constexpr void Rol(Machine& m)
    {
        if constexpr (MODE == A) {
            unsigned rc = (m.a << 1) | m.carry();
            m.set<SZC>(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = (m.Read(adr) << 1) | m.carry();
            m.Write(adr, rc);
            m.set<SZC>(rc);
        }
    }

    template <int FROM, int TO> static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != SP) m.set_SZ<TO>();
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   INSTRUCTION TABLE
    ///
    /////////////////////////////////////////////////////////////////////////

public:
    template <bool USE_BCD = false> static const auto& getInstructions()
    {
        static const  std::vector<Instruction> instructionTable = {

            { "nop", {{ 0xea, 2, NONE, [](Machine& ) {} }} },

            { "lda", {
                { 0xa9, 2, IMM, Load<A, IMM>},
                { 0xa5, 2, ZP, Load<A, ZP>},
                { 0xb5, 4, ZPX, Load<A, ZPX>},
                { 0xad, 4, ABS, Load<A, ABS>},
                { 0xbd, 4, ABSX, Load<A, ABSX>},
                { 0xb9, 4, ABSY, Load<A, ABSY>},
                { 0xa1, 6, INDX, Load<A, INDX>},
                { 0xb1, 5, INDY, Load<A, INDY>},
            } },

            { "ldx", {
                { 0xa2, 2, IMM, Load<X, IMM>},
                { 0xa6, 3, ZP, Load<X, ZP>},
                { 0xb6, 4, ZPY, Load<X, ZPY>},
                { 0xae, 4, ABS, Load<X, ABS>},
                { 0xbe, 4, ABSY, Load<X, ABSY>},
            } },

            { "ldy", {
                { 0xa0, 2, IMM, Load<Y, IMM>},
                { 0xa4, 3, ZP, Load<Y, ZP>},
                { 0xb4, 4, ZPX, Load<Y, ZPX>},
                { 0xac, 4, ABS, Load<Y, ABS>},
                { 0xbc, 4, ABSX, Load<Y, ABSX>},
            } },

            { "sta", {
                { 0x85, 3, ZP, Store<A, ZP>},
                { 0x95, 4, ZPX, Store<A, ZPX>},
                { 0x8d, 4, ABS, Store<A, ABS>},
                { 0x9d, 4, ABSX, Store<A, ABSX>},
                { 0x99, 4, ABSY, Store<A, ABSY>},
                { 0x81, 6, INDX, Store<A, INDX>},
                { 0x91, 5, INDY, Store<A, INDY>},
            } },

            { "stx", {
                { 0x86, 3, ZP, Store<X, ZP>},
                { 0x96, 4, ZPY, Store<X, ZPY>},
                { 0x8e, 4, ABS, Store<X, ABS>},
            } },

            { "sty", {
                { 0x84, 3, ZP, Store<Y, ZP>},
                { 0x94, 4, ZPX, Store<Y, ZPX>},
                { 0x8c, 4, ABS, Store<Y, ABS>},
            } },

            { "dec", {
                { 0xc6, 5, ZP, Inc<ZP, -1>},
                { 0xd6, 6, ZPX, Inc<ZPX, -1>},
                { 0xce, 6, ABS, Inc<ABS, -1>},
                { 0xde, 7, ABSX, Inc<ABSX, -1>},
            } },

            { "inc", {
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
                { 0x69, 2, IMM, Adc<IMM, USE_BCD>},
                { 0x65, 3, ZP, Adc<ZP, USE_BCD>},
                { 0x75, 4, ZPX, Adc<ZPX, USE_BCD>},
                { 0x6d, 4, ABS, Adc<ABS, USE_BCD>},
                { 0x7d, 4, ABSX, Adc<ABSX, USE_BCD>},
                { 0x79, 4, ABSY, Adc<ABSY, USE_BCD>},
                { 0x61, 6, INDX, Adc<INDX, USE_BCD>},
                { 0x71, 5, INDY, Adc<INDY, USE_BCD>},
            } },

            { "sbc", {
                { 0xe9, 2, IMM, Sbc<IMM, USE_BCD>},
                { 0xe5, 3, ZP, Sbc<ZP, USE_BCD>},
                { 0xf5, 4, ZPX, Sbc<ZPX, USE_BCD>},
                { 0xed, 4, ABS, Sbc<ABS, USE_BCD>},
                { 0xfd, 4, ABSX, Sbc<ABSX, USE_BCD>},
                { 0xf9, 4, ABSY, Sbc<ABSY, USE_BCD>},
                { 0xe1, 6, INDX, Sbc<INDX, USE_BCD>},
                { 0xf1, 5, INDY, Sbc<INDY, USE_BCD>},
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
                { 0x4a, 2, NONE, Lsr<A>},
                { 0x4a, 2, ACC, Lsr<A>},
                { 0x46, 5, ZP, Lsr<ZP>},
                { 0x56, 6, ZPX, Lsr<ZPX>},
                { 0x4e, 6, ABS, Lsr<ABS>},
                { 0x5e, 7, ABSX, Lsr<ABSX>},
            } },

            { "asl", {
                { 0x0a, 2, NONE, Asl<A>},
                { 0x0a, 2, ACC, Asl<A>},
                { 0x06, 5, ZP, Asl<ZP>},
                { 0x16, 6, ZPX, Asl<ZPX>},
                { 0x0e, 6, ABS, Asl<ABS>},
                { 0x1e, 7, ABSX, Asl<ABSX>},
            } },

            { "ror", {
                { 0x6a, 2, NONE, Ror<A>},
                { 0x6a, 2, ACC, Ror<A>},
                { 0x66, 5, ZP, Ror<ZP>},
                { 0x76, 6, ZPX, Ror<ZPX>},
                { 0x6e, 6, ABS, Ror<ABS>},
                { 0x7e, 7, ABSX, Ror<ABSX>},
            } },

            { "rol", {
                { 0x2a, 2, NONE, Rol<A>},
                { 0x2a, 2, ACC, Rol<A>},
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
                    m.set_SR(m.stack[m.sp+1]);
                    m.pc = (m.stack[m.sp+2] | (m.stack[m.sp+3]<<8));
                    m.sp += 3;
                } }
            } },

            { "brk", {
                { 0x00, 7, NONE, [](Machine& m) {
                    m.ReadPC();
                    m.stack[m.sp] = m.pc >> 8;
                    m.stack[m.sp-1] = m.pc & 0xff;
                    m.stack[m.sp-2] = m.get_SR();// | (1<<BRK);
                    m.sp -= 3;
                    m.pc = m.Read16(m.to_adr(0xfe, 0xff));
                } }
            } },

            { "rts", {
                { 0x60, 6, NONE, [](Machine& m) {
                    if constexpr (POLICY::ExitOnStackWrap) {
                        if (m.sp == 0xff) {
                            m.cycles = m.toCycles;
                            return;
                        }
                    }
                    m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1;
                    m.sp += 2;
                } }
            } },

            { "jmp", {
                { 0x4c, 3, ABS, [](Machine& m) {
                    m.pc = m.ReadPC16();
                } },
                { 0x6c, 5, IND, [](Machine& m) {
                    m.pc = m.Read16(m.ReadPC16());
                } }
            } },

            { "jsr", {
                { 0x20, 6, ABS, [](Machine& m) {
                    m.stack[m.sp] = (m.pc+1) >> 8;
                    m.stack[m.sp-1] = (m.pc+1) & 0xff;
                    m.sp -= 2;
                    m.pc = m.ReadPC16();
                } }
            } },

        };
        return instructionTable;
    }
};
} // namespace sixfive
