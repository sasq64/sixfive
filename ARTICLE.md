
# Emulating the 6502 using modern C++

...investigating wether you can write traditionally size/performance
sensitive code without macros, reduntant code and other ugliness.

### INTRO

The _6502_ — for those of you who are too young to remember — is the
classic 8-bit processor, used in machines such as the Commodore 64, the
Atari 2600, the NES and the Apple II. It is a pretty simple CPU with ~155
opcodes (~56 textual instructions) and only 3 general purpose registers.

Opcodes are 1 to 3 bytes long, where the first byte uniquely identifies
the opcode.

Traditionally, 6502 emulation has been done in C, using either a big
switch statement or a jump table, where the first byte of the opcode
decides what code is to be executed. Since many opcodes are similar this
usually means lots of macros and/or reduntant code.

I wanted to see if I could write a 6502 emulator...

* Using modern C++
* Without any _macros_ or _#ifdefs_
* Being as fast or _faster_ than earlier emulators
* Being more _configurable_ than earlier emulators.
* Being more _extensible_ than earlier emulators

### A comparison

Let's take a look at an example of emulation code. We will use the Vice
C64 emulator, and it's implementation of the LDA, LDX and LDY opcodes.

All these opcodes work similarly, in that they load a value into one of
the tree (A, X or Y) registers. 

First there are macros for all the opcodes like this;

```c++
#define LDA(value, clk_inc, pc_inc) \
    do {                            \
        BYTE tmp = (BYTE)(value);   \
        reg_a_write(tmp);           \
        CLK_ADD(CLK, (clk_inc));    \
        LOCAL_SET_NZ(tmp);          \
        INC_PC(pc_inc);             \
    } while (0)

#define LDX(value, clk_inc, pc_inc) \
    do {                            \
        reg_x_write((BYTE)(value)); \
        LOCAL_SET_NZ(reg_x_read);   \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)

#define LDY(value, clk_inc, pc_inc) \
    do {                            \
        reg_y_write((BYTE)(value)); \
        LOCAL_SET_NZ(reg_y_read);   \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)
```

Then there is the actual switch statement, here showing just the
relevant portion;

```c++
    case 0xa0:          /* LDY #$nn */
        LDY(p1, 0, 2);
        break;

    case 0xa1:          /* LDA ($nn,X) */
        LDA(LOAD_IND_X(p1), 1, 2);
        break;

    case 0xa2:          /* LDX #$nn */
        LDX(p1, 0, 2);
        break;
```

To compare, my _Load_ method looks like this;

```c++
    template <int REG, int MODE> static constexpr void Load(Machine& m)
    {
        m.Reg<REG>() = m.LoadEA<MODE>();
        m.set_SZ<REG>();
    }
```

(To be fair my emulator is not yet cycle correct...)

And the corresponding part of the jump table;

```c++
    { 0xa0, 2, IMM, Load<Y, IMM>},
    { 0xa1, 6, INDX, Load<A, INDX>},
    { 0xa2, 2, IMM, Load<X, IMM>},
```

### Issues

So, as a rule I was aiming to generate similar machine code for my
emulator to Vice, but I wanted to break out all common functionality into
methods, and use template arguments for everything that was known at
compile time instead of using macros.

One major problem with this is that while modern C++ compilers are usually good at
figuring out what is compile time constant, and usually inlines all small
templated methods, there is no real gurantee. So how do we know a small
change or a missed const somewhere wont blow up the code size and slow
everything down?

The obvious answer is benchmarking. But that only measures speed, and
also does not give the same insight/control as when code at lower level, where
you can control these things.

### Disassemble it!

My solution was to have a test that disassembled every opcode in the jumptable,
and made sure that

* The average size of each emulated opcode doesn't suddenly grow
* That opcodes does not contain any calls, and preferably no branches whatsoever.

It also allowed me to dive in and easily check the actual assembly of my functions
when I was unsure.

I used a third party disassembler called [Zydis](https://github.com/zyantific/zydis)
for this;

```c++
    Result r;
    while (decoder.decodeInstruction(info)) {

        if (info.mnemonic >= InstructionMnemonic::JA &&
            info.mnemonic <= InstructionMnemonic::JS)
            r.jumps++;

        switch (info.mnemonic) {
        case InstructionMnemonic::RET: return;
        case InstructionMnemonic::CALL: r.calls++; break;
        default: break;
        }
        r.opcodes++;
    }
```

This allowed me to easily count all the opcodes, and specifically the
number of branches and calls, for each emulated opcode.

```
$ build/sixfive -O
### AVG OPCODES: 21 TOTAL OPS/CALLS/JUMPS: 3306/0/4
$
```

Here we see that our 156 functions disassembles to 3306 x86 opcodes in total.

### The mysterous branches

Interestingly we also have 4 branches. The full output show the four offending
functions;
```
rti (34/0/1)
rts (13/0/1)
plp (22/0/1)
bmi (14/0/1)
```

The reason that _RTI_ and _PLP_ contains a branch is because they can write
the status register directly. Let's check the code path for the _PLP_ opcode;

Here is the entry in the jumptable;
```c++
    { "plp", { { 0x28, 4, NONE, [](Machine& m) {
        m.set_SR(m.stack[++m.sp]);
    } } } },
```

And here is the `set_SR()` method.
```c++
    void set_SR(uint8_t s)
    {
        if ((s ^ sr) & d_FLAG) { // D bit changed
            if (s & d_FLAG) {
                setDec<true>();
            } else {
                setDec<false>();
            }
        }
        sr = s | 0x30;
    }
```

So the _if_ statement in `set_SR()` is what causes our branch.
It should be mentioned that branches such as this will not have a huge
performance impact on modern CPUs, since most code never touches the
D flag, and so branch prediction will work well here.

What is more interesting is that we have another instruction that calls
`set_SR()`, namely the _bit_ opcode.

```c++
    template <int REG, int MODE> static constexpr void Bit(Machine& m)
    {
        unsigned z = m.LoadEA<MODE>();
        m.set_SR((m.get_SR() & 0x3d) | (z & 0xc0) | (!(z & m.a) << 1));
    }
```

In this case, the compiler is smart enough to see that the D bit can not
possibly be changed by the code in `Bit()` so it can eliminate the _if_
statement. Very nice!

_RTS_ contains this bit of code;

```c++
    if constexpr (POLICY::ExitOnStackWrap) {
        if (m.sp == 0xff) {
            m.cycles = m.toCycles;
            return;
        }
    }
```

that is, when _ExitOnStackWrap_ is true, _RTS_ does an extra check that
lets us stop the emulation on the "last" time we return from a subroutine,
this explaining that branch opcode.

_BMI_ has no real reason to contain a branch -- I need to investigate after I'm
done writing this.


