#include "rand.h"

static i64 _seed;
static i64 _scale  = 0xdead;
static i64 _offset = 0xface;

void
rnd_init(i64 seed)
{
	_seed = seed;
}

u16
rnd_next_u16(void)
{
	_seed = _seed * _scale + _offset;
	return (u16)(_seed & INT32_C(0x0ffff));
}
