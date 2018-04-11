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

#include <bbsutils/console.h>
#include <bbsutils/editor.h>

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

    using Machine = sixfive::Machine<DebugPolicy>;

    static constexpr uint16_t IOMASK = 0xf0;
    static constexpr uint16_t IOBANK = 0xd0;


    Machine& machine;

    bbs::Console* console;

    DebugPolicy(Machine& m) : machine(m) {
        using namespace bbs;
        using namespace utils;
        console = Console::createLocalConsole();
    }

    template <typename ... ARGS>
    void print(const std::string& fmt, ARGS ... params) {
        auto s = utils::format(fmt, params...);
        console->write(s);
    };

    static inline constexpr void writeIO(Machine& m, uint16_t adr, uint8_t v) {
        //m.policy().print("Wrote %04x := %02x\n", adr, v);
        auto* console = m.policy().console;
        int x = (adr - 0xd000) % 40; 
        int y = (adr - 0xd000) / 40; 
        console->put(x, y, v);
        console->flush();
    }
    static inline constexpr uint8_t readIO(Machine&, uint16_t adr)
    {
        return 0;
    }
    void checkEffect(Machine& m)
    {
        static sixfive::Machine<DebugPolicy> om;
        if (om.regPC() != 0) {
            // if(om.pc != m.pc)
            print("%04x : ", (unsigned)om.regPC());
            print("[ ");
            if (om.regA() != m.regA()) print("A:%02x ", m.regA());
            if (om.regX() != m.regX()) print("X:%02x ", m.regX());
            if (om.regY() != m.regY()) print("Y:%02x ", m.regY());
            if (om.regSR() != m.regSR()) print("SR:%02x ", m.regSR());
            if (om.regSP() != m.regSP()) print("SP:%02x ", m.regSP());
            bool first = true;
            for (int i = 0; i < 65536; i++)
                if (m.Ram(i) != om.Ram(i)) {
                    if (!first) print(" # ");
                    first = false;
                    print("%04x: ", i);
                    while (m.Ram(i) != om.Ram(i)) {
                        print("%02x ", m.Ram(i) & 0xff);
                        om.Ram(i) = m.Ram(i);
                    }
                }
            print("]\n");
        } else {
            for (int i = 0; i < 65536; i++)
                om.Ram(i) = m.Ram(i);
        }
        om.regs() = m.regs();
    }

    std::unordered_map<uint16_t, std::function<void(Machine& m)>> breaks;

    void set_break(uint16_t pc, std::function<void(Machine& m)> f)
    {
        breaks[pc] = std::move(f);
    }

    inline static bool doTrace = false;

    static bool eachOp(Machine& m)
    {
        static int lastpc = -1;
        if (doTrace) m.policy().checkEffect(m);
        if (m.regPC() == lastpc) {
            m.policy().print("STALL\n");
            return true;
        }
        lastpc = m.regPC();
        return false;
    }

};


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
            //monitor(m);

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
        data[0x3b91] = 0x60;
        Machine<CheckPolicy> m;
        m.writeRam(0, &data[0], 0x10000);
        m.setPC(0x1000);
        m.run(1000000000);
        printf("Done.\n");
    }

    if(runFullTest || doBenchmarks || checkOpcodes)
        return 0;

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
