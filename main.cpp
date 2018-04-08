#include "compile.h"
#include "emulator.h"
#include "monitor.h"

#include "CLI11.hpp"

#include <coreutils/file.h>
#include <coreutils/utils.h>

#include <benchmark/benchmark.h>

#include <functional>
#include <tuple>
#include <unordered_map>

// clang-format off
constexpr static const char* modeNames[] = { 
	"ILLEGAL",
	"NONE",
	"ACC",

	"SIZE2",

	"#IMM",
	"REL",

	"$ZP",
	"$ZP,X",
	"$ZP,Y",
	"$(ZP,X)",
	"$(ZP),Y",

	"SIZE3",

	"($IND)",
	"$ABS",
	"$ABS,X",
	"$ABS,Y",
};
// clang-format on

// sixfive -s <source> -io [c64|simple|none] -break [adr] -trace

struct DebugPolicy : public sixfive::DefaultPolicy
{

    static void checkEffect(sixfive::Machine<DebugPolicy>& m)
    {
        static sixfive::Machine<DebugPolicy> om;
        if (om.regPC() != 0) {
            // if(om.pc != m.pc)
            printf("%04x : ", (unsigned)om.regPC());
            printf("[ ");
            if (om.regA() != m.regA()) printf("A:%02x ", m.regA());
            if (om.regX() != m.regX()) printf("X:%02x ", m.regX());
            if (om.regY() != m.regY()) printf("Y:%02x ", m.regY());
            if (om.regSR() != m.regSR()) printf("SR:%02x ", m.regSR());
            if (om.regSP() != m.regSP()) printf("SP:%02x ", m.regSP());
            bool first = true;
            for (int i = 0; i < 65536; i++)
                if (m.Ram(i) != om.Ram(i)) {
                    if (!first) printf(" # ");
                    first = false;
                    printf("%04x: ", i);
                    while (m.Ram(i) != om.Ram(i)) {
                        printf("%02x ", m.Ram(i) & 0xff);
                        om.Ram(i) = m.Ram(i);
                    }
                }
            printf("]\n");
        } else {
            for (int i = 0; i < 65536; i++)
                om.Ram(i) = m.Ram(i);
        }
        om.regs() = m.regs();
    }

    std::unordered_map<uint16_t,
                       std::function<void(sixfive::Machine<DebugPolicy>& m)>>
        breaks;

    void set_break(uint16_t pc,
                   std::function<void(sixfive::Machine<DebugPolicy>& m)> f)
    {
        breaks[pc] = std::move(f);
    }

    static bool doTrace;

    static bool eachOp(sixfive::Machine<DebugPolicy>& m)
    {
        static int lastpc = -1;
        if (doTrace) checkEffect(m);
        if (m.regPC() == lastpc) {
            printf("STALL\n");
            return true;
        }
        lastpc = m.regPC();
        return false;
    }
};

bool DebugPolicy::doTrace = false;

struct CheckPolicy : public sixfive::DefaultPolicy
{
    static bool eachOp(sixfive::Machine<CheckPolicy>& m)
    {
        static int lastpc = -1;
        if (m.regPC() == lastpc) {
            const auto [a, x, y, sr, sp, pc] = m.regs();
            printf("STALL @ %04x A %02x X %02x Y %02x SR %02x SP %02x\n",
                   lastpc, a, x, y, sr, sp);
            for (int i = 0; i < 256; i++)
                printf("%02x ", m.Stack(i));
            printf("\n");
            monitor(m);

            return true;
        }
        lastpc = m.regPC();
        return false;
    }

    static inline bool doTrace = false;
};

struct IOPolicy : public sixfive::DefaultPolicy
{
    static constexpr uint16_t IOMASK = 0xff00;
    static constexpr uint16_t IOBANK = 0xff00;
    static inline constexpr void writeIO(sixfive::Machine<IOPolicy>& m,
                                         uint16_t adr, uint8_t v)
    {}
    static inline constexpr uint8_t readIO(sixfive::Machine<IOPolicy>& m,
                                           uint16_t adr)
    {
        return 0;
    }
};

namespace sixfive {
void checkAllCode(bool dis);
}

int main(int argc, char** argv)
{
    using namespace sixfive;

    bool doMonitor = false;
    bool doTest = false;
    bool checkOpcodes = false;
    bool runFullTest = false;
    bool doBenchmarks = false;
    bool disasm = false;
    std::string asmFile;

    static CLI::App opts{"sixfive"};

    opts.add_flag("--disassemble", disasm, "Disassmble checked opcodes");
    opts.add_flag("-O,--check-opcodes", checkOpcodes, "Check all opcodes");
    opts.add_flag("-B,--benchmarks", doBenchmarks, "Run benchmarks");
    opts.add_flag("-F,--full-test", runFullTest, "Run full 6502 test");

    opts.add_flag("-m,--monitor", doMonitor, "Jump into monitor");
    opts.add_option("asmfile", asmFile, "Assembly file to compile");

    CLI11_PARSE(opts, argc, argv);

    // Run tests
    if (checkOpcodes) checkAllCode(disasm);

    if (doBenchmarks) {
        benchmark::Initialize(&argc, argv);
        benchmark::RunSpecifiedBenchmarks();
    }

    if (runFullTest) {
        printf("Running full 6502 test...\n");
        utils::File f{"6502test.bin"};
        auto data = f.readAll();
        data[0x3af8] = 0x60;
        Machine<CheckPolicy> m;
        m.writeRam(0, &data[0], 0x10000);
        m.setPC(0x1000);
        m.run(1000000000);
        printf("Done.\n");
        return 0;
    }

    Machine<DebugPolicy> m;

    if (!asmFile.empty()) {
        bool ok = compile(asmFile, m);

        if (!ok) {
            printf("Parse failed\n");
            return -1;
        }
    }
    if (doMonitor) monitor(m);
    m.setPC(0x01000);
    m.run(100000);
    return 0;
}
