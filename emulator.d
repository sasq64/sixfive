
// Adressing modes
enum 
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
    ABSY,
}


enum 
{
    DIRECT,  // Access `ram` array directly; Means no bank switching, ROM areas
             // or IO areas
    BANKED,  // Access memory through `wbank` and `rbank`; Means no IO areas
    CALLBACK // Access banks via function pointer, behavour can be customized
}

// The Policy defines the compile time settings for the emulator
struct DefaultPolicy
{
    // Must be convertable and constructable from uin16_t
    // lo() and hi() functions must extract low and high byte
    alias AdrType = ushort;
    static const bool ExitOnStackWrap = true;

    // PC accesses does not normally need to work in IO areas
    static const int PC_AccessMode = BANKED;

    // Generic reads and writes should normally not be direct
    static const int Read_AccessMode = CALLBACK;
    static const int Write_AccessMode = CALLBACK;

    static const bool Debug = false;
    static const int MemSize = 65536;

    // This function is run after each opcode. Return true to stop emulation.
    static bool eachOp() { return false; }
}

class Machine(POLICY = DefaultPolicy)
{
    enum
    {
        A = 20,
        X,
        Y,
        SP,
        SR
    }

    const static bool IsReg(MODE)() { return MODE >= A; }

    enum
    {
        CARRY,
        ZERO,
        IRQ,
        DECIMAL,
        BRK,
        xXx,
        OVER,
        SIGN
    }

    alias Adr = POLICY.AdrType;
    alias Word = ubyte;
    alias Flags = ubyte;

    alias OpFunc = void function(Machine);

    struct Opcode
    {
        ubyte code;
        ubyte cycles;
        ubyte mode;
        OpFunc op;
    }

    struct Instruction
    {
        string name;
        Opcode[] opcodes;
    }

    this()
    {
        sp = 0xff;
        stack = &ram[0x100];
        cycles = 0;
        a = x = y = 0;
        sr = 0x30;
        toCycles = 0;
        for (int i = 0; i < 256; i++) {
            rbank[i] = wbank[i] = &ram[(i * 256) % POLICY.MemSize];
            rcallbacks[i] = &read_bank;
            wcallbacks[i] = &write_bank;
        }
        foreach (ref i ; getInstructions!false()) {
            foreach (ref o ; i.opcodes)
                jumpTable_normal[o.code] = o;
        }
        foreach (ref i ; getInstructions!true()) {
            foreach (ref o ; i.opcodes)
                jumpTable_bcd[o.code] = o;
        }
        jumpTable = &jumpTable_normal[0];
    }

    ref POLICY policy()
    {
        static POLICY policy(this);
        return policy;
    }

    // Access ram directly

    ref Word Ram(Adr a) { return ram[a]; }

    ref Word Stack(ubyte a) const { return stack[a]; }

    void writeRam(ushort org, Word data) { ram[org] = data; }

    /* void writeRam(ushort org, const void* data, int size) */
    /* { */
    /*     auto* data8 = (ubyte*)data; */
    /*     for (int i = 0; i < size; i++) */
    /*         ram[org + i] = data8[i]; */
    /* } */

    /* void readRam(ushort org, void* data, int size) const */
    /* { */
    /*     auto* data8 = (Word*)data; */
    /*     for (int i = 0; i < size; i++) */
    /*         data8[i] = ram[org + i]; */
    /* } */

    Word readRam(ushort org) const { return ram[org]; }

    // Access memory through bank mapping

    Word readMem(ushort org) const { return rbank[org >> 8][org & 0xff]; }

/*     void readMem(ushort org, void* data, int size) const */
/*     { */
/*         auto* data8 = (Word*)data; */
/*         for (int i = 0; i < size; i++) */
/*             data8[i] = readMem(org + i); */
/*     } */

/*     // Map ROM to a bank */
/*     void mapRom(ubyte bank, const Word* data, int len) */
/*     { */
/*         auto end = data + len; */
/*         while (data < end) { */
/*             rbank[bank++] = const_cast<Word*>(data); */
/*             data += 256; */
/*         } */
/*     } */

/*     void mapReadCallback(ubyte bank, int len, */
/*                           ubyte (*cb)(const Machine, ushort a)) */
/*     { */
/*         while (len > 0) { */
/*             rcallbacks[bank++] = cb; */
/*             len -= 256; */
/*         } */
/*     } */
/*     void mapWriteCallback(ubyte bank, int len, */
/*                           void (*cb)(Machine, ushort a, ubyte v)) */
/*     { */
/*         while (len > 0) { */
/*             wcallbacks[bank++] = cb; */
/*             len -= 256; */
/*         } */
/*     } */

