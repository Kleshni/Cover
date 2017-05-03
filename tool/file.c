#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "file.h"

bool file_read(size_t *length, uint8_t *data, const char *name, bool whole) {
	FILE *file = fopen(name, "rb");

	if (file == NULL) {
		perror("LibC error");

		return false;
	}

	bool result = true;

	*length = fread(data, 1, *length, file);

	if (ferror(file)) {
		perror("LibC error");

		result = false;
	} else if (whole) {
		if (fgetc(file) != EOF) {
			fputs("File too long\n", stderr);

			result = false;
		} if (ferror(file)) {
			perror("LibC error");

			result = false;
		}
	}

	if (fclose(file) == EOF) {
		perror("LibC error");

		result = false;
	}

	return result;
}

bool file_write(const char *name, size_t length, const uint8_t *data) {
	FILE *file = fopen(name, "wb");

	if (file == NULL) {
		perror("LibC error");

		return false;
	}

	bool result = true;

	if (fwrite(data, length, 1, file) != 1) {
		perror("LibC error");

		result = false;
	}

	if (fclose(file) == EOF) {
		perror("LibC error");

		result = false;
	}

	return result;
}
