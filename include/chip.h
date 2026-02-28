#if !defined(CHIP_H)
#define CHIP_H

#include "config.h"
#include "typedefs.h"
#include <SDL3/SDL.h>

struct context {
	SDL_RWLock   *rwlock;
	struct config config;
	u32	      user_event;
	u8	      r;
	u8	      g;
	u8	      b;
};

#endif // CHIP_H
