#include "emulator.h"
#include "assembler.h"

#include <coreutils/utils.h>
#include <coreutils/file.h>

#include <functional>
#include <unordered_map>
#include <tuple>

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

	Machine<> m;
	m.init();

	if(argc >= 2 && strcmp(argv[1], "-X") == 0) {
		utils::File f { "6502test.bin" };
		auto data = f.readAll();
		data[0x3af8] = 0x60;
		m.writeRam(0, &data[0], 0x10000);
		m.setPC(0x1000);
		m.runDebug(1000000000);
		return 0;
	}


	FILE *fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	auto source = std::make_unique<char[]>(size+1);
	int rc = fread(&source[0], 1, size, fp);
	if(rc != size)
		return -1;
	source[size] = 0;
	fclose(fp);

	//std::unordered_map<uint16_t, std::string> reqs;
	int maxOrg = 0;
	bool ok = parse(&source[0], [&](uint16_t org, const std::string &op, const std::string &arg) -> int {
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
			m.set_break(org, [=](Machine<> &m) {
					
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

	uint8_t temp[65536];
	int len = maxOrg - 0x1000;
	if(len > 0) {
		fp = fopen("dump.dat", "wb");
		m.readRam(0x1000, temp, len);
		fwrite(temp, 1, len, fp);
		fclose(fp);
	}

	if(!ok) {
		printf("Parse failed\n");
		return -1;
	}
	//return 0;
	m.setPC(0x01000);
	//m.pc = 0x1000;
	m.runDebug(100000);

	//printf("%02x\n", m.mem[0x2000]);
	return 0;
}
