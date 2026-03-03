#include "argv.h"
#include <getopt.h>
#include <stdio.h>

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
		.name	 = "disasm",
		.has_arg = no_argument,
		.flag	 = nullptr,
		.val	 = 'D',
	 },
	{
		.name	 = "help",
		.has_arg = no_argument,
		.flag	 = nullptr,
		.val	 = 'h',
	 },
	{
		.name	 = "verbose",
		.has_arg = no_argument,
		.flag	 = nullptr,
		.val	 = 'V',
	 },
	{0},
};

static void	   _print_usage(char const *prog);
static enum status _parse_argv_impl(
	struct config *config, i32 argc, char *argv[const]);

enum status
parse_argv(struct config *config, i32 argc, char *argv[const])
{
	enum status const sv = _parse_argv_impl(config, argc, argv);
	if (sv == E_ARG_PARSE) {
		_print_usage(argv[0]);
	}
	return sv;
}

enum status
_parse_argv_impl(struct config *config, i32 argc, char *argv[const])
{

	/* default options */
	struct config tmp = {
		.width	     = 640,
		.height	     = 480,
		.clock_speed = 60,
		.verbose     = 0,
		.disasm	     = true,
	};

	for (;;) {
		i32 rv = getopt_long_only(argc, argv, "", opts, nullptr);
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case 'W':
			tmp.width = SDL_strtoul(optarg, nullptr, 0);
			tmp.width = SDL_max(tmp.width, 640);
			break;
		case 'H':
			tmp.height = SDL_strtoul(optarg, nullptr, 0);
			tmp.height = SDL_max(tmp.height, 480);
			break;
		case 'C':
			tmp.clock_speed = SDL_strtoul(optarg, nullptr, 0);
			tmp.clock_speed = SDL_max(tmp.clock_speed, 1);
			break;
		case 'D':
			tmp.disasm = true;
			break;
		case 'V':
			tmp.verbose = 1;
			break;
		default:
			return E_ARG_PARSE;
		}
	}

	if (optind == argc) {
		/* mandatory arguments missing! */
		return E_ARG_PARSE;
	}

	tmp.rom_file = argv[optind];
	*config	     = tmp;
	return E_OK;
}

static void
_print_usage(char const *prog)
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
