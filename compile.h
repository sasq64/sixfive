#include "assembler.h"
#include "emulator.h"

#include <cstdint>
#include <coreutils/file.h>
#include <string>
#include <vector>

namespace sixfive {

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

template <typename POLICY> int compile(const std::string &fileName, Machine<POLICY> &m) {
	utils::File f {fileName };
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
			m.policy().set_break(org, [=](Machine<POLICY> &m) {
					
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
		int len = assemble(org, (uint8_t*)&temp[0], std::string(" ") + op + " " + arg);
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
		f.write((uint8_t*)temp, len);
	}

	return ok;
}
}
