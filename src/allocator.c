#include "allocator.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

void *
heap_alloc(usize n_bytes)
{
	void *rv = calloc(n_bytes, 1);
	if (!rv) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			"heap_alloc(n_bytes: %zu) failed", n_bytes);
		exit(1);
	}
	return rv;
}

void
heap_free(void *ptr)
{
	if (ptr) {
		free(ptr);
	}
}
