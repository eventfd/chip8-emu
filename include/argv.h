#if !defined(CHIP8_ARGV_H)
#define CHIP8_ARGV_H

#include "chip.h"

enum status parse_argv(struct config *config, i32 argc, char *argv[const]);

#endif // CHIP8_ARGV_H
