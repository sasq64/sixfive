#include "emulator.h"

#include <coreutils/file.h>
#include <coreutils/log.h>

struct C64Policy : sixfive::DefaultPolicy
{
    static bool eachOp(sixfive::Machine<C64Policy>& m)
    {
        //LOGD("PC: %04x", m.regPC());
        return false;
    }
};

int main()
{
    using C64 = sixfive::Machine<C64Policy>;

    C64 machine;

    logging::setLevel(logging::DEBUG);

    auto kernal = utils::File{"c64/kernal"}.readAll();
    auto basic = utils::File{"c64/basic"}.readAll();
    auto cargen = utils::File{"c64/chargen"}.readAll();

    machine.mapRom(0xe0, &kernal[0], 8192);
    machine.mapRom(0xa0, &basic[0], 8192);

    machine.mapReadCallback(0xD0, 4096,
                            [](const C64& m, uint16_t adr) -> uint8_t {
                                LOGI("IO Read from %04x", adr);
                                return 0;
                            });
    machine.mapWriteCallback(0xD0, 4096, [](C64& m, uint16_t adr, uint8_t v) {
        LOGI("%04x : IO Write %02x to %04x", m.regPC(), v, adr);
    });

    machine.mapWriteCallback(0x00, 256, [](C64& m, uint16_t adr, uint8_t v) {
        if (adr == 0x0001) {
            LOGD("Write %02x to $01", v);
            // Do bank switch
        }
        m.Ram(adr) = v;
    });

    uint16_t start = machine.readMem(0xfffc) | (machine.readMem(0xfffd) << 8);
    machine.setPC(start);
    machine.run(1000000);
    for(int i=0x1000; i<0x1100; i++)
        printf("%02x ", machine.Ram(i));
}
