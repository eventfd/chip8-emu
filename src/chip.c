#include "chip.h"
#include "argv.h"
#include "config.h"
#include "rom.h"
#include "vm.h"
#include <SDL3/SDL.h>

#define HZ_TO_NS(hz) (UINT64_C(1000000000) / (hz))

static u64  _clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval);
static void _vm_on_event(struct vm const *vm, enum vm_event ev, void *arg);
static void _put_event(i32 code, void *data1, void *data2);
static void _handle_event(struct context *ctx, SDL_UserEvent const *event);

enum status
chip_main(i32 argc, char *argv[const])
{
	enum status    sv    = 0;
	struct context ctx   = {0};
	SDL_Event      event = {0};

	sv = parse_argv(&ctx.config, argc, argv);
	if (sv != E_OK) {
		goto _exit;
	}

	if (ctx.config.verbose) {
		SDL_Log("ROM File: %s", ctx.config.rom_file);
		SDL_Log("Width: %u", ctx.config.window_width);
		SDL_Log("Height: %u", ctx.config.window_height);
		SDL_Log("Clock (Hz): %u", ctx.config.clock_speed);
	}

	sv = parse_rom(&ctx.vm, &ctx.config);
	if (sv != E_OK) {
		goto _exit;
	}

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		sv = E_SDL_INIT;
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_Init failed: %s", SDL_GetError());
		goto _exit;
	}

	ctx.user_event = SDL_RegisterEvents(1);
	ctx.rwlock     = SDL_CreateRWLock();

	bool const rv = SDL_CreateWindowAndRenderer("CHIP8 Emulator",
		ctx.config.window_width, ctx.config.window_height,
		SDL_WINDOW_HIGH_PIXEL_DENSITY, &ctx.window, &ctx.renderer);
	if (!rv) {
		sv = E_SDL_WIN_INIT;
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateWindowAndRenderer failed: %s",
			SDL_GetError());
		goto _clean_exit;
	}

	SDL_Texture *const texture = SDL_CreateTexture(ctx.renderer,
		SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
		ctx.config.window_height, ctx.config.window_height);
	if (!texture) {
		sv = E_SDL_TEXTURE_INIT;
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateTexture failed: %s", SDL_GetError());
		goto _exit_loop;
	}

	ctx.texture = texture;
	/* clear the screen first */
	_vm_on_event(&ctx.vm, EV_CLS, nullptr);

	SDL_AddTimerNS(
		HZ_TO_NS(ctx.config.clock_speed), _clock_pulse, (void *)&ctx);

	/* start the event loop */
	for (;;) {
		if (!SDL_WaitEvent(&event)) {
			continue;
		}
		if (event.type == SDL_EVENT_QUIT) {
			goto _exit_loop;
		} else if (event.type == ctx.user_event) {
			_handle_event(&ctx, &event.user);
		} else if (event.type == SDL_EVENT_KEY_UP) {
			_handle_event(&ctx, &(SDL_UserEvent){
						    .type  = SDL_EVENT_USER,
						    .code  = EV_KEY_PRESS,
						    .data1 = &event.key,
					    });
		}
	}

_exit_loop:
	SDL_DestroyRenderer(ctx.renderer);
	SDL_DestroyWindow(ctx.window);

_clean_exit:
	SDL_DestroyRWLock(ctx.rwlock);
	SDL_Quit();

_exit:
	return sv;
}

void
_put_event(i32 code, void *data1, void *data2)
{
	SDL_PushEvent(&(SDL_Event) {
		.user = {
			.type = SDL_EVENT_USER,
			.code = code,
			.data1 = data1,
			.data2 = data2,
		},
	});
}

u64
_clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval)
{
	struct context *const ctx = (struct context *)userdata;

	/* evaluate sound timer */
	i16 const sound_timer = ctx->vm.sound_timer;
	if (sound_timer >= 0) {
		sound_timer == 0 ? _put_event(EV_SOUND, nullptr, nullptr)
				 : (void)0;
		ctx->vm.sound_timer = sound_timer - 1;
	}

	/* run the cpu */
	vm_step(&ctx->vm, _vm_on_event);
	if (ctx->config.verbose >= 2) {
		SDL_Log("> Clock Event at %" PRIu64 "  ms", SDL_GetTicks());
	}

	return HZ_TO_NS(ctx->config.clock_speed);
}

void
_vm_on_event(struct vm const *vm, enum vm_event ev, void *arg)
{
	switch (ev) {
	case EV_CLS:
		_put_event(EV_CLS, nullptr, nullptr);
		break;
	case EV_KEY_PRESS:
		_put_event(EV_KEY_PRESS, arg, nullptr);
		break;
	default:
		break;
	}
}

void
_handle_event(struct context *ctx, SDL_UserEvent const *event)
{
	SDL_KeyboardEvent const *kev = (SDL_KeyboardEvent const *)event->data1;
	u32			*pixels = nullptr;
	i32			 row_len;

	switch (event->code) {
	case EV_CLS:
		SDL_LockTexture(
			ctx->texture, nullptr, (void **)&pixels, &row_len);
		SDL_UnlockTexture(ctx->texture);
		break;
	case EV_KEY_PRESS:
		if (ctx->config.verbose) {
			SDL_Log("KeyPress: %u", kev->key);
		}
		if (ctx->vm.kbd_r < 16) {
			ctx->vm.regs[ctx->vm.kbd_r] = kev->key;
		}
		break;
	case EV_SOUND:
		/* TODO: Play Beep Sound */
		break;
	default:
		break;
	}

	/* unblock the cpu */
	atomic_store(&ctx->vm.state, VM_IDLE);
}
