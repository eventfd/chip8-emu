#include "vm.h"
#include "chip.h"
#include "rand.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

static void disasm_instr(struct vm const *vm);

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
	vm->kbd_r = -1;
	vm->state = VM_RESUME;
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
	if (vm->pc >= vm->code_size + 0x200) {
		return;
	}

	u8	  imm, r, d;
	u16	  addr;
	i16	  tmp;
	bool	  mask;
	u16 const raw_opcode = READ_INSTR(vm->mem, vm->pc);
	vm->pc += 2;

	struct context const *ctx
		= (struct context const *)container_of(vm, struct context, vm);
	if (ctx->config.disasm) {
		disasm_instr(vm);
	}

	/* handlers */
	switch (raw_opcode) {
	case 0xe0:
		/* CLS */
		memset(vm->fb, 0, sizeof(vm->fb));
		callback(vm, EV_DRAW, nullptr);
		break;
	case 0xee:
		/* RET */
		if (vm->sp) {
			vm->sp -= 1;
			vm->pc = vm->stack[vm->sp];
		}
		break;
	default:
		break;
	}
	switch ((raw_opcode >> 12) & 0xfu) {
	case 0x1:
		/* JP addr */
		addr   = raw_opcode & 0xfffu;
		vm->pc = addr;
		break;
	case 0x2:
		/* CALL addr */
		addr = raw_opcode & 0xfffu;
		if (vm->sp != 32) {
			vm->stack[vm->sp] = vm->pc;
			vm->sp += 1;
			vm->pc = addr;
		}
		break;
	case 0x3:
		/* SE Vx, byte */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		if (vm->regs[r] == imm) {
			vm->pc += 2;
		}
		break;
	case 0x4:
		/* SNE Vx, byte */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		if (vm->regs[r] != imm) {
			vm->pc += 2;
		}
		break;
	case 0x5:
		/* SE Vx, Vy */
		r = (raw_opcode >> 4) & 0x0fu;
		d = (raw_opcode >> 8) & 0x0fu;
		if (vm->regs[d] == vm->regs[r]) {
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
		imm	      = raw_opcode & 0xffu;
		r	      = (raw_opcode >> 8) & 0x0fu;
		tmp	      = (i16)imm + (i16)vm->regs[r];
		vm->regs[r]   = (u8)(tmp & 0x0ffu);
		vm->regs[0xf] = (u8)((tmp >> 8) & 0xffu);
		break;
	case 0x8:
		switch (raw_opcode & 0xfu) {
		case 0:
			/* LD Vx, Vy */
			r	    = (raw_opcode >> 4) & 0x0fu;
			d	    = (raw_opcode >> 8) & 0x0fu;
			vm->regs[d] = vm->regs[r];
			break;
		case 1:
			/* OR Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			vm->regs[d] |= vm->regs[r];
			break;
		case 2:
			/* AND Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			vm->regs[d] &= vm->regs[r];
			break;
		case 3:
			/* XOR Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			vm->regs[d] ^= vm->regs[r];
			break;
		case 4:
			/* ADD Vx, Vy */
			r	      = (raw_opcode >> 4) & 0x0fu;
			d	      = (raw_opcode >> 8) & 0x0fu;
			tmp	      = (i16)vm->regs[d] + (i16)vm->regs[r];
			vm->regs[d]   = (u8)(tmp & 0xffu);
			vm->regs[0xf] = (u8)((tmp >> 8) & 0xffu);
			break;
		case 5:
			/* SUB Vx, Vy */
			r	      = (raw_opcode >> 4) & 0x0fu;
			d	      = (raw_opcode >> 8) & 0x0fu;
			tmp	      = (i16)vm->regs[d] - (i16)vm->regs[r];
			vm->regs[d]   = (u8)(tmp & 0xffu);
			vm->regs[0xf] = (u8)((tmp >> 8) & 0xffu); /* borrow */
			break;
		case 6:
			/* SHR Vx, Vy */
			r	      = (raw_opcode >> 4) & 0x0fu;
			d	      = (raw_opcode >> 8) & 0x0fu;
			vm->regs[0xf] = vm->regs[d] & 0x01;
			vm->regs[d] >>= 1;
			break;
		case 7:
			/* SUBN Vx, Vy */
			r	      = (raw_opcode >> 4) & 0x0fu;
			d	      = (raw_opcode >> 8) & 0x0fu;
			tmp	      = (i16)vm->regs[r] - (i16)vm->regs[d];
			vm->regs[d]   = (u8)(tmp & 0xffu);
			vm->regs[0xf] = (u8)((tmp >> 8) & 0xffu); /* borrow */
			break;
		case 0xe:
			/* SHL Vx, Vy */
			r	      = (raw_opcode >> 4) & 0x0fu;
			d	      = (raw_opcode >> 8) & 0x0fu;
			vm->regs[0xf] = ((u16)vm->regs[d] >> 7) & 0x01;
			vm->regs[d] <<= 1;
			break;
		}
		break;
	case 0x9:
		/* SNE Vx, Vy */
		r = (raw_opcode >> 4) & 0x0fu;
		d = (raw_opcode >> 8) & 0x0fu;
		if (vm->regs[d] != vm->regs[r]) {
			vm->pc += 2;
		}
		break;
	case 0xa:
		/* LD I, addr */
		addr  = raw_opcode & 0xfffu;
		vm->i = addr;
		break;
	case 0xb:
		/* JP V0, addr */
		addr   = raw_opcode & 0xfffu;
		vm->pc = addr + (u16)vm->regs[0];
		break;
	case 0xc:
		/* RND Vx, imm */
		imm	    = raw_opcode & 0xffu;
		r	    = (raw_opcode >> 8) & 0x0fu;
		vm->regs[r] = (u8)(imm & rnd_next_u16());
		break;
	case 0xd:
		/* DRW Vx, Vy, imm4 */
		imm = raw_opcode & 0x0fu;
		r   = vm->regs[(raw_opcode >> 4) & 0x0fu];
		d   = vm->regs[(raw_opcode >> 8) & 0x0fu];
		if (vm->i + imm >= sizeof(vm->mem)) {
			break;
		}
		for (u32 y = 0; y != imm; y++) {
			for (u32 x = 0; x != 8; x++) {
				tmp  = (r + y) % 32 * 64 + (d + x) % 64;
				mask = (vm->mem[vm->i + y] >> (7 - x)) & 0x1;
				if (vm->fb[tmp] != (0xffu * mask)) {
					/* collision */
					vm->regs[0xf] = 1;
				}
				vm->fb[tmp] ^= 0xffu * mask;
			}
		}
		callback(vm, EV_DRAW, nullptr);
		break;
	case 0xf:
		switch (raw_opcode & 0xffu) {
		case 0x7:
			/* LD Vx, DT */
			r		= (raw_opcode >> 8) & 0xfu;
			vm->delay_timer = vm->regs[r];
			break;
		case 0xa:
			/* LD Vx, K */
			r	  = (raw_opcode >> 8) & 0xfu;
			vm->kbd_r = (i8)r;
			callback(vm, EV_KEY_PRESS, &vm->regs[r]);
			break;
		case 0x18:
			/* LD ST, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			atomic_store(&vm->sound_timer, vm->regs[r]);
			break;
		case 0x1e:
			/* ADD I, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			vm->i += (u16)r;
			break;
		case 0x29:
			r = (raw_opcode >> 8) & 0xfu;
			/* TODO: load sprite for digit regs[r] */
			break;
		case 0x55:
			/* LD [I], Vx */
			r = (raw_opcode >> 8) & 0xfu;
			if (vm->i + (u16)r >= sizeof(vm->mem)) {
				break;
			}
			memcpy(&vm->mem[vm->i], &vm->regs[0], r);
			break;
		case 0x65:
			/* LD Vx, [I] */
			r = (raw_opcode >> 8) & 0xfu;
			if (vm->i + (u16)r >= sizeof(vm->mem)) {
				break;
			}
			memcpy(&vm->regs[0], &vm->mem[vm->i], r);
			break;
		default:
			break;
		}
	default:
		break;
	}
}

