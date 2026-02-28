#include "chip.h"
#include "argv.h"
#include "config.h"
#include "rom.h"
#include <SDL3/SDL.h>

#define HZ_TO_NS(hz) (UINT64_C(1000000000) / (hz))

static u64 _clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval);

enum status
chip_main(i32 argc, char *argv[const])
{
	enum status    sv  = 0;
	struct context ctx = {0};

	sv = parse_argv(&ctx.config, argc, argv);
	if (sv != E_OK) {
		goto _exit;
	}

	sv = parse_rom(&ctx.vm, &ctx.config);
	if (sv != E_OK) {
		goto _exit;
	}

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
		sv = E_SDL_INIT;
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_Init failed: %s", SDL_GetError());
		goto _exit;
	}

	SDL_Window   *window   = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Event     event    = {0};

	ctx.user_event = SDL_RegisterEvents(1);
	ctx.rwlock     = SDL_CreateRWLock();

	bool const rv = SDL_CreateWindowAndRenderer("CHIP8 Emulator",
		ctx.config.window_width, ctx.config.window_height,
		SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer);
	if (!rv) {
		sv = E_SDL_WIN_INIT;
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateWindowAndRenderer failed: %s",
			SDL_GetError());
		goto _clean_exit;
	}

	SDL_AddTimerNS(
		HZ_TO_NS(ctx.config.clock_speed), _clock_pulse, (void *)&ctx);

	/* start the event loop */
	for (;;) {
		if (!SDL_PollEvent(&event)) {
			continue;
		}
		switch (event.type) {
		case SDL_EVENT_QUIT:
			goto _exit_loop;
		}

		if (event.type == ctx.user_event) {
			switch (event.user.code) {
			case EV_CLOCK_TICK:
				break;
			case EV_PAINT:
				break;
			}
		}

		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}

_exit_loop:
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

_clean_exit:
	SDL_DestroyRWLock(ctx.rwlock);
	SDL_Quit();

_exit:
	return sv;
}

u64
_clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval)
{
	struct context *ctx = (struct context *)userdata;
	SDL_PushEvent(&(SDL_Event){
		.user = {
			 .type = SDL_EVENT_USER,
			 .code =  EV_PAINT,
		},
	 });
	return HZ_TO_NS(ctx->config.clock_speed);
}
