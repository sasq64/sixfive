#include "emulator.h"
#include "compile.h"

#include <coreutils/utils.h>
#include <coreutils/file.h>

#include <functional>
#include <unordered_map>
#include <tuple>

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

bool parse(const std::string &code, 
		std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode);

#include <benchmark/benchmark.h>


// sixfive -s <source> -io [c64|simple|none] -break [adr] -trace


struct DebugPolicy : public sixfive::DefaultPolicy
{

	static void checkEffect(sixfive::Machine<DebugPolicy> &m) {
		static sixfive::Machine<DebugPolicy> om;
		printf("[ ");
		if(om.pc != m.pc)
			printf("PC := %04x ", (unsigned)m.pc);
		if(om.a != m.a)
			printf("A := %02x ", m.a);
		if(om.x != m.x)
			printf("X := %02x ", m.x);
		if(om.y != m.y)
			printf("Y := %02x ", m.y);
		if(om.sr != m.sr)
			printf("SR := %02x ", m.sr);
		if(om.sp != m.sp)
			printf("SP := %02x ", m.sp);
		printf("]\n");
		for(int i = 0; i < 65536; i++)
			if(m.mem[i] != om.mem[i]) {
				printf("%04x := %02x ", i, m.mem[i] & 0xff);
				om.mem[i] = m.mem[i];
			}
		puts("");

		om.a = m.a;
		om.x = m.x;
		om.y = m.y;
		om.sp = m.sp;
		om.sr = m.sr;
		om.pc = m.pc;
		om.sp = m.sp;
	}

	std::unordered_map<uint16_t, std::function<void(sixfive::Machine<DebugPolicy> &m)>> breaks;

	void set_break(uint16_t pc, std::function<void(sixfive::Machine<DebugPolicy> &m)> f) {
		breaks[pc] = f;
	}

	 static bool eachOp(sixfive::Machine<DebugPolicy> &m) {
		static int lastpc = -1;
		checkEffect(m);
		if(m.pc == lastpc) {
			printf("STALL\n");
			return true;
		}
		lastpc = m.pc;
		return false;
	}
};

struct CheckPolicy : public sixfive::DefaultPolicy
{
	 static bool eachOp(sixfive::Machine<CheckPolicy> &m) {
		static int lastpc = -1;
		if(m.pc == lastpc) {
			printf("STALL\n");
			return true;
		}
		lastpc = m.pc;
		return false;
	}
};


struct IOPolicy : public sixfive::DefaultPolicy {
	static constexpr uint16_t IOMASK = 0xff00;
	static constexpr uint16_t IOBANK = 0xff00;
	static inline constexpr void writeIO(sixfive::Machine<IOPolicy> &m, uint16_t adr, uint8_t v) {
	}
	static inline constexpr uint8_t readIO(sixfive::Machine<IOPolicy> &m, uint16_t adr) { return 0; }
};

namespace sixfive {
void checkAllCode();
}

int main(int argc, char **argv)
{
	using namespace sixfive;


	// Run tests
	if(argc >= 2 && strcmp(argv[1], "-t") == 0) {
		checkAllCode();
		benchmark::Initialize(&argc, argv);
		benchmark::RunSpecifiedBenchmarks();
		utils::File f { "6502test.bin" };
		auto data = f.readAll();
		data[0x3af8] = 0x60;
		Machine<CheckPolicy> m;
		m.writeRam(0, &data[0], 0x10000);
		m.setPC(0x1000);
		m.run(1000000000);
		return 0;
	}

	Machine<DebugPolicy> m;

	bool ok = compile(argv[1], m);

	if(!ok) {
		printf("Parse failed\n");
		return -1;
	}

	m.setPC(0x01000);
	m.run(100000);
	return 0;
}