void
disasm_instr(struct vm const *vm)
{
	static char buffer[128];
	memset(buffer, 0, sizeof buffer);

	u8  r, imm, d;
	u16 addr;

	snprintf(buffer, 127, "%03x   ", vm->pc - 2);
	u16 raw_opcode = READ_INSTR(vm->mem, vm->pc - 2);

	switch (raw_opcode) {
	case 0xe0:
		strncat(buffer, "CLS", 4);
		break;
	case 0xee:
		strncat(buffer, "RET", 4);
		break;
	default:
		break;
	}
	switch ((raw_opcode >> 12) & 0xfu) {
	case 0x1:
		/* JP addr */
		addr = raw_opcode & 0xfffu;
		snprintf(buffer + 6, 121, "%-4s @%03x", "JP", addr);
		break;
	case 0x2:
		/* CALL addr */
		addr = raw_opcode & 0xfffu;
		snprintf(buffer + 6, 121, "%-4s @%03x", "CALL", addr);
		break;
	case 0x3:
		/* SE Vx, byte */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, #%02x", "SE", r, imm);
		break;
	case 0x4:
		/* SNE Vx, byte */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, #%02x", "SNE", r, imm);
		break;
	case 0x5:
		/* SE Vx, Vy */
		r = (raw_opcode >> 4) & 0x0fu;
		d = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, V%u", "SE", d, r);
		break;
	case 0x6:
		/* LD Vx, imm */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, #%02x", "LD", r, imm);
		break;
	case 0x7:
		/* ADD Vx, imm */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, #%02x", "ADD", r, imm);
		break;
	case 0x8:
		switch (raw_opcode & 0xfu) {
		case 0:
			/* LD Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "LD", d, r);
			break;
		case 1:
			/* OR Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "OR", d, r);
			break;
		case 2:
			/* AND Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "AND", d, r);
			break;
		case 3:
			/* XOR Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "XOR", d, r);
			break;
		case 4:
			/* ADD Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "ADD", d, r);
			break;
		case 5:
			/* SUB Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "SUB", d, r);
			break;
		case 6:
			/* SHR Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "SHR", d, r);
			break;
		case 7:
			/* SUBN Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(
				buffer + 6, 121, "%-6s4V%u, V%u", "SUBN", d, r);
			break;
		case 0xe:
			/* SHL Vx, Vy */
			r = (raw_opcode >> 4) & 0x0fu;
			d = (raw_opcode >> 8) & 0x0fu;
			snprintf(buffer + 6, 121, "%-4s V%u, V%u", "SHL", d, r);
			break;
		}
		break;
	case 0x9:
		/* SNE Vx, Vy */
		r = (raw_opcode >> 4) & 0x0fu;
		d = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, V%u", "SNE", d, r);
		break;
	case 0xa:
		/* LD I, addr */
		addr = raw_opcode & 0xfffu;
		snprintf(buffer + 6, 121, "%-4s I, @%03x", "LD", addr);
		break;
	case 0xb:
		/* JP V0, addr */
		addr = raw_opcode & 0xfffu;
		snprintf(buffer + 6, 121, "%-4s V0, @%03x", "JP", addr);
		break;
	case 0xc:
		/* RND Vx, imm */
		imm = raw_opcode & 0xffu;
		r   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, #%02x", "RND", r, imm);
		break;
	case 0xd:
		/* DRW Vx, Vy, imm4 */
		imm = raw_opcode & 0x0fu;
		r   = (raw_opcode >> 4) & 0x0fu;
		d   = (raw_opcode >> 8) & 0x0fu;
		snprintf(buffer + 6, 121, "%-4s V%u, V%u, %#01x", "DRW", d, r,
			imm);
		break;
	case 0xf:
		switch (raw_opcode & 0xffu) {
		case 0x7:
			/* LD Vx, DT */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s V%u", "DT", r);
			break;
		case 0xa:
			/* LD Vx, K */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s V%u, K", "LD", r);
			break;
		case 0x18:
			/* LD ST, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s ST, V%u", "LD", r);
			break;
		case 0x29:
			r = (raw_opcode >> 8) & 0xfu;
			/* TODO: load sprite for digit regs[r] */
			snprintf(buffer + 6, 121, "%-4s F, V%u", "LD", r);
			break;
		default:
			break;
		}
	default:
		break;
	}

	puts(buffer);
}
