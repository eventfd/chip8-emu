#if !defined(CHIP_TYPES_H)
#define CHIP_TYPES_H

#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MKINT(size)                     \
	typedef int##size##_t  i##size; \
	typedef uint##size##_t u##size

MKINT(8);
MKINT(16);
MKINT(32);
MKINT(64);

#undef MKINT

typedef float  f32;
typedef double f64;

typedef intptr_t  isize;
typedef uintptr_t usize;

#define UNUSED(arg) ((void)(arg))

#define container_of(ptr, ctr_ty, ctr_member_name)                  \
	(ctr_ty *)((u8 *)(ptr) - offsetof(ctr_ty, ctr_member_name))

#endif // CHIP_TYPES_H
