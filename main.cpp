//#include "compile.h"
#include "emulator.h"
//#include "monitor.h"

#include "CLI11.hpp"

//#include <coreutils/file.h>
//#include <coreutils/utils.h>

#include <benchmark/benchmark.h>

#include <functional>
#include <tuple>
#include <unordered_map>

//#include <bbsutils/console.h>
//#include <bbsutils/editor.h>

// sixfive -s <source> -io [c64|simple|none] -break [adr] -trace

struct CheckPolicy : public sixfive::DefaultPolicy {

	sixfive::Machine<CheckPolicy>& machine;

	CheckPolicy(sixfive::Machine<CheckPolicy>& m) : machine(m) {}

    static bool eachOp(CheckPolicy& dp)
    {
		auto& m = dp.machine;
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
        std::ifstream stream("6502test.bin", std::ios::in | std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        data[0x3b91] = 0x60;
        Machine<CheckPolicy> m;
        m.writeRam(0, &data[0], 0x10000);
        m.setPC(0x1000);
        m.run(1000000000);
        printf("Done.\n");
    }

    if(runFullTest || doBenchmarks || checkOpcodes)
        return 0;

    // Machine<DebugPolicy> m;
    //
    // if (!asmFile.empty()) {
    //     bool ok = compile(asmFile, m);
    //
    //     if (!ok) {
    //         printf("Parse failed\n");
    //         return -1;
    //     }
    // }
    // if (doMonitor) monitor(m);
    //m.setPC(0x01000);
    //m.run(100000);
    return 0;
}
