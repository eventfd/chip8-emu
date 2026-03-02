#include "rom.h"
#include <stdio.h>

enum status
parse_rom(struct vm *vm, struct config const *cfg)
{
	enum status sv = E_OK;

	vm_init(vm);
	FILE *file = fopen(cfg->rom_file, "rb");
	if (!file) {
		i32 const _err = errno;
		switch (_err) {
		case EACCES:
			sv = E_FILE_PERM_READ;
			break;
		case EINVAL:
			sv = E_FILE_PATH_INV;
			break;
		case ENOENT:
			sv = E_FILE_EXIST;
			break;
		default:
			sv = E_GENERIC;
		}
		goto _exit;
	}

	i32 _ok = fseek(file, 0, SEEK_END);
	if (_ok == -1) {
		i32 const _err = errno;
		switch (_err) {
		case ESPIPE:
			sv = E_FILE_SEEKABLE;
			break;
		default:
			sv = E_GENERIC;
		}
		goto _clean_exit;
	}

	/*
	 * file is seekable by now, so we should not have any errors
	 */
	isize const _pos = ftell(file);
	if (_pos == -1) {
		/* unknown error */
		sv = E_GENERIC;
		goto _clean_exit;
	}

	if (!_pos) {
		/* handle empty files */
		sv = E_FILE_TOO_SMALL;
		goto _clean_exit;
	}

	/* max addressable space is 0xe00 bytes */
	if (_pos > 0xe00) {
		sv = E_FILE_TOO_LARGE;
		goto _clean_exit;
	}

	_ok = fseek(file, 0, SEEK_SET);
	if (_ok == -1) {
		/* we should not have any errors by now, just in case! */
		i32 const _err = errno;
		switch (_err) {
		case ESPIPE:
			sv = E_FILE_SEEKABLE;
			break;
		default:
			sv = E_GENERIC;
		}
		goto _clean_exit;
	}

	i32 const n_read = fread(vm->code, 1, _pos, file);
	if (n_read != _pos) {
		sv = E_FILE_SIZE_MISMATCH;
		goto _clean_exit;
	}

	vm->code_size = n_read;

_clean_exit:
	fclose(file);
_exit:
	return sv;
}
