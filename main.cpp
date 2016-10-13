#include "emulator.h"
#include "assembler.h"

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


class run_exception : public std::exception
{
public:
	run_exception(const std::string &m = "RUN Exception") : msg(m) {}
	virtual const char *what() const throw() { return msg.c_str(); }

private:
	std::string msg;
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


int main(int argc, char **argv)
{
	using namespace sixfive;


	if(argc >= 2 && strcmp(argv[1], "-t") == 0) {
		checkAllCode();
		benchmark::Initialize(&argc, argv);
		benchmark::RunSpecifiedBenchmarks();
		printf("%d\n", (int)sizeof(int_fast8_t));
		return 0;
	}

	if(argc >= 2 && strcmp(argv[1], "-X") == 0) {
		utils::File f { "6502test.bin" };
		auto data = f.readAll();
		data[0x3af8] = 0x60;
		Machine<CheckPolicy> m;
		m.writeRam(0, &data[0], 0x10000);
		m.setPC(0x1000);
		m.run(1000000000);
		return 0;
	}

	Machine<IOPolicy> mm2;

	Machine<DebugPolicy> m;

	utils::File f { argv[1] };
	auto src = f.readAll();
	auto srcText = std::string((char*)&src[0], src.size());

	int maxOrg = 0;
	bool ok = parse(srcText, [&](uint16_t org, const std::string &op, const std::string &arg) -> int {
		if(op == "b") {
			//memcpy(&m.mem[org], &arg[0], arg.size());
			for(int i=0; i<(int)arg.size(); i++)
				m.writeRam(org+i, arg[i]);
			int o = org + arg.size();
			if(o > maxOrg)
				maxOrg = o;
			return arg.size();
		}
		printf(">> %s %s\n", op.c_str(), arg.c_str());
		if(op[0] == '@') {
			//reqs[org] = arg;
			std::string what, value;
			std::vector<std::pair<int, int>> reqs;
			enum { RA = 0x10000, RX, RY, RSR, RSP, RPC };
			for(const auto &r :  utils::split(arg, ",")) {
				std::tie(what,value) = utils::splitn<2>(r, "=");
				int v = std::stol(value, 0, 0);
				int w;
				if(what == "a") w = RA;
				else if(what == "x") w = RX;
				else if(what == "y") w = RY;
				else if(what == "sr") w = RSR;
				else if(what == "pc") w = RPC;
				else w = std::stol(what, 0, 0);
				printf("%s must be %02x\n", what.c_str(), v);
				reqs.push_back(std::make_pair(w,v));
			}
			m.policy().set_break(org, [=](Machine<DebugPolicy> &m) {
					
					//printf("Break at %04x\n", m.getPC());
					printf("Break\n");
					for(auto &r : reqs) {
						int v = 0;
						switch(r.first) {
						case RA: v = m.regA(); break;
						case RX: v = m.regX(); break;
						case RY: v = m.regY(); break;
						case RSR: v = m.regSR(); break;
						default: v = m.readRam(r.first); break;
						}
						printf("COMP %d %d\n", v, r.second);
								
						if(v != r.second)
							throw run_exception(std::string("REQ Failed: ") + std::to_string(v) + " != " + std::to_string(r.second));
					}
					printf("DONE\n");

			});

			return 0;
		}
		uint8_t temp[4];
		int len = assemble(org, &temp[0], std::string(" ") + op + " " + arg);
		if(len > 0) {
			m.writeRam(org, &temp[0], len);
			auto o = org + len;
			if(o > maxOrg)
				maxOrg = o;
		}
		return len;
	});

	int len = maxOrg - 0x1000;
	if(len > 0) {
		uint8_t temp[65536];
		m.readRam(0x1000, temp, len);
		utils::File f { "dump.dat" };
		f.write(temp, len);
	}

	if(!ok) {
		printf("Parse failed\n");
		return -1;
	}

	m.setPC(0x01000);
	m.run(100000);
	return 0;
}
