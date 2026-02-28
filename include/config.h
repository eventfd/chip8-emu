#if !defined(CHIP_CONFIG)
#define CHIP_CONFIG

#include "typedefs.h"

struct config {
	u32	    window_width;
	u32	    window_height;
	u32	    clock_speed; /* Hz */
	char const *rom_file;
};

enum event_type {
	EV_CLOCK_TICK = 1,
	EV_PAINT      = 2,
};

#endif // CHIP_CONFIG
