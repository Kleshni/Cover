#ifndef FILE_H
	#define FILE_H

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	bool file_read(size_t *length, uint8_t *data, const char *name, bool whole);
	bool file_write(const char *name, size_t length, const uint8_t *data);
#endif
