#include "sixfive.h"

const Instruction instructions[] = {
	{ "lda", 
		{
			{0xa9, 2, IMM},
			{0xa5, 2, ZP},
			{0xb5, 4, ZP_X},
			{0xad, 4, ABS},
			{0xbd, 4, ABS_X},
			{0xb9, 4, ABS_Y},
			{0xa1, 6, IND_X},
			{0xb1, 5, IND_Y},
		}, [](Machine &m, uint8_t *ea)
		{
			m.a = *ea;
			m.sr = (m.sr & 0x7d) | (m.a & SR_S) | (m.a == 0 ? SR_Z : 0x0);
		}
	},

	{ "ldx", 
		{
			{0xa2, 2, IMM},
			{0xa6, 2, ZP},
			{0xb6, 4, ZP_Y},
			{0xae, 4, ABS},
			{0xbe, 4, ABS_Y},
		}, [](Machine &m, uint8_t *ea)
		{
			m.x = *ea;
			m.sr = (m.sr & 0x7d) | (m.x & SR_S) | (m.x == 0 ? SR_Z : 0x0);
		}
	},

	{ "ldy", 
		{
			{0xa0, 2, IMM},
			{0xa4, 2, ZP},
			{0xb4, 4, ZP_X},
			{0xac, 4, ABS},
			{0xbc, 4, ABS_X},
		}, [](Machine &m, uint8_t *ea)
		{
			m.y = *ea;
			m.sr = (m.sr & 0x7d) | (m.y & SR_S) | (m.y == 0 ? SR_Z : 0x0);
		}
	},

	{ "sec",
		{
			{0x38, 2, NONE},
		}, [](Machine &m, uint8_t *ea)
		{
			m.sr |= 0x01;
		}
	},

	{ "clc",
		{
			{0x18, 2, NONE},
		}, [](Machine &m, uint8_t *ea)
		{
			m.sr &= 0xfe;
		}
	},

	{ "inx",
		{
			{0xe8, 2, NONE},
		}, [](Machine &m, uint8_t *ea)
		{
			m.x++;
			m.sr = (m.sr & 0x7d) | (m.x & SR_S) | (m.x == 0 ? SR_Z : 0x0);
		}
	},

	{ "adc",
		{
			{0x69, 2, IMM},
			{0x65, 3, ZP},
			{0x75, 4, ZP_X},
			{0x6d, 4, ABS},
			{0x7d, 4, ABS_X},
			{0x79, 4, ABS_Y},
			{0x61, 6, IND_X},
			{0x71, 5, IND_Y},
		}, [](Machine &m, uint8_t *ea)
		{
			unsigned rc = m.a + *ea + (m.sr & 1);
			m.sr = (m.sr & 0x7c) | (rc>>8) |  (rc & SR_S) | (rc == 0 ? SR_Z : 0x0);
			m.a = rc & 0xff;
		}
	},

	{ "and",
		{
			{0x29, 2, IMM},
			{0x25, 3, ZP},
			{0x35, 4, ZP_X},
			{0x2d, 4, ABS},
			{0x3d, 4, ABS_X},
			{0x39, 4, ABS_Y},
			{0x21, 6, IND_X},
			{0x31, 5, IND_Y},
		}, [](Machine &m, uint8_t *ea)
		{
			m.a = m.a & *ea;
			m.sr = (m.sr & 0x7d) | (m.a & SR_S) | (m.a == 0 ? SR_Z : 0x0);
		}
	},

	{ "asl",
		{
			{0x0a, 2, ACC},
		}, [](Machine &m, uint8_t *ea)
		{
			int rc = m.a << 1;
			m.sr = (m.sr & 0x7c) | (rc>>8) |  (rc & SR_S) | (rc == 0 ? SR_Z : 0x0);
			m.a = rc & 0xff;
		}
	},

	{ "asl",
		{
			{0x06, 5, ZP},
			{0x16, 6, ZP_X},
			{0x0e, 6, ABS},
			{0x1e, 7, ABS_X},
		}, [](Machine &m, uint8_t *ea)
		{
			int rc = *ea << 1;
			m.sr = (m.sr & 0x7c) | (rc>>8) |  (rc & SR_S) | (rc == 0 ? SR_Z : 0x0);
			*ea = rc & 0xff;
		}
	},

	{ "bit",
		{
			{0x24, 3, ZP},
			{0x2c, 4, ABS},
		}, [](Machine &m, uint8_t *ea)
		{
			int rc = m.a & *ea;
			m.sr = (m.sr & 0x7c) | (rc>>8) |  (rc & SR_S) | (rc == 0 ? SR_Z : 0x0);
		}
	},

	{ "bpl", { {0x10, 3, REL},
		}, [](Machine &m, uint8_t *ea)
		{
			if(!(m.sr & SR_S)) {
				m.pc += *ea;
				m.cycles++;
			}
		}
	},
	{ "bmi", { {0x30, 3, REL},
		}, [](Machine &m, uint8_t *ea)
		{
			if(m.sr & SR_S) {
				m.pc += *ea;
				m.cycles++;
			}
		}
	},

	{ "bne", { {0xd0, 3, REL},
		}, [](Machine &m, uint8_t *ea)
		{
			if(!(m.sr & SR_Z)) {
				m.pc += *(int8_t*)ea;
				m.cycles++;
			}
		}
	},
	{ "beq", { {0xf0, 3, REL},
		}, [](Machine &m, uint8_t *ea)
		{
			if(m.sr & SR_Z) {
				m.pc += *(int8_t*)ea;
				m.cycles++;
			}
		}
	},

	{ "brk", { {0x00, 7, NONE},
		}, [](Machine &m, uint8_t *ea)
		{
			//m.nmi = true;
		}
	},

	{ "cmp",
		{
			{0xc9, 2, IMM},
			{0xc5, 3, ZP},
			{0xd5, 4, ZP_X},
			{0xcd, 4, ABS},
			{0xdd, 4, ABS_X},
			{0xd9, 4, ABS_Y},
			{0xc1, 6, IND_X},
			{0x31, 5, IND_Y},
		}, [](Machine &m, uint8_t *ea)
		{
			int rc = m.a - *ea;
			m.sr = (m.sr & 0x7d) | (m.a & SR_S) | (m.a == 0 ? SR_Z : 0x0);
		}
	},


	{ "sta", 
		{
			{0x85, 3, ZP },
			{0x95, 4, ZP_X},
			{0x8d, 4, ABS },
			{0x9d, 4, ABS_X },
			{0x99, 4, ABS_Y },
			{0x81, 4, IND_X },
			{0x91, 4, IND_Y },
		}, [](Machine &m, uint8_t *ea)
		{
			*ea = m.a;
		}
	},

	{ "jsr", 
		{
			{0x20, 6, ABS },
		}, [](Machine &m, uint8_t *ea)
		{
			m.stack[m.sp-- & 0xff] = m.pc & 0xff;
			m.stack[m.sp-- & 0xff] = m.pc >> 8; 
			m.pc = m.mem[*ea] | (m.mem[ea[1]]<<8);
		}
	},

	{ "jmp", 
		{
			{0x4C, 3, ABS },
			{0x6C, 5, IMM /* Really IND */ },
		}, [](Machine &m, uint8_t *ea)
		{
			// TODO: Incorrect, needs to wrap around to page
			m.pc = m.mem[*ea] | (m.mem[ea[1]]<<8);
		}
	},

	{ "rts", 
		{
			{0x60, 6, NONE },
		}, [](Machine &m, uint8_t *)
		{
			m.pc = m.stack[m.sp+1] | (m.stack[m.sp+2]<<8);
			m.sp += 2;
		}
	},
};
