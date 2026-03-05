#include "argv.h"
#include <getopt.h>
#include <stdio.h>

static struct option const opts[] = {
	{
		.name	 = "dx",
		.has_arg = required_argument,
		.flag	 = nullptr,
		.val	 = 'W',
	 },
	{
		.name	 = "dy",
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
		.name	 = "asm",
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
		.dx	     = 10,
		.dy	     = 15,
		.clock_speed = 60,
		.verbose     = 0,
		.disasm	     = 0,
	};

	for (;;) {
		i32 rv = getopt_long_only(argc, argv, "", opts, nullptr);
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case 'W':
			tmp.dx = SDL_strtoul(optarg, nullptr, 0);
			tmp.dx = SDL_max(tmp.dx, 1);
			break;
		case 'H':
			tmp.dy = SDL_strtoul(optarg, nullptr, 0);
			tmp.dy = SDL_max(tmp.dy, 1);
			break;
		case 'C':
			tmp.clock_speed = SDL_strtoull(optarg, nullptr, 0);
			tmp.clock_speed = SDL_max(tmp.clock_speed, 1);
			break;
		case 'D':
			tmp.disasm = true;
			break;
		case 'V':
			tmp.verbose = 1;
			tmp.disasm  = true;
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
	       "    --dx=N		scaled width of each pixel "
	       "[default: 10]\n"
	       "    --dy=N		scaled height of each pixel "
	       "[default: 15]\n"
	       "    --clk=N		clock frequency of the emulator in Hz "
	       "[default: 60Hz]\n"
	       "    --asm		disassemble instructions [default: "
	       "false]\n"
	       "    --help		show this help message\n",
		prog);
}
