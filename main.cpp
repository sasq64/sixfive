#include "emulator.h"
#include "assembler.h"

#include <coreutils/utils.h>

#include <functional>
#include <unordered_map>
#include <tuple>

bool parse(const std::string &code, 
		std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode);

int main(int argc, char **argv)
{
	using namespace sixfive;
	Machine m;
	m.init();

	FILE *fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	auto source = std::make_unique<char[]>(size+1);
	fread(&source[0], 1, size, fp);
	source[size] = 0;
	fclose(fp);

	//std::unordered_map<uint16_t, std::string> reqs;


	bool ok = parse(&source[0], [&](uint16_t org, const std::string &op, const std::string &arg) -> int {
		if(op == "b") {
			//memcpy(&m.mem[org], &arg[0], arg.size());
			m.writeRam(org, (uint8_t*)&arg[0], arg.size());
			return arg.size();
		}
		printf(">> %s %s\n", op.c_str(), arg.c_str());
		if(op[0] == '@') {
/*			//reqs[org] = arg;
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
			set_break(org, [=](Machine &m) {
					printf("Break at %04x\n", m.pc);
					for(auto &r : reqs) {
						int v = 0;
						switch(r.first) {
						case RA: v = m.a; break;
						case RX: v = m.x; break;
						case RY: v = m.y; break;
						case RSR: v = m.sr; break;
						default: v = m.mem[r.first]; break;
						}
						printf("COMP %d %d\n", v, r.second);
								
						if(v != r.second)
							throw run_exception(std::string("REQ Failed: ") + std::to_string(v) + " != " + std::to_string(r.second));
					}

			});
*/
			return 0;
		}
		uint8_t temp[4];
		int len = assemble(org, &temp[0], std::string(" ") + op + " " + arg);
		if(len > 0)
			m.writeRam(org, &temp[0], len);
		return len;
	});

	if(!ok) {
		printf("Parse failed\n");
		return -1;
	}
	//return 0;
	m.setPC(0x01000);
	//m.pc = 0x1000;
	m.run(1000);

	//printf("%02x\n", m.mem[0x2000]);
	return 0;
}
