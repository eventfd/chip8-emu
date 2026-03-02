#include "vm.h"
#include <string.h>

#define READ_INSTR(ptr, off)                                 \
	((u16)((ptr)[(off)]) << 8 | (u16)((ptr)[(off) + 1]))

typedef u8 sprite_t[5];

sprite_t sprites[] = {
	[8] = {0xf0, 0x90, 0xf0, 0x90, 0xf0},
};

void
vm_init(struct vm *vm)
{
	memset(vm, 0, sizeof(struct vm));
	vm->sound_timer = -1;
	vm->delay_timer = -1;
	vm->state	= VM_IDLE;
}

void
vm_step(struct vm *vm, vm_callback_fn callback)
{
	/*
	 * VM cannot proceed until the previous instruction is committed
	 */
	if (atomic_load(&vm->state) == VM_WAIT) {
		return;
	}
	if (vm->pc >= vm->code_size) {
		return;
	}

	u8	  imm, r;
	u16	  addr;
	u16 const raw_opcode = READ_INSTR(vm->code, vm->pc);
	vm->pc += 2;

	/* handlers */
	switch (raw_opcode) {
	case 0xe0:
		callback(vm, EV_CLS, nullptr);
		break;
	default:
		/* TODO */
		break;
	}
	switch ((raw_opcode >> 12) & 0xfu) {
	case 0x1:
		/* JP addr */
		addr   = raw_opcode & 0xfffu;
		vm->pc = addr - 0x200;
		break;
	case 0x3:
		/* SE Vx, byte */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		if (vm->regs[r] == imm) {
			vm->pc += 2;
		}
		break;
	case 0x6:
		/* LD Vx, imm */
		imm	    = raw_opcode & 0xffu;
		r	    = (raw_opcode >> 8) & 0x0fu;
		vm->regs[r] = imm;
		break;
	case 0x7:
		/* ADD Vx, imm */
		imm	    = raw_opcode & 0xffu;
		r	    = (raw_opcode >> 8) & 0x0fu;
		vm->regs[r] = (u8)((u16)imm + (u16)vm->regs[r]);
		break;
	case 0xa:
		/* LD I, addr */
		addr   = raw_opcode & 0xfffu;
		vm->ir = addr;
		break;
	case 0xf:
		switch (raw_opcode & 0xffu) {
		case 0xa:
			r	  = (raw_opcode >> 8) & 0xfu;
			vm->kbd_r = r;
			callback(vm, EV_KEY_PRESS, &vm->regs[r]);
			break;
		case 0x18:
			r		= (raw_opcode >> 8) & 0xfu;
			vm->sound_timer = vm->regs[r];
			break;
		case 0x29:
			r = (raw_opcode >> 8) & 0xfu;
			/* load sprite for digit regs[r] */
			break;
		default:
			break;
		}
	default:
		break;
	}
}
