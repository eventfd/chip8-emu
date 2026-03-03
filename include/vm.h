#if !defined(CHIP8_VM_H)
#define CHIP8_VM_H

#include "typedefs.h"

enum vm_event {
	EV_TIMER     = 1,
	EV_DRAW	     = 2,
	EV_KEY_PRESS = 4,
	EV_MAX,
};

enum vm_state {
	VM_RESUME = 0,
	VM_WAIT	  = 1,
};

struct draw_params {
	u8 buf[16];
	u8 x;
	u8 y;
	u8 n;
};

struct vm {
	u8		       mem[0x1000];
	u32		       code_size;
	u8		       regs[16];
	u16		       stack[32];
	u8		       fb[32 * 64];
	u16		       pc;
	u16		       i;
	u16		       sp;
	i8		       kbd_r;
	u16		       delay_timer;
	_Atomic(u16)	       sound_timer;
	_Atomic(enum vm_state) state;
};

typedef void (*vm_callback_fn)(struct vm *vm, enum vm_event ev, void *arg);

void vm_init(struct vm *vm);
void vm_step(struct vm *vm, vm_callback_fn callback);

#endif // CHIP8_VM_H
