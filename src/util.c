#include "util.h"

void
asm_break(void)
{
	__asm__ volatile("int3");
}