    Word regA() const { return a; }
    Word regX() const { return x; }
    Word regY() const { return y; }
    ubyte regSP() const { return sp; }
    Adr regPC() const { return pc; }

    ubyte regSR() const { return sr; }

    void setPC(ushort p) { pc = p; }

    uint run(uint runc = 0x01000000)
    {

        toCycles = cycles + runc;
        uint opcodes = 0;
        while (cycles < toCycles) {

            //if (POLICY::eachOp(*this)) break;

            auto code = ReadPC();
            static if (POLICY.ExitOnStackWrap) {
                if (code == 0x60 && cast(ubyte)sp == 0xff) return opcodes;
            }
            auto ref op = jumpTable[code];
            op.op(this);
            cycles += op.cycles;
            opcodes++;
        }
        return opcodes;
    }

    /* auto regs() const { return std::make_tuple(a, x, y, sr, sp, pc); } */
    /* auto regs() { return std::tie(a, x, y, sr, sp, pc); } */

private:
    // The 6502 registers
    Adr pc;
    uint a;
    uint x;
    uint y;
    uint sr;
    ubyte sp;

    // 6502 RAM
	Word[POLICY.MemSize] ram;

    // Stack normally points to ram[0x100];
    Word* stack;

    // Banks normally point to corresponding ram
	const(Word)*[256] rbank;
	Word*[256] wbank;

	ubyte function(const Machine, uint)[256] rcallbacks;
	void function(Machine, uint, ubyte)[256] wcallbacks;

    static void write_bank(Machine m, uint adr, ubyte v)
    {
        m.wbank[adr >> 8][adr & 0xff] = v;
    }

    static ubyte read_bank(const Machine m, uint adr)
    {
        return m.rbank[adr >> 8][adr & 0xff];
    }

    uint toCycles;
    uint cycles;

    Opcode[256] jumpTable_normal;
    Opcode[256] jumpTable_bcd;
    // Current jumptable
    const Opcode* jumpTable;

	auto ref Reg(int REG)() {
    	static if(REG == A) return a;
    	static if(REG == X) return x;
    	static if(REG == Y) return y;
    	static if(REG == SP) return sp;
	}
    /////////////////////////////////////////////////////////////////////////
    ///
    /// THE STATUS REGISTER
    ///
    /////////////////////////////////////////////////////////////////////////

    // S V - b d i Z C
    // 1 1 0 0 0 0 1 1

    enum
    {
        C = 0x1,
        Z = 0x2,
        d_FLAG = 0x8,
        V = 0x40,
        S = 0x80,
    }

    static const auto SZ = S | Z;
    static const auto SZC = S | Z | C;
    static const auto SZCV = S | Z | C | V;

    ubyte get_SR() const { return sr; }

    ubyte lastSR = 0; // Only for the D bit

    void setDec(bool DEC)()
    {
        static if (DEC)
            jumpTable = &jumpTable_bcd[0];
        else
            jumpTable = &jumpTable_normal[0];
    }

    void set_SR(ubyte s)
    {
        if ((s ^ sr) & d_FLAG) { // D bit changed
            if (s & d_FLAG) {
                setDec!true();
            } else {
                setDec!false();
            }
        }
        sr = s | 0x30;
    }

    void set(int BITS)(int res, int arg = 0)
    {
        sr &= ~BITS;

        static if ((BITS & S) != 0) sr |= (res & 0x80); // Apply signed bit
        static if ((BITS & Z) != 0) sr |= (!(res & 0xff) << 1); // Apply zero
        static if ((BITS & C) != 0) sr |= ((res >> 8) & 1); // Apply carry
        static if ((BITS & V) != 0)
            sr |= ((~(a ^ arg) & (a ^ res) & 0x80) >> 1); // Apply overflow
    }

    void set_SZ(int REG)()
    {
        sr = (sr & ~SZ) | (Reg!REG() & 0x80) | (!Reg!REG() << 1);
    }

