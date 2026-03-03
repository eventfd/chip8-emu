#if !defined(CHIP8_ALLOC_H)
#define CHIP8_ALLOC_H

#include "typedefs.h"

void *heap_alloc(usize n_bytes);
void  heap_free(void *ptr);

#endif // CHIP8_ALLOC_H
