
## Emulating the 6502 using modern C++
_by Jonas Minnberg_

Investigating wether you can write traditionally size/performance
sensitive code without macros, reduntant code and other ugliness

---
# The 6502

@ul
* Used in C64, NES, Atari 2600, Apple II etc
* Around 155 opcodes (56 instructions)
* Opcodes 1-3 bytes long
* Uniquely identified by first byte
@ulend

---
@ul
I wanted to see if I could write a 6502 emulator...
* Using modern C++
* Without any _macros_ or _ifdefs_
* Being as fast or _faster_ than earlier emulators
* Being more _configurable_ than earlier emulators.
@ulend
---
## Jump table or switch statement ?
---
Jump table! becuase
@ul
* Better code separation
* Allows for templated function calls
* yeah
@ulend

---

```c++
class Machine {
    using OpFunc = void (*op)(Machine&);
    std::array<OpFunc, 256> jumpTable;
    uint16_t pc = 0;
    int cycles = 0;
    std::array<uint8_t, 65536> memory;
    // ...
    void run(int numOpcodes) {
        while (numOpcodes--) {
            auto code = memory[pc++];
            jumpTable[code](*this);
        }
    }
}
```
@[3-6]
@[6]
@[9-12]
@[11]

---

## Example opcode: **TAX**

---
```c++
    template <int FROM, int TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != SP) m.set<SZ>(m.Reg<TO>());
    }
```
---
```c++
    template <int REG> constexpr auto& Reg()
    {
        if constexpr (REG == A) return a;
        if constexpr (REG == X) return x;
        if constexpr (REG == Y) return y;
        if constexpr (REG == SP) return sp;
    }
```
---
```c++
    template <int BITS> void set(int res, int arg = 0)
    {
        sr &= ~BITS;

        if constexpr ((BITS & S) != 0) sr |= (res & 0x80); // Apply signed bit
        if constexpr ((BITS & Z) != 0) sr |= (!(res & 0xff) << 1); // Apply zero
        if constexpr ((BITS & C) != 0) sr |= ((res >> 8) & 1); // Apply carry
        if constexpr ((BITS & V) != 0)
            sr |= ((~(a ^ arg) & (a ^ res) & 0x80) >> 1); // Apply overflow
    }
```
---
```c++
    template <int FROM, int TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != SP) m.set<SZ>(m.Reg<TO>());
    }
```
---
```c++
    template <int FROM, int TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != SP) m.set_SZ<TO>();
    }
```
---
```c++
    template <int REG> void set_SZ()
    {
        sr = (sr & ~SZ) | (Reg<REG>() & 0x80) | (!Reg<REG>() << 1);
    }
```
---

And here are our jump table entries;

```c++
    { "tax", { { 0xaa, 2, NONE, Transfer<A, X> } } },
    { "txa", { { 0x8a, 2, NONE, Transfer<X, A> } } },
    { "tay", { { 0xa8, 2, NONE, Transfer<A, Y> } } },
    { "tya", { { 0x98, 2, NONE, Transfer<Y, A> } } },
    { "txs", { { 0x9a, 2, NONE, Transfer<X, SP> } } },
    { "tsx", { { 0xba, 2, NONE, Transfer<SP, X> } } },
```

