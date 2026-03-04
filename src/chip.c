#include "chip.h"
#include "allocator.h"
#include "argv.h"
#include "config.h"
#include "rand.h"
#include "rom.h"
#include "util.h"
#include "vm.h"
#include <SDL3/SDL.h>

#define HZ_TO_NS(hz) (UINT64_C(1000000000) / (hz))
#define RGB(r, g, b) ((u32)(r) | (u32)(g) << 8 | (u32)(b) << 16)

static u64 cpu_clock_handler(void *userdata, SDL_TimerID timerID, u64 interval);
static u64 sound_clock_handler(
	void *userdata, SDL_TimerID timerID, u64 interval);
static void vm_callback(struct vm *vm, enum vm_event ev, void *arg);
static void post_event(i32 code, void *data1, void *data2, u32 size);
static void _handle_event(struct context *ctx, SDL_UserEvent const *event);
static void _play_beep(struct context *ctx);

constexpr u32 AUDIO_FREQ      = 750;
constexpr u32 SAMPLE_RATE     = 44100;
constexpr u32 SAMPLE_DURATION = 200; /* millis */
constexpr u32 AUDIO_SIZE      = SAMPLE_RATE * SAMPLE_DURATION / 1000;
static u8     audio_samples[AUDIO_SIZE];

enum status
chip_main(i32 argc, char *argv[const])
{
	enum status    sv    = E_OK;
	struct context ctx   = {0};
	SDL_Event      event = {0};

	sv = parse_argv(&ctx.config, argc, argv);
	if (sv != E_OK) {
		goto _exit;
	}

	if (ctx.config.verbose) {
		SDL_Log("ROM File: %s", ctx.config.rom_file);
		SDL_Log("Width: %u", ctx.config.dx);
		SDL_Log("Height: %u", ctx.config.dy);
		SDL_Log("Clock (Hz): %u", ctx.config.clock_speed);
	}

	sv = parse_rom(&ctx.vm, &ctx.config);
	if (sv != E_OK) {
		goto _exit;
	}

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		sv = E_SDL_INIT;
		LOG_ERROR(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
			SDL_GetError());
		goto _exit;
	}

	ctx.user_event = SDL_RegisterEvents(1);
	ctx.rwlock     = SDL_CreateRWLock();

	bool const rv = SDL_CreateWindowAndRenderer("CHIP8 Emulator",
		ctx.config.dx * 64, ctx.config.dy * 32,
		SDL_WINDOW_HIGH_PIXEL_DENSITY, &ctx.window, &ctx.renderer);
	if (!rv) {
		sv = E_SDL_WIN_INIT;
		LOG_ERROR(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateWindowAndRenderer failed: %s",
			SDL_GetError());
		goto _clean_exit;
	}

	SDL_Texture *const texture = SDL_CreateTexture(ctx.renderer,
		SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING,
		ctx.config.dx * 64, ctx.config.dy * 32);
	if (!texture) {
		sv = E_SDL_TEXTURE_INIT;
		LOG_ERROR(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateTexture failed: %s", SDL_GetError());
		goto _exit_loop;
	}

	ctx.texture = texture;
	/* clear the screen first */
	vm_callback(&ctx.vm, EV_DRAW, nullptr);

	/* initialize audio device */
	SDL_AudioSpec const audio_spec = {
		.format	  = SDL_AUDIO_U8,
		.channels = 2,
		.freq	  = SAMPLE_RATE,
	};
	SDL_AudioStream *audio_stream
		= SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
			&audio_spec, nullptr, nullptr);
	if (!audio_stream) {
		sv = E_SDL_AUDIO_INIT;
		LOG_ERROR(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
		goto _exit_loop;
	}

	ctx.audio_stream = audio_stream;
	for (u32 i = 0; i != AUDIO_SIZE; i++) {
		f64 const time = (f64)i / (f64)SAMPLE_RATE;
		f64 const _sin
			= SDL_sin(2.0 * SDL_PI_F * (f64)AUDIO_FREQ * time)
				  * 127.0
			  + 128.0;
		audio_samples[i] = (u8)_sin;
	}

	/* initialize random number generator */
	rnd_init(SDL_GetTicks());

	/* initialize cpu clock */
	SDL_AddTimerNS(HZ_TO_NS(ctx.config.clock_speed), cpu_clock_handler,
		(void *)&ctx);

	/* BUGFIX: sound timer has a separate clock */
	SDL_AddTimerNS(HZ_TO_NS(ctx.config.clock_speed), sound_clock_handler,
		(void *)&ctx);

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

		SDL_RenderClear(ctx.renderer);
		SDL_RenderTexture(ctx.renderer, ctx.texture, nullptr, nullptr);
		SDL_RenderPresent(ctx.renderer);
	}

	SDL_DestroyAudioStream(ctx.audio_stream);

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
post_event(i32 code, void *data1, void *data2, u32 data2_size)
{
	SDL_Event ev = {0};
	if (data2 && data2_size) {
		/* escape to heap */
		void *hptr = heap_alloc(data2_size);
		memcpy(hptr, data2, data2_size);
		data2 = hptr;
	}
	ev.user = (SDL_UserEvent){
		.type  = SDL_EVENT_USER,
		.code  = code,
		.data1 = data1,
		.data2 = data2,
	};
	SDL_PushEvent(&ev);
}