    int check(int FLAG)() const { return sr & (1 << FLAG); }

    static const bool SET = true;
    static const bool CLEAR = false;

    bool check(int FLAG, bool v)() const
    {
        return cast(bool)(sr & (1 << FLAG)) == v;
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    /// MEMORY ACCESS
    ///
    /////////////////////////////////////////////////////////////////////////

    static  Word lo(uint a) { return a & 0xff; }
    static  Word hi(uint a) { return (a >> 8) & 0xff; }
    static  Adr to_adr(Word lo, Word hi) { return (hi << 8) | lo; }

    Word Read(int ACCESS_MODE = POLICY.Read_AccessMode)(uint adr) const
    {
        static if (ACCESS_MODE == DIRECT)
            return ram[adr];
        else static if (ACCESS_MODE == BANKED)
            return rbank[hi(adr)][lo(adr)];
        else
            return rcallbacks[hi(adr)](this, adr);
    }

    void Write(int ACCESS_MODE = POLICY.Write_AccessMode)(ushort adr, Word v)
    {
        static if (ACCESS_MODE == DIRECT)
            ram[adr] = v;
        else static if (ACCESS_MODE == BANKED)
            wbank[hi(adr)][lo(adr)] = v;
        else
            wcallbacks[hi(adr)](this, adr, v);
    }

    Word ReadPC() { return Read!(POLICY.PC_AccessMode)(pc++); }

    Adr ReadPC8(int offs = 0)
    {
        return (Read!(POLICY.PC_AccessMode)(pc++) + offs) & 0xff;
    }

    Adr ReadPC16(int offs = 0)
    {
        auto adr = to_adr(Read!(POLICY.PC_AccessMode)(pc),
                          Read!(POLICY.PC_AccessMode)(pc + 1));
        pc += 2;
        return cast(Adr)(adr + offs);
    }

    Adr Read16(int a, int offs = 0) const
    {
        return cast(Adr)(to_adr(Read(a), Read(a + 1)) + offs);
    }

    // Read operand from PC and create effective adress depeding on 'MODE'
    Adr ReadEA(int MODE)()
    {
        static if (MODE == ZP) return ReadPC8();
        static if (MODE == ZPX) return ReadPC8(x);
        static if (MODE == ZPY) return ReadPC8(y);
        static if (MODE == ABS) return ReadPC16();
        static if (MODE == ABSX) return ReadPC16(x);
        static if (MODE == ABSY) return ReadPC16(y);
        static if (MODE == INDX) return Read16(ReadPC8(x));
        static if (MODE == INDY) return Read16(ReadPC8(), y);
        static if (MODE == IND) return Read16(ReadPC16()); // TODO: ZP wrap?
    }

    void StoreEA(int MODE)(Word v)
    {
        auto adr = ReadEA!MODE();
        Write(adr, v);
    }

    Word LoadEA(int MODE)()
    {
        static if (MODE == IMM)
            return ReadPC();
        else {
            Adr adr = ReadEA!MODE();
            return Read(adr);
        }
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   OPCODES
    ///
    /////////////////////////////////////////////////////////////////////////

    static void Set(int FLAG, bool v)(Machine m)
    {
        static if (FLAG == DECIMAL) m.setDec!v();
        m.sr = (m.sr & ~(1 << FLAG)) | (v << FLAG);
    }

    static void Store(int REG, int MODE)(Machine m)
    {
        m.StoreEA!MODE(m.Reg!REG());
    }

    static void Load(int REG, int MODE)(Machine m)
    {
        m.Reg!REG() = m.LoadEA!MODE();
        m.set_SZ!REG();
    }

    static void Branch(int FLAG, bool v)(Machine m)
    {
        int8_t diff = m.ReadPC();
        int d = m.check!(FLAG, v)();
        m.cycles += d;
        m.pc += (diff * d);
    }

    static void Inc(int MODE, int inc)(Machine m)
    {
        static if (IsReg!MODE()) {
            m.Reg!MODE() = (m.Reg!MODE() + inc) & 0xff;
            m.set_SZ!MODE();
        } else {
            auto adr = m.ReadEA!MODE();
            auto rc = (m.Read(adr) + inc);
            m.Write(adr, rc);
            m.set!SZ(rc);
        }
    }

    // === COMPARE, ADD & SUBTRACT

    static void Bit(int REG, int MODE)(Machine m)
    {
        Word z = m.LoadEA!MODE();
        m.set_SR((m.get_SR() & 0x3d) | (z & 0xc0) | (!(z & m.a) << 1));
    }

    static void Cmp(int REG, int MODE)(Machine m)
    {
        Word z = (~m.LoadEA!MODE()) & 0xff;
        uint rc = m.Reg!REG() + z + 1;
        m.set!SZC(rc);
    }

    static void Sbc(int MODE, bool DEC = false)(Machine m)
    {
        static if (DEC) {
            uint z = m.LoadEA!MODE();
            auto al = (m.a & 0xf) - (z & 0xf) + (m.check!CARRY() - 1);
            auto ah = (m.a >> 4) - (z >> 4);
            if (al & 0x10) {
                al = (al - 6) & 0xf;
                ah--;
            }
            if (ah & 0x10) ah = (ah - 6) & 0xf;
            uint rc = m.a - z + (m.check!CARRY() - 1);
            m.set!SZCV(rc ^ 0x100, z);
            m.a = al | (ah << 4);
        } else {
            uint z = (~m.LoadEA!MODE()) & 0xff;
            uint rc = m.a + z + m.check!CARRY();
            m.set!SZCV(rc, z);
            m.a = rc & 0xff;
        }
    }

    static void Adc(int MODE, bool DEC = false)(Machine m)
    {
        uint z = m.LoadEA!MODE();
        uint rc = m.a + z + m.check!CARRY();
        static if (DEC) {
            if (((m.a & 0xf) + (z & 0xf) + m.check!CARRY()) >= 10) rc += 6;
            if ((rc & 0xff0) > 0x90) rc += 0x60;
        }
        m.set!SZCV(rc, z);
        m.a = rc & 0xff;
    }

    static void And(int MODE)(Machine m)
    {
        m.a &= m.LoadEA!MODE();
        m.set_SZ!A();
    }

    static void Ora(int MODE)(Machine m)
    {
        m.a |= m.LoadEA!MODE();
        m.set_SZ!A();
    }

    static void Eor(int MODE)(Machine m)
    {
        m.a ^= m.LoadEA!MODE();
        m.set_SZ!A();
    }

    // === SHIFTS & ROTATES

    static void Asl(int MODE)(Machine m)
    {
        static if (MODE == A) {
            int rc = m.a << 1;
            m.set!SZC(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA!MODE();
            int rc = m.Read(adr) << 1;
            m.set!SZC(rc);
            m.Write(adr, rc);
        }
    }

    static void Lsr(int MODE)(Machine m)
    {
        static if (MODE == A) {
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a >>= 1;
            m.set_SZ!A();
        } else {
            auto adr = m.ReadEA!MODE();
            uint rc = m.Read(adr);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set!SZ(rc);
        }
    }

    static void Ror(int MODE)(Machine m)
    {
        static if (MODE == A) {
            uint rc = ((m.sr << 8) | m.a) >> 1;
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a = rc & 0xff;
            m.set_SZ!A();
        } else {
            auto adr = m.ReadEA!MODE();
            uint rc = m.Read(adr) | (m.sr << 8);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set!SZ(rc);
        }
    }

    static void Rol(int MODE)(Machine m)
    {
        static if (MODE == A) {
            uint rc = (m.a << 1) | m.check!CARRY();
            m.set!SZC(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA!MODE();
            uint rc = (m.Read(adr) << 1) | m.check!CARRY();
            m.Write(adr, rc);
            m.set!SZC(rc);
        }
    }

    static void Transfer(int FROM, int TO)(Machine m)
    {
        m.Reg!TO() = m.Reg!FROM();
        static if (TO != SP) m.set_SZ!TO();
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   INSTRUCTION TABLE
    ///
    /////////////////////////////////////////////////////////////////////////

public:
    static Instruction[] getInstructions(bool USE_BCD)()
    {
        static const Instruction[] instructionTable = [

            Instruction("nop", [ Opcode( 0xea, 2, NONE, function(Machine) {} ) ] ),

            Instruction("lda", [
                Opcode(0xa9, 2, IMM, &Load!(A, IMM)),
                Opcode(0xa5, 2, ZP, &Load!(A, ZP)),
                Opcode(0xb5, 4, ZPX, &Load!(A, ZPX)),
                Opcode(0xad, 4, ABS, &Load!(A, ABS)),
                Opcode(0xbd, 4, ABSX, &Load!(A, ABSX)),
                Opcode(0xb9, 4, ABSY, &Load!(A, ABSY)),
                Opcode(0xa1, 6, INDX, &Load!(A, INDX)),
                Opcode(0xb1, 5, INDY, &Load!(A, INDY)),
            ] ),

            Instruction("ldx", [
                Opcode(0xa2, 2, IMM, &Load!(X, IMM)),
                Opcode(0xa6, 3, ZP, &Load!(X, ZP)),
                Opcode(0xb6, 4, ZPY, &Load!(X, ZPY)),
                Opcode(0xae, 4, ABS, &Load!(X, ABS)),
                Opcode(0xbe, 4, ABSY, &Load!(X, ABSY)),
            ] ),

            Instruction("ldy", [
                Opcode(0xa0, 2, IMM, &Load!(Y, IMM)),
                Opcode(0xa4, 3, ZP, &Load!(Y, ZP)),
                Opcode(0xb4, 4, ZPX, &Load!(Y, ZPX)),
                Opcode(0xac, 4, ABS, &Load!(Y, ABS)),
                Opcode(0xbc, 4, ABSX, &Load!(Y, ABSX)),
            ] ),

            Instruction("sta", [
                Opcode(0x85, 3, ZP, &Store!(A, ZP)),
                Opcode(0x95, 4, ZPX, &Store!(A, ZPX)),
                Opcode(0x8d, 4, ABS, &Store!(A, ABS)),
                Opcode(0x9d, 4, ABSX, &Store!(A, ABSX)),
                Opcode(0x99, 4, ABSY, &Store!(A, ABSY)),
                Opcode(0x81, 6, INDX, &Store!(A, INDX)),
                Opcode(0x91, 5, INDY, &Store!(A, INDY)),
            ] ),

            Instruction("stx", [
                Opcode(0x86, 3, ZP, &Store!(X, ZP)),
                Opcode(0x96, 4, ZPY, &Store!(X, ZPY)),
                Opcode(0x8e, 4, ABS, &Store!(X, ABS)),
            ] ),

            Instruction("sty", [
                Opcode(0x84, 3, ZP, &Store!(Y, ZP)),
                Opcode(0x94, 4, ZPX, &Store!(Y, ZPX)),
                Opcode(0x8c, 4, ABS, &Store!(Y, ABS)),
            ] ),

            Instruction("dec", [
                Opcode(0xc6, 5, ZP, &Inc!(ZP, -1)),
                Opcode(0xd6, 6, ZPX, &Inc!(ZPX, -1)),
                Opcode(0xce, 6, ABS, &Inc!(ABS, -1)),
                Opcode(0xde, 7, ABSX, &Inc!(ABSX, -1)),
            ] ),

            Instruction("inc", [
                Opcode(0xe6, 5, ZP, &Inc!(ZP, 1)),
                Opcode(0xf6, 6, ZPX, &Inc!(ZPX, 1)),
                Opcode(0xee, 6, ABS, &Inc!(ABS, 1)),
                Opcode(0xfe, 7, ABSX, &Inc!(ABSX, 1)),
            ] ),

            Instruction("tax", [ Opcode(0xaa, 2, NONE, &Transfer!(A, X) ) ] ),
            Instruction("txa", [ Opcode(0x8a, 2, NONE, &Transfer!(X, A) ) ] ),
            Instruction("tay", [ Opcode(0xa8, 2, NONE, &Transfer!(A, Y) ) ] ),
            Instruction("tya", [ Opcode(0x98, 2, NONE, &Transfer!(Y, A) ) ] ),
            Instruction("txs", [ Opcode(0x9a, 2, NONE, &Transfer!(X, SP) ) ] ),
            Instruction("tsx", [ Opcode(0xba, 2, NONE, &Transfer!(SP, X) ) ] ),

            Instruction("dex", [ Opcode(0xca, 2, NONE, &Inc!(X, -1) ) ] ),
            Instruction("inx", [ Opcode(0xe8, 2, NONE, &Inc!(X, 1) ) ] ),
            Instruction("dey", [ Opcode(0x88, 2, NONE, &Inc!(Y, -1) ) ] ),
            Instruction("iny", [ Opcode(0xc8, 2, NONE, &Inc!(Y, 1) ) ] )

            /* Instruction("pha", { Opcode(0x48, 3, NONE, [](Machine m) { */
            /*     m.stack[m.sp--] = m.a; */
            /* } } ] ), */

            /* Instruction("pla", { Opcode(0x68, 4, NONE, [](Machine m) { */
            /*     m.a = m.stack[++m.sp]; */
            /* } } ] ), */

            /* Instruction("php", { Opcode(0x08, 3, NONE, [](Machine m) { */
            /*     m.stack[m.sp--] = m.get_SR(); */
            /* } } ] ), */

            /* Instruction("plp", { Opcode(0x28, 4, NONE, [](Machine m) { */
            /*     m.set_SR(m.stack[++m.sp]); */
            /* } } ] ), */

            /* Instruction("bcc", { Opcode(0x90, 2, REL, &Branch!(CARRY, CLEAR) }, ] ), */
            /* Instruction("bcs", { Opcode(0xb0, 2, REL, &Branch!(CARRY, SET) }, ] ), */
            /* Instruction("bne", { Opcode(0xd0, 2, REL, &Branch!(ZERO, CLEAR) }, ] ), */
            /* Instruction("beq", { Opcode(0xf0, 2, REL, &Branch!(ZERO, SET) }, ] ), */
            /* Instruction("bpl", { Opcode(0x10, 2, REL, &Branch!(SIGN, CLEAR) }, ] ), */
            /* Instruction("bmi", { Opcode(0x30, 2, REL, &Branch!(SIGN, SET) }, ] ), */
            /* Instruction("bvc", { Opcode(0x50, 2, REL, &Branch!(OVER, CLEAR) }, ] ), */
            /* Instruction("bvs", { Opcode(0x70, 2, REL, &Branch!(OVER, SET) }, ] ), */

            /* Instruction("adc", { */
            /*     Opcode(0x69, 2, IMM, &Adc!(IMM, USE_BCD)}, */
            /*     Opcode(0x65, 3, ZP, &Adc!(ZP, USE_BCD)}, */
            /*     Opcode(0x75, 4, ZPX, &Adc!(ZPX, USE_BCD)}, */
            /*     Opcode(0x6d, 4, ABS, &Adc!(ABS, USE_BCD)}, */
            /*     Opcode(0x7d, 4, ABSX, &Adc!(ABSX, USE_BCD)}, */
            /*     Opcode(0x79, 4, ABSY, &Adc!(ABSY, USE_BCD)}, */
            /*     Opcode(0x61, 6, INDX, &Adc!(INDX, USE_BCD)}, */
            /*     Opcode(0x71, 5, INDY, &Adc!(INDY, USE_BCD)}, */
            /* ] ), */

            /* Instruction("sbc", { */
            /*     Opcode(0xe9, 2, IMM, &Sbc!(IMM, USE_BCD)}, */
            /*     Opcode(0xe5, 3, ZP, &Sbc!(ZP, USE_BCD)}, */
            /*     Opcode(0xf5, 4, ZPX, &Sbc!(ZPX, USE_BCD)}, */
            /*     Opcode(0xed, 4, ABS, &Sbc!(ABS, USE_BCD)}, */
            /*     Opcode(0xfd, 4, ABSX, &Sbc!(ABSX, USE_BCD)}, */
            /*     Opcode(0xf9, 4, ABSY, &Sbc!(ABSY, USE_BCD)}, */
            /*     Opcode(0xe1, 6, INDX, &Sbc!(INDX, USE_BCD)}, */
            /*     Opcode(0xf1, 5, INDY, &Sbc!(INDY, USE_BCD)}, */
            /* ] ), */

            /* Instruction("cmp", { */
            /*     Opcode(0xc9, 2, IMM, &Cmp!(A, IMM)}, */
            /*     Opcode(0xc5, 3, ZP, &Cmp!(A, ZP)}, */
            /*     Opcode(0xd5, 4, ZPX, &Cmp!(A, ZPX)}, */
            /*     Opcode(0xcd, 4, ABS, &Cmp!(A, ABS)}, */
            /*     Opcode(0xdd, 4, ABSX, &Cmp!(A, ABSX)}, */
            /*     Opcode(0xd9, 4, ABSY, &Cmp!(A, ABSY)}, */
            /*     Opcode(0xc1, 6, INDX, &Cmp!(A, INDX)}, */
            /*     Opcode(0xd1, 5, INDY, &Cmp!(A, INDY)}, */
            /* ] ), */

            /* Instruction("cpx", { */
            /*     Opcode(0xe0, 2, IMM, &Cmp!(X, IMM)}, */
            /*     Opcode(0xe4, 3, ZP, &Cmp!(X, ZP)}, */
            /*     Opcode(0xec, 4, ABS, &Cmp!(X, ABS)}, */
            /* ] ), */

            /* Instruction("cpy", { */
            /*     Opcode(0xc0, 2, IMM, &Cmp!(Y, IMM)}, */
            /*     Opcode(0xc4, 3, ZP, &Cmp!(Y, ZP)}, */
            /*     Opcode(0xcc, 4, ABS, &Cmp!(Y, ABS)}, */
            /* ] ), */

            /* Instruction("and", { */
            /*     Opcode(0x29, 2, IMM, &And!(IMM)}, */
            /*     Opcode(0x25, 3, ZP, &And!(ZP)}, */
            /*     Opcode(0x35, 4, ZPX, &And!(ZPX)}, */
            /*     Opcode(0x2d, 4, ABS, &And!(ABS)}, */
            /*     Opcode(0x3d, 4, ABSX, &And!(ABSX)}, */
            /*     Opcode(0x39, 4, ABSY, &And!(ABSY)}, */
            /*     Opcode(0x21, 6, INDX, &And!(INDX)}, */
            /*     Opcode(0x31, 5, INDY, &And!(INDY)}, */
            /* ] ), */

            /* Instruction("eor", { */
            /*     Opcode(0x49, 2, IMM, &Eor!(IMM)}, */
            /*     Opcode(0x45, 3, ZP, &Eor!(ZP)}, */
            /*     Opcode(0x55, 4, ZPX, &Eor!(ZPX)}, */
            /*     Opcode(0x4d, 4, ABS, &Eor!(ABS)}, */
            /*     Opcode(0x5d, 4, ABSX, &Eor!(ABSX)}, */
            /*     Opcode(0x59, 4, ABSY, &Eor!(ABSY)}, */
            /*     Opcode(0x41, 6, INDX, &Eor!(INDX)}, */
            /*     Opcode(0x51, 5, INDY, &Eor!(INDY)}, */
            /* ] ), */

            /* Instruction("ora", { */
            /*     Opcode(0x09, 2, IMM, &Ora!(IMM)}, */
            /*     Opcode(0x05, 3, ZP, &Ora!(ZP)}, */
            /*     Opcode(0x15, 4, ZPX, &Ora!(ZPX)}, */
            /*     Opcode(0x0d, 4, ABS, &Ora!(ABS)}, */
            /*     Opcode(0x1d, 4, ABSX, &Ora!(ABSX)}, */
            /*     Opcode(0x19, 4, ABSY, &Ora!(ABSY)}, */
            /*     Opcode(0x01, 6, INDX, &Ora!(INDX)}, */
            /*     Opcode(0x11, 5, INDY, &Ora!(INDY)}, */
            /* ] ), */

            /* Instruction("sec", { Opcode(0x38, 2, NONE, &Set!(CARRY, true) } ] ), */
            /* Instruction("clc", { Opcode(0x18, 2, NONE, &Set!(CARRY, false) } ] ), */
            /* Instruction("sei", { Opcode(0x58, 2, NONE, &Set!(IRQ, true) } ] ), */
            /* Instruction("cli", { Opcode(0x78, 2, NONE, &Set!(IRQ, false) } ] ), */
            /* Instruction("sed", { Opcode(0xf8, 2, NONE, &Set!(DECIMAL, true) } ] ), */
            /* Instruction("cld", { Opcode(0xd8, 2, NONE, &Set!(DECIMAL, false) } ] ), */
            /* Instruction("clv", { Opcode(0xb8, 2, NONE, &Set!(OVER, false) } ] ), */

            /* Instruction("lsr", { */
            /*     Opcode(0x4a, 2, NONE, &Lsr!(A)}, */
            /*     Opcode(0x4a, 2, ACC, &Lsr!(A)}, */
            /*     Opcode(0x46, 5, ZP, &Lsr!(ZP)}, */
            /*     Opcode(0x56, 6, ZPX, &Lsr!(ZPX)}, */
            /*     Opcode(0x4e, 6, ABS, &Lsr!(ABS)}, */
            /*     Opcode(0x5e, 7, ABSX, &Lsr!(ABSX)}, */
            /* ] ), */

            /* Instruction("asl", { */
            /*     Opcode(0x0a, 2, NONE, &Asl!(A)}, */
            /*     Opcode(0x0a, 2, ACC, &Asl!(A)}, */
            /*     Opcode(0x06, 5, ZP, &Asl!(ZP)}, */
            /*     Opcode(0x16, 6, ZPX, &Asl!(ZPX)}, */
            /*     Opcode(0x0e, 6, ABS, &Asl!(ABS)}, */
            /*     Opcode(0x1e, 7, ABSX, &Asl!(ABSX)}, */
            /* ] ), */

            /* Instruction("ror", { */
            /*     Opcode(0x6a, 2, NONE, &Ror!(A)}, */
            /*     Opcode(0x6a, 2, ACC, &Ror!(A)}, */
            /*     Opcode(0x66, 5, ZP, &Ror!(ZP)}, */
            /*     Opcode(0x76, 6, ZPX, &Ror!(ZPX)}, */
            /*     Opcode(0x6e, 6, ABS, &Ror!(ABS)}, */
            /*     Opcode(0x7e, 7, ABSX, &Ror!(ABSX)}, */
            /* ] ), */

            /* Instruction("rol", { */
            /*     Opcode(0x2a, 2, NONE, &Rol!(A)}, */
            /*     Opcode(0x2a, 2, ACC, &Rol!(A)}, */
            /*     Opcode(0x26, 5, ZP, &Rol!(ZP)}, */
            /*     Opcode(0x36, 6, ZPX, &Rol!(ZPX)}, */
            /*     Opcode(0x2e, 6, ABS, &Rol!(ABS)}, */
            /*     Opcode(0x3e, 7, ABSX, &Rol!(ABSX)}, */
            /* ] ), */

            /* Instruction("bit", { */
            /*     Opcode(0x24, 3, ZP, &Bit!(X, ZP)}, */
            /*     Opcode(0x2c, 4, ABS, &Bit!(X, ABS)}, */
            /* ] ), */

            /* Instruction("rti", { */
            /*     Opcode(0x40, 6, NONE, [](Machine m) { */
            /*         m.set_SR(m.stack[++m.sp]);// & !(1<<BRK)); */
            /*         m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8)); */
            /*         m.sp += 2; */
            /*     } } */
            /* ] ), */

            /* Instruction("brk", { */
            /*     Opcode(0x00, 7, NONE, [](Machine m) { */
            /*         m.ReadPC(); */
            /*         m.stack[m.sp--] = m.pc >> 8; */
            /*         m.stack[m.sp--] = m.pc & 0xff; */
            /*         m.stack[m.sp--] = m.get_SR();// | (1<<BRK); */
            /*         m.pc = m.Read16(m.to_adr(0xfe, 0xff)); */
            /*     } } */
            /* ] ), */

            /* Instruction("rts", { */
            /*     Opcode(0x60, 6, NONE, [](Machine m) { */
            /*         m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1; */
            /*         m.sp += 2; */
            /*     } } */
            /* ] ), */

            /* Instruction("jmp", { */
            /*     Opcode(0x4c, 3, ABS, [](Machine m) { */
            /*         m.pc = m.ReadPC16(); */
            /*     ] ), */
            /*     Opcode(0x6c, 5, IND, [](Machine m) { */
            /*         m.pc = m.Read16(m.ReadPC16()); */
            /*     } } */
            /* ] ), */

            /* Instruction("jsr", { */
            /*     Opcode(0x20, 6, ABS, [](Machine m) { */
            /*         m.stack[m.sp--] = (m.pc+1) >> 8; */
            /*         m.stack[m.sp--] = (m.pc+1) & 0xff; */
            /*         m.pc = m.ReadPC16(); */
            /*     } } */
            /* ] ), */

        ];
        return instructionTable;
    }
};

int main()
{
	Machine!DefaultPolicy m;
	return 0;

}

