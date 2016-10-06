#include "emulator.h"
#include "assembler.h"

#include <functional>

bool parse(const std::string &code, 
		std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode);

int main(int argc, char **argv)
{
	using namespace sixfive;
	Machine m;
	init(m);



	FILE *fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	auto source = std::make_unique<char[]>(size+1);
	fread(&source[0], 1, size, fp);
	source[size] = 0;
	fclose(fp);

	parse(&source[0], [&](uint16_t org, const std::string &op, const std::string &arg) -> int {
		printf(">> %s %s\n", op.c_str(), arg.c_str());
		int len = assemble(org, &m.mem[org], std::string(" ") + op + " " + arg);
		return len;
	});
/*


	auto code = R"(
	* = 0x1000
YO = 88
	lda #YO
    sec
loop:
    lda #$3f
    adc #$40
	lda #$05
	beq	done
	tax
	sta $2000
	dex
	bne loop
	clc
	adc #$47
	ldx #3
	bcc $1003
done:
	rts
)";

	assemble(0x1000, &m.mem[0x1000],
R"(
    sec
    lda #$3f
    adc #$40
	lda #$05
	tax
	sta $2000
	dex
	bne $1003
	clc
	adc #$47
	ldx #3
	bcc $1003
	rts
)");
*/
	m.pc = 0x1000;
	run(m, 1000);

	printf("%02x\n", m.mem[0x2000]);
	return 0;
}
