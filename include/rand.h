#if !defined(CHIP8_RAND_H)
#define CHIP8_RAND_H

#include "typedefs.h"

void rnd_init(i64 seed);
u16  rnd_next_u16(void);

#endif // CHIP8_RAND_H
