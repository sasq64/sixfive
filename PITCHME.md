
@ul
I wanted to see if I could write a 6502 emulator...
* Using modern C++
* Without any _macros_ or _ifdefs_
* Being as fast or _faster_ than earlier emulators
* Being more _configurable_ than earlier emulators.
* Being more _extensible_ than earlier emulators
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
@[10-13](The simpilfied mainloop)
