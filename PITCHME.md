
@ul
I wanted to see if I could write a 6502 emulator...
* Using modern C++
* Without any _macros_ or _ifdefs_
* Being as fast or _faster_ than earlier emulators
* Being more _configurable_ than earlier emulators.
* Being more _extensible_ than earlier emulators
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
@[3](We use a `std::array` for our jumptable)
@[6](And for our memory (which is 8bit of course))
@[9-12](The simpilfied mainloop)

---

Here is our opcode function:

```c++
    template <int FROM, int TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != SP) m.set_SZ<TO>();
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

