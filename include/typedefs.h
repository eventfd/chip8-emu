#if !defined(CHIP_TYPES_H)
#define CHIP_TYPES_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define _DEF_INT(size)                  \
	typedef int##size##_t  i##size; \
	typedef uint##size##_t u##size

_DEF_INT(8);
_DEF_INT(16);
_DEF_INT(32);
_DEF_INT(64);

#undef _DEF_INT

typedef float  f32;
typedef double f64;

typedef intptr_t  isize;
typedef uintptr_t usize;

#define nullptr NULL

#endif // CHIP_TYPES_H
