#if !defined(CHIP_H)
#define CHIP_H

#include "config.h"
#include "typedefs.h"
#include "vm.h"
#include <SDL3/SDL.h>

#define DEF_STATUS(F)                                                \
	F(E_OK, "Ok")                                                \
	F(E_ARG_PARSE, "Argument Parsing failed")                    \
	F(E_SDL_INIT, "SDL initialization failed")                   \
	F(E_SDL_WIN_INIT, "SDL Window creation failed")              \
	F(E_SDL_TEXTURE_INIT, "SDL Texture creation failed")         \
	F(E_SDL_AUDIO_INIT, "SDL Audio initialization failed")       \
	F(E_FILE_PERM_READ, "Insufficient permissions to read file") \
	F(E_FILE_PATH_INV, "Invalid file path")                      \
	F(E_FILE_EXIST, "File does not exist")                       \
	F(E_FILE_SEEKABLE, "File is not seekable")                   \
	F(E_FILE_TOO_LARGE, "ROM file too large (> 3584 bytes)")     \
	F(E_FILE_TOO_SMALL, "ROM file too small")                    \
	F(E_FILE_SIZE_MISMATCH, "ROM file size mismatch")            \
	F(E_GENERIC, "I/O Error")

enum status {
#define ITER(v, _) v,
	DEF_STATUS(ITER)
#undef ITER
};

struct context {
	SDL_RWLock	*rwlock;
	SDL_Window	*window;
	SDL_Renderer	*renderer;
	SDL_AudioStream *audio_stream;
	SDL_Texture	*texture;
	struct config	 config;
	u32		 user_event;
	struct vm	 vm;
};

#endif // CHIP_H
