#if !defined(CHIP8_VM_H)
#define CHIP8_VM_H

#include "typedefs.h"

enum vm_event {
	EV_SOUND     = 1,
	EV_TIMER     = 2,
	EV_DRAW	     = 3,
	EV_CLS	     = 4,
	EV_KEY_PRESS = 5,
	EV_MAX,
};

enum vm_state {
	VM_IDLE	  = 0,
	VM_OUTPUT = 1,
	VM_INPUT  = 2,
};

struct vm {
	u8		       code[0xe00];
	u32		       code_size;
	u8		       regs[16];
	u8		       stack[64];
	u8		       fb[64][32];
	u16		       pc;
	u16		       ir;
	u16		       sp;
	u16		       kbd_r;
	i16		       delay_timer;
	i16		       sound_timer;
	_Atomic(enum vm_state) state;
};

typedef void (*vm_callback_fn)(
	struct vm const *vm, enum vm_event ev, void *arg);
void vm_step(struct vm *vm, vm_callback_fn callback);

#endif // CHIP8_VM_H
