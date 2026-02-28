#if !defined(CHIP8_ROM_H)
#define CHIP8_ROM_H

#include "chip.h"

enum status parse_rom(struct vm *vm, struct config const *cfg);

#endif // CHIP8_ROM_H
