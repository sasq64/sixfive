#include "emulator.h"
#include "assembler.h"

int main(int argc, char **argv)
{
	using namespace sixfive;
	Machine m;
	init(m);
	assemble(0x1000, &m.mem[0x1000],
R"(
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

	m.pc = 0x1000;
	run(m, 1000);

	printf("%02x\n", m.mem[0x2000]);
}
