#if !defined(CHIP8_UTIL_H)
#define CHIP8_UTIL_H

void asm_break(void);

#define _STR(x) #x
#define STR(x)	_STR(x)

#define LOG_ERROR(ty, fmt, ...)                                           \
	SDL_LogError(ty, __FILE__ ":" STR(__LINE__) " " fmt, __VA_ARGS__)

#define LOG_INFO(fmt, ...)                                       \
	SDL_Log(__FILE__ ":" STR(__LINE__) " " fmt, __VA_ARGS__)

#endif // CHIP8_UTIL_H
