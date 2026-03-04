#include "vm.h"
#include "chip.h"
#include "rand.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

typedef void (*disasm_cb_fn)(char const *str);

static void disasm_instr(struct vm const *vm, disasm_cb_fn cb);
static void _puts(char const *str);
static void init_sprites(struct vm *vm);

#define READ_INSTR(ptr, off)                                 \
	((u16)((ptr)[(off)]) << 8 | (u16)((ptr)[(off) + 1]))

void
vm_init(struct vm *vm, u8 const *code, u32 code_len)
{
	memset(vm, 0, sizeof(struct vm));
	vm->kbd_r     = -1;
	vm->state     = VM_RESUME;
	vm->pc	      = 0x200;
	vm->code_size = code_len;
	memcpy(&vm->mem[0x200], code, code_len);
	init_sprites(vm);
}

void
vm_step(struct vm *vm, vm_cb_fn callback)
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
		disasm_instr(vm, _puts);
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
	case 0xe:
		switch (raw_opcode & 0x0ff) {
		case 0x09eu:
			/* SKP Vx */
			r = (raw_opcode >> 8) & 0x0f;
			break;
		case 0x0A1u:
			/* SKNP Vx */
			r = (raw_opcode >> 8) & 0x0f;
			break;
		}
		break;
	case 0xf:
		switch (raw_opcode & 0xffu) {
		case 0x7:
			/* LD Vx, DT */
			r	    = (raw_opcode >> 8) & 0xfu;
			vm->regs[r] = vm->delay_timer;
			break;
		case 0xa:
			/* LD Vx, K */
			r	  = (raw_opcode >> 8) & 0xfu;
			vm->kbd_r = (i8)r;
			callback(vm, EV_KEY_PRESS, &vm->regs[r]);
			break;
		case 0x15:
			/* LD DT, Vx */
			r		= (raw_opcode >> 8) & 0xfu;
			vm->delay_timer = vm->regs[r];
			break;
		case 0x18:
			/* LD ST, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			atomic_store(&vm->sound_timer, vm->regs[r]);
			break;
		case 0x1e:
			/* ADD I, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			vm->i += (u16)vm->regs[r];
			break;
		case 0x29:
			/* LD F, Vx */
			r     = (raw_opcode >> 8) & 0xfu;
			vm->i = 0x5u * (u16)vm->regs[r];
			break;
		case 0x33:
			/* LD B, Vx */
			r = vm->regs[(raw_opcode >> 8) & 0xfu];
			for (u32 i = 0; i != 3; i++) {
				vm->mem[2 - i + vm->i] = r % 10;
				r /= 10;
			}
			break;
		case 0x55:
			/* LD [I], Vx */
			r = (raw_opcode >> 8) & 0xfu;
			if (vm->i + (u16)r + 1 >= sizeof(vm->mem)) {
				break;
			}
			memcpy(&vm->mem[vm->i], &vm->regs[0], r + 1);
			vm->i += r + 1;
			break;
		case 0x65:
			/* LD Vx, [I] */
			r = (raw_opcode >> 8) & 0xfu;
			if (vm->i + (u16)r + 1 >= sizeof(vm->mem)) {
				break;
			}
			memcpy(&vm->regs[0], &vm->mem[vm->i], r + 1);
			vm->i += r + 1;
			break;
		default:
			break;
		}
	default:
		break;
	}
}

void
disasm_instr(struct vm const *vm, disasm_cb_fn cb)
{
	static char buffer[128];
	memset(buffer, 0, sizeof buffer);

	u8  r, imm, d;
	u16 addr;

	snprintf(buffer, 127, "%03x   ", vm->pc - 2);
	u16 raw_opcode = READ_INSTR(vm->mem, vm->pc - 2);

	switch (raw_opcode) {
	case 0x0e0u:
		strncat(buffer, "CLS", 4);
		cb(buffer);
		return;
	case 0x0eeu:
		strncat(buffer, "RET", 4);
		cb(buffer);
		return;
	}
	switch ((raw_opcode >> 12) & 0x0fu) {
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
		switch (raw_opcode & 0x0ffu) {
		case 0x7:
			/* LD Vx, DT */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s V%u, DT", "LD", r);
			break;
		case 0xa:
			/* LD Vx, K */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s V%u, K", "LD", r);
			break;
		case 0x15:
			/* LD DT, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s DT, V%u", "LD", r);
			break;
		case 0x18:
			/* LD ST, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s I, V%u", "ADD", r);
			break;
		case 0x1e:
			/* LD ST, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s ST, V%u", "LD", r);
			break;
		case 0x29u:
			/* LD F, Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s F, V%u", "LD", r);
			break;
		case 0x55:
			/* LD [I], Vx */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s [I], V%u", "LD", r);
			break;
		case 0x65:
			/* LD Vx, [I] */
			r = (raw_opcode >> 8) & 0xfu;
			snprintf(buffer + 6, 121, "%-4s V%u, [I]", "LD", r);
			break;
		default:
			break;
		}
	default:
		snprintf(buffer + 6, 121, "(unhandled) %04x", raw_opcode);
		break;
	}

	cb(buffer);
}

void
_puts(char const *str)
{
	(void)puts(str);
}

void
init_sprites(struct vm *vm)
{
	static u8 sprites[][5] = {
		{0xf0, 0x90, 0x90, 0x90, 0xf0}, /* 0 */
		{0x20, 0x60, 0x20, 0x20, 0x70}, /* 1 */
		{0xf0, 0x10, 0xf0, 0x80, 0xf0}, /* 2 */
		{0xf0, 0x10, 0xf0, 0x10, 0xf0}, /* 3 */
		{0x90, 0x90, 0xf0, 0x10, 0x10}, /* 4 */
		{0xf0, 0x80, 0xf0, 0x10, 0xf0}, /* 5 */
		{0xf0, 0x80, 0xf0, 0x90, 0xf0}, /* 6 */
		{0xf0, 0x10, 0x20, 0x40, 0x40}, /* 7 */
		{0xf0, 0x90, 0xf0, 0x90, 0xf0}, /* 8 */
		{0xf0, 0x90, 0xf0, 0x10, 0xf0}, /* 9 */
		{0xf0, 0x90, 0xf0, 0x90, 0x90}, /* A */
		{0xf0, 0x90, 0xe0, 0x90, 0xe0}, /* B */
		{0xf0, 0x80, 0x80, 0x80, 0xf0}, /* C */
		{0xe0, 0x90, 0x90, 0x90, 0xe0}, /* D */
		{0xf0, 0x80, 0xf0, 0x80, 0xf0}, /* E */
		{0xf0, 0x80, 0xf0, 0x80, 0x80}, /* F */
	};

	for (u32 i = 0; i != 16; i++) {
		memcpy(&vm->mem[i * 5], sprites[i], 5);
	}
}
