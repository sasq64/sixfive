

#include "assembler.h"
#include "emulator.h"
#include "parser.h"
#include <bbsutils/console.h>
#include <bbsutils/editor.h>

#include <memory>

namespace sixfive {

// clang-format off
constexpr static const char* modeTemplate[] = { 
	"",
	"",
	"A",

	//"SIZE2",

	"#$%02x",
	"$%04x",

	"$%02x",
	"$%02x,x",
	"$%02x,y",
	"($%02x,x)",
	"($%02x),y",

	//"SIZE3",

	"($%04x)",
	"$%04x",
	"$%04x,x",
	"$%04x,y",
};
// clang-format on

std::string disasm(uint16_t& org, uint8_t* mem)
{

    for (const auto& ins : Machine<>::getInstructions()) {
        for (const auto& op : ins.opcodes) {
            if (op.code == mem[0]) {
                int v = 0;
                auto* orgmem = mem;
                mem++;
                if (sixfive::opSize(op.mode) > 1) v = *mem++;
                if (sixfive::opSize(op.mode) > 2) v = v | (*mem++) << 8;
                if (op.mode == REL) v = ((int8_t)v) + 2 + org;
                org += (mem - orgmem);
                if (op.mode == NONE) return ins.name;
                return std::string(ins.name) + " " +
                       utils::format(modeTemplate[op.mode], v);
            }
        }
    }
    org++;
    return utils::format("db $%02x", mem[0]);
};

template <typename POLICY> void monitor(Machine<POLICY>& m)
{
    using namespace bbs;
    using namespace utils;
    //auto* console = Console::createLocalConsole();
    auto* console = m.policy().console;
    uint16_t start = m.regPC();
    int size = 16;
    std::string prefix = "";
    auto print = [=](const std::string& fmt, auto... params) {
        auto s = utils::format(fmt, params...);
        console->write(s);
    };


    MonParser parser;

    auto lineEd = std::make_unique<bbs::LineEditor>(*console, 40);

    while (true) {
        print(">>");
        lineEd->setXY();
        lineEd->setString(prefix);
        while (lineEd->update(500) != bbs::Console::KEY_ENTER)
            ;
        console->write("\n");
        auto line = lineEd->getResult();
        if (line == prefix) {
            prefix = "";
            continue;
        }
        auto cmd = parser.parseLine(line);
        if (!cmd) {
            print("?SYNTAX  ERROR\n");
            continue;
        }
        /* print("CMD '%s' ARG '%s'\n", cmd.name, cmd.strarg); */
        /* for(const auto& a : cmd.args) { */
        /*     print("%x\n", a); */
        /* } */

        if (cmd.name == "trace") {
            POLICY::doTrace = (cmd.strarg == "on");
        } else if (cmd.name == "d") {
            if (cmd.args.size() > 0) start = cmd.args[0];
            if (cmd.args.size() > 1)
                size = cmd.args[1];
            else
                size = 16;
            uint8_t input[4];
            for (int i = 0; i < size; i++) {
                m.readMem(start, input, 3);
                auto org = start;
                auto s = disasm(start, (uint8_t*)input);
                print("%04x: %s\n", org, s);
            }

        } else if (cmd.name == "c") {
            m.run();
        } else if (cmd.name == "g") {
            m.setPC(cmd.args[0]);
            m.run();
        } else if (cmd.name == "af") {
            bool ok = compile(cmd.strarg, m);
            if(ok)
                print("Assemble successful\n");
        } else if (cmd.name == "a") {
            uint8_t output[4];
            if (cmd.args.size() > 0) start = cmd.args[0];
            int len = assemble(start, output, cmd.strarg);
            if (len > 0) {
                m.writeRam(start, output, len);
                start += len;
                prefix = utils::format("a %04x ", start);
            }
        } else if (cmd.name == "m") {
            if (cmd.args.size() > 0) start = cmd.args[0];
            if (cmd.args.size() > 1)
                size = cmd.args[1];
            else
                size = 16;
            print("%04x : ", start);
            for (int i = 0; i < size; i++)
                print("%02x ", m.readMem(start + i));
            print("\n");
        } else if (cmd.name == "r") {
            const auto [a, x, y, sr, sp, pc] = m.regs();
            print("PC: %04x A: %02x X: %02x Y: %02x SR: %02x SP: %02x [%04x]\n", pc,
                  a, x, y, sr, sp, m.Stack(sp-1)<<8 | m.Stack(sp-2));
        }
    }
};

} // namespace sixfive