u64
cpu_clock_handler(void *userdata, SDL_TimerID timer_id, u64 interval)
{
	UNUSED(timer_id);
	UNUSED(interval);
	struct context *const ctx = (struct context *)userdata;

	/* handle the delay timer*/
	if (ctx->vm.delay_timer) {
		ctx->vm.delay_timer -= 1;
	}

	/* run the cpu */
	vm_step(&ctx->vm, vm_callback);
	if (ctx->config.verbose >= 2) {
		SDL_Log("> Clock Event at %" PRIu64 "  ms", SDL_GetTicks());
	}

	return HZ_TO_NS(ctx->config.clock_speed);
}

void
vm_callback(struct vm *vm, enum vm_event ev, void *arg)
{
	atomic_store(&vm->state, VM_WAIT);
	UNUSED(vm);

	switch (ev) {
	case EV_DRAW:
		post_event(EV_DRAW, nullptr, arg, 0);
		break;
	default:
		break;
	}
}

void
_handle_event(struct context *ctx, SDL_UserEvent const *event)
{
	SDL_KeyboardEvent const *kev = (SDL_KeyboardEvent const *)event->data1;

	u8  *pixels;
	i32  tmp_w;
	u32  width, height, mask;
	bool rv;

	switch (event->code) {
	case EV_KEY_PRESS:
		if (SDLK_0 <= kev->key && kev->key <= SDLK_9) {
			mask = 1 << (kev->key - SDLK_0);
			ctx->vm.keymap |= mask;
		}
		/* WIP */
		if (0 > ctx->vm.kbd_r || ctx->vm.kbd_r >= 16) {
			break;
		}
		if (ctx->config.verbose) {
			SDL_Log("KeyPress: %u", kev->key);
		}
		if (SDLK_0 <= kev->key && kev->key <= SDLK_9) {
			ctx->vm.regs[ctx->vm.kbd_r] = kev->key - SDLK_0;
		} else if (SDLK_A <= kev->key && kev->key <= SDLK_F) {
			ctx->vm.regs[ctx->vm.kbd_r] = kev->key - SDLK_A;
		}
		break;
	case EV_DRAW:
		rv = SDL_LockTexture(
			ctx->texture, nullptr, (void **)&pixels, &tmp_w);
		if (!rv) {
			break;
		}
		/* sync the framebuffer */
		width  = ctx->config.dx * 64;
		height = ctx->config.dy * 32;
		for (u32 y = 0; y != height; y++) {
			for (u32 x = 0; x != width; x++) {
				pixels[y * width + x]
					= ctx->vm.fb[64 * (y / ctx->config.dy)
						     + x / ctx->config.dx];
			}
		}
		SDL_UnlockTexture(ctx->texture);
		break;
	default:
		break;
	}

	/* unblock the cpu */
	atomic_store(&ctx->vm.state, VM_RESUME);
}

void
_play_beep(struct context *ctx)
{
	SDL_ResumeAudioStreamDevice(ctx->audio_stream);
	SDL_PutAudioStreamData(ctx->audio_stream, audio_samples, AUDIO_SIZE);
	SDL_Delay(SAMPLE_DURATION);
	SDL_PauseAudioStreamDevice(ctx->audio_stream);
}

u64
sound_clock_handler(void *userdata, SDL_TimerID timerID, u64 interval)
{
	UNUSED(timerID);
	struct context *const ctx = (struct context *)userdata;

	/* evaluate sound timer */
	u16 const sound_timer = atomic_load(&ctx->vm.sound_timer);
	if (sound_timer) {
		_play_beep(ctx);
		atomic_store(&ctx->vm.sound_timer, sound_timer - 1);
	}

	return interval;
}
