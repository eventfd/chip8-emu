#if !defined(CHIP_CONFIG)
#define CHIP_CONFIG

#include "typedefs.h"

struct config {
	char const *rom_file;
	u32	    width;
	u32	    height;
	u32	    clock_speed; /* Hz */
	u8	    verbose;
};

#endif // CHIP_CONFIG
