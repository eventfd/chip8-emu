#if !defined(CHIP_CONFIG)
#define CHIP_CONFIG

#include "typedefs.h"

struct config {
	char const *rom_file;
	u32	    dx;
	u32	    dy;
	u32	    clock_speed; /* Hz */
	u8	    verbose;
	bool	    disasm;
};

#endif // CHIP_CONFIG
