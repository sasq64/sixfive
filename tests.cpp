#include "emulator.h"
#include "zyan-disassembler-engine/Zydis/Zydis.hpp"
#include <benchmark/benchmark.h>

#include <coreutils/format.h>

#include <cstdio>
#include <vector>
#include <string>

namespace sixfive {

struct Result
{
    int calls;
    int opcodes;
    int jumps;
    bool tooLong;
};

std::vector<std::string> disasm(void* ptr, struct Result& r)
{
    std::vector<std::string> res;
    using namespace Zydis;

    MemoryInput input(ptr, 0x200);
    InstructionInfo info;
    InstructionDecoder decoder;
    decoder.setDisassemblerMode(DisassemblerMode::M32BIT);
    decoder.setDataSource(&input);
    decoder.setInstructionPointer((uint64_t)ptr);
    IntelInstructionFormatter formatter;
    r.tooLong = false;
    r.calls = r.opcodes = r.jumps = 0;
    while (decoder.decodeInstruction(info)) {
        if (!(info.flags & IF_ERROR_MASK))
            res.push_back(utils::format("    %s", formatter.formatInstruction(info)));

        if (info.mnemonic >= InstructionMnemonic::JA &&
            info.mnemonic <= InstructionMnemonic::JS)
            r.jumps++;

        switch (info.mnemonic) {
        case InstructionMnemonic::RET: return res;
        case InstructionMnemonic::CALL: r.calls++; break;
        default: break;
        }
        r.opcodes++;
    }
    r.tooLong = true;
    return res;
}

template <typename POLICY> void checkCode(bool dis)
{

    using namespace sixfive;

    Result r;
    int jumps = 0;
    int count = 0;
    int calls = 0;
    int opcodes = 0;

    for (const auto& i : Machine<POLICY>::getInstructions()) {
        for (int j=0; j<i.opcodes.size(); j++) {
            const auto& o = i.opcodes[j];
            auto res = disasm((void*)o.op, r);
            auto bytes = 0;
            //if(j < i.opcodes.size()-1) bytes = (char*)i.opcodes[j+1].op - (char*)o.op;
            printf("%p %s (%d bytes)  (%d/%d/%d)\n", (void*)o.op, i.name, bytes, r.opcodes, r.calls, r.jumps);
            if(dis) {
                for(const auto& line : res) {
                    puts(line.c_str());
                }
            }

            jumps += r.jumps;
            calls += r.calls;
            opcodes += r.opcodes;
            count++;
        }
    }
    printf("### AVG OPCODES: %d TOTAL OPS/CALLS/JUMPS: %d/%d/%d\n",
           opcodes / count, opcodes, calls, jumps);

    //disasm((void*)&Machine<POLICY>::runMachine, true, r);
}

struct DirectPolicy : sixfive::DefaultPolicy {
	DirectPolicy(sixfive::Machine<DirectPolicy>&m) {}
    static constexpr int PC_AccessMode = DIRECT;
    static constexpr int Read_AccessMode = DIRECT;
    static constexpr int Write_AccessMode = DIRECT;
};

void checkAllCode(bool dis)
{
    checkCode<DirectPolicy>(dis);
}

/*
uint32_t Machine::runDebug(uint32_t runc) {

    auto toCycles = impl->cycles + runc;
    uint32_t opcodes = 0;
    while(impl->cycles < toCycles) {

        checkEffect(*impl);

        uint8_t code = ReadPC(*impl);
        printf("code %02x\n", code);
        if(code == 0x60 && (uint8_t)impl->sp == 0xff)
            return opcodes;
        auto &op = jumpTable[code];
        op.op(*impl);
        impl->cycles += op.cycles;
        if(impl->breaks.count(impl->pc) == 1)
            impl->breaks[impl->pc](*this);
        opcodes++;
    }
    return opcodes;
}
*/
//} // namespace

static void Bench_sort(benchmark::State& state)
{

    static const uint8_t sortCode[] = {
        0xa0, 0x00, 0x84, 0x32, 0xb1, 0x30, 0xaa, 0xc8, 0xca, 0xb1,
        0x30, 0xc8, 0xd1, 0x30, 0x90, 0x10, 0xf0, 0x0e, 0x48, 0xb1,
        0x30, 0x88, 0x91, 0x30, 0x68, 0xc8, 0x91, 0x30, 0xa9, 0xff,
        0x85, 0x32, 0xca, 0xd0, 0xe6, 0x24, 0x32, 0x30, 0xd9, 0x60};

    static const uint8_t data[] = {
        0,   19, 73,  2,   54,  97,  21,  45,  66,  13, 139, 56,  220, 50,
        30,  20, 67,  111, 109, 175, 4,   66,  100, 19, 73,  2,   54,  97,
        21,  45, 66,  13,  139, 56,  220, 50,  30,  20, 67,  111, 109, 175,
        4,   66, 100, 19,  73,  2,   54,  97,  21,  45, 66,  13,  139, 56,
        220, 50, 30,  20,  67,  111, 109, 175, 4,   66, 100,
    };

    sixfive::Machine<DirectPolicy> m;
    for (int i = 0; i < (int)sizeof(data); i++)
        m.writeRam(0x2000 + i, data[i]);
    for (int i = 0; i < (int)sizeof(sortCode); i++)
        m.writeRam(0x1000 + i, sortCode[i]);
    m.writeRam(0x30, 0x00);
    m.writeRam(0x31, 0x20);
    m.writeRam(0x2000, sizeof(data) - 1);
    m.setPC(0x1000);
    printf("Opcodes %d\n", m.run(50000000));
    // uint8_t temp[256];
    // m.readRam(0x2000, temp, sizeof(data));
    // for(int i=0; i<sizeof(data)+1; i++)
    //	printf("%02x ", temp[i]);
    // puts("");
    while (state.KeepRunning()) {
        for (int i = 1; i < (int)sizeof(data); i++)
            m.writeRam(0x2000 + i, data[i]);
        m.setPC(0x1000);
        m.run(5000000);
    }
}
BENCHMARK(Bench_sort);

struct POLICY2
{
    static constexpr bool Debug = true;
    static constexpr bool AlignReads = false;
    static constexpr bool StatusOpt = false;
    typedef uint8_t Word;
};

static void Bench_emulate(benchmark::State& state)
{

    static const unsigned char WEEK[] = {
        0xa0, 0x74, 0xa2, 0x0a, 0xa9, 0x07, 0x20, 0x0a, 0x10, 0x60, 0xe0,
        0x03, 0xb0, 0x01, 0x88, 0x49, 0x7f, 0xc0, 0xc8, 0x7d, 0x2a, 0x10,
        0x85, 0x06, 0x98, 0x20, 0x26, 0x10, 0xe5, 0x06, 0x85, 0x06, 0x98,
        0x4a, 0x4a, 0x18, 0x65, 0x06, 0x69, 0x07, 0x90, 0xfc, 0x60, 0x01,
        0x05, 0x06, 0x03, 0x01, 0x05, 0x03, 0x00, 0x04, 0x02, 0x06, 0x04};

    sixfive::Machine<> m;
    for (int i = 0; i < (int)sizeof(WEEK); i++)
        m.writeRam(0x1000 + i, WEEK[i]);
    m.setPC(0x1000);
    m.run(5000000);
    while (state.KeepRunning()) {
        m.setPC(0x1000);
        m.run(5000);
    }
};

BENCHMARK(Bench_emulate);

static void Bench_allops(benchmark::State& state)
{
    sixfive::Machine<> m;
    m.setPC(0x1000);
    while (state.KeepRunning()) {
        m.setPC(0x1000);
        for (const auto& i : m.getInstructions()) {
            for (const auto& o : i.opcodes) {
                o.op(m);
            }
        }
    }
}
BENCHMARK(Bench_allops);
/*
int main(int argc, char **argv) {
    sixfive::Machine<> m;
    return 0;
}*/

} // namespace sixfive
