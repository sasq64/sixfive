Instruction instructions[] = {
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
			m.sr = (m.sr & 0x7d) | (m.a & 0x80) | (m.a == 0 ? 0x2 : 0x0);
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
			m.sr = (m.sr & 0x7d) | (m.x & 0x80) | (m.x == 0 ? 0x2 : 0x0);
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
			m.sr = (m.sr & 0x7d) | (m.y & 0x80) | (m.y == 0 ? 0x2 : 0x0);
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

	{ "adc",
		{
			{0x69, 3, IMM},
		}, [](Machine &m, uint8_t *ea)
		{
			unsigned rc = m.a + *ea + (m.sr & 1);
			m.sr = (m.sr & 0x7c) | (rc>>8) |  (rc & 0x80) | (rc == 0 ? 0x2 : 0x0);
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
			printf("STA to %04x\n", ea - m.mem);
			*ea = m.a;
		}
	},

	{ "rts", 
		{
			{0x60, 2, NONE },
		}, [](Machine &m, uint8_t *)
		{
			m.pc = m.mem[ m.mem[m.sp] | (m.mem[m.sp-1]<<8) ];
			m.sp -= 2;
		}
	},
};
