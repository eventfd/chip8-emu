#include "chip.h"
#include <stdlib.h>

extern enum status chip_main(i32 argc, char *argv[const]);

i32
main(i32 argc, char *argv[const])
{
	static char const *const error_desc[] = {
#define ITER(v, text) [v] = text,
		DEF_STATUS(ITER)
#undef ITER
	};
	i32 const sv = (i32 const)chip_main(argc, argv);
	if (!sv) {
		return EXIT_SUCCESS;
	}
	if (sv < 0 || sv > E_GENERIC) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"Invalid Error Status: '%d'", sv);
		/* invalid return value */
	} else if (sv != E_ARG_PARSE) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"Emulation Failed due to '%s'", error_desc[sv]);
	}
	return EXIT_FAILURE;
}
