#include "chip.h"
#include "config.h"
#include <SDL3/SDL.h>
#include <getopt.h>
#include <stdio.h>

#define HZ_TO_NS(hz) (UINT64_C(1000000000) / (hz))

static u64 _clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval);

static void print_usage(char const *prog);

static bool parse_argv(struct config *config, i32 argc, char *argv[const]);

i32
main(i32 argc, char *argv[const])
{
	i32 status = 0;

	struct context ctx = {0};
	if (!parse_argv(&ctx.config, argc, argv)) {
		status = 1;
		goto _exit;
	}

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_Init failed: %s", SDL_GetError());
		status = 1;
		goto _exit;
	}

	SDL_Window   *window   = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Event     event    = {0};

	ctx.user_event = SDL_RegisterEvents(1);
	ctx.rwlock     = SDL_CreateRWLock();

	bool rv = SDL_CreateWindowAndRenderer(ctx.config.window_title,
		ctx.config.window_width, ctx.config.window_height,
		SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer);
	if (!rv) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"SDL_CreateWindowAndRenderer failed: %s",
			SDL_GetError());
		status = 2;
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
				SDL_LockRWLockForReading(ctx.rwlock);
				SDL_SetRenderDrawColor(
					renderer, ctx.r, ctx.g, ctx.b, 255);
				SDL_UnlockRWLock(ctx.rwlock);
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
	return status;
}

u64
_clock_pulse(void *userdata, SDL_TimerID timerID, u64 interval)
{
	struct context *ctx = (struct context *)userdata;
	SDL_LockRWLockForWriting(ctx->rwlock);
	ctx->r += 1;
	ctx->g += 1;
	ctx->b += 1;
	SDL_UnlockRWLock(ctx->rwlock);
	SDL_PushEvent(&(SDL_Event){
		.user = {
			 .type = SDL_EVENT_USER,
			 .code =  EV_PAINT,
		},
	 });
	return HZ_TO_NS(ctx->config.clock_speed);
}

void
print_usage(char const *prog)
{
	printf("Usage: %s [options] <rom-file>\n\n"
	       "Options:\n"
	       "    --width=N		width of the window [default: 640]\n"
	       "    --height=N		height of the window [default: 480]\n"
	       "    --clk=N		clock frequency of the emulator in Hz "
	       "[default: 60Hz]\n"
	       "    --help		show this help message\n",
		prog);
}

static bool
parse_argv(struct config *config, i32 argc, char *argv[const])
{
	static struct option const opts[] = {
		{
			.name	 = "width",
			.has_arg = required_argument,
			.flag	 = nullptr,
			.val	 = 'W',
		 },
		{
			.name	 = "height",
			.has_arg = required_argument,
			.flag	 = nullptr,
			.val	 = 'H',
		 },
		{
			.name	 = "clk",
			.has_arg = required_argument,
			.flag	 = nullptr,
			.val	 = 'C',
		 },
		{
			.name	 = "help",
			.has_arg = no_argument,
			.flag	 = nullptr,
			.val	 = 'h',
		 },
		{0},
	};

	/* default options */
	struct config tmp = {
		.window_width  = 640,
		.window_height = 480,
		.window_title  = "CHIP-8 Emulator",
		.clock_speed   = 60,
	};

	for (;;) {
		i32 rv = getopt_long_only(argc, argv, "", opts, nullptr);
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case 'W':
			tmp.window_width = SDL_strtoul(optarg, nullptr, 0);
			tmp.window_width = SDL_max(tmp.window_width, 640);
			break;
		case 'H':
			tmp.window_height = SDL_strtoul(optarg, nullptr, 0);
			tmp.window_height = SDL_max(tmp.window_height, 480);
			break;
		case 'C':
			tmp.clock_speed = SDL_strtoul(optarg, nullptr, 0);
			tmp.clock_speed = SDL_max(tmp.clock_speed, 60);
			break;
		default:
			print_usage(argv[0]);
			return false;
		}
	}

	*config = tmp;
	return true;
}
