#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include <cover/container.h>

#include "main.h"
#include "container-file.h"

static int main_read(int argc, char **argv) {
	int result = EXIT_FAILURE;

	opterr = 0;

	if (getopt(argc, argv, "") != -1) {
		fputs("No command line options supported\n", stderr);

		goto error_command_line;
	}

	if (argc - optind != 2) {
		fputs("Wrong number of file arguments\n", stderr);

		goto error_command_line;
	}

	struct container_file image;

	if (!container_file_initialize(&image, argv[optind])) {
		fputs("Can't read image\n", stderr);

		goto error_image;
	}

	struct Cover_container *container = &image.container;

	printf("Width in blocks: %zu\n", container->width_in_blocks);
	printf("Height in blocks: %zu\n", container->height_in_blocks);
	printf("Coefficients: %zu\n", container->coefficients_count);

	FILE *result_file = fopen(argv[optind + 1], "wb");

	if (result_file == NULL) {
		perror("LibC error");
		fputs("Can't open result file\n", stderr);

		goto error_result_file;
	}

	if (setjmp(image.catch) != 0) {
		fputs("Can't read coefficients\n", stderr);

		goto error_coefficients;
	}

	struct jpeg_decompress_struct *decompressor = container->decompressor;
	struct jvirt_barray_control *coefficients = container->coefficients;

	size_t width_in_blocks = container->width_in_blocks;
	size_t height_in_blocks = container->height_in_blocks;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, false
		);

		for (size_t x = 0; x < width_in_blocks; ++x) {
			for (size_t c = 0; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				JCOEF coefficient = buffer[0][x][c];

				uint8_t bytes[2] = {
					(uint_fast16_t) coefficient >> 8 & 0xff,
					(uint_fast16_t) coefficient & 0xff
				};

				if (fwrite(bytes, 2, 1, result_file) != 1) {
					perror("LibC error");
					fputs("Can't write result\n", stderr);

					goto error_output;
				}
			}
		}
	}

	result = EXIT_SUCCESS;

	error_output: ;

	error_coefficients: if (fclose(result_file) == EOF) {
		perror("LibC error");
		fputs("Can't write result\n", stderr);

		result = EXIT_FAILURE;
	}

	error_result_file: container_file_destroy(&image);
	error_image: ;
	error_command_line: ;

	return result;
}

static int main_replace(int argc, char **argv) {
	int result = EXIT_FAILURE;

	opterr = 0;

	if (getopt(argc, argv, "") != -1) {
		fputs("No command line options supported\n", stderr);

		goto error_command_line;
	}

	if (argc - optind != 3) {
		fputs("Wrong number of file arguments\n", stderr);

		goto error_command_line;
	}

	struct container_file image;

	if (!container_file_initialize(&image, argv[optind + 1])) {
		fputs("Can't read image\n", stderr);

		goto error_image;
	}

	struct Cover_container *container = &image.container;

	FILE *coefficients_file = fopen(argv[optind], "rb");

	if (coefficients_file == NULL) {
		perror("LibC error");
		fputs("Can't open coefficients file\n", stderr);

		goto error_coefficients_file;
	}

	if (setjmp(image.catch) != 0) {
		fputs("Can't write coefficients\n", stderr);

		goto error_apply;
	}

	struct jpeg_decompress_struct *decompressor = container->decompressor;
	struct jvirt_barray_control *coefficients = container->coefficients;

	size_t width_in_blocks = container->width_in_blocks;
	size_t height_in_blocks = container->height_in_blocks;

	size_t changed_count = 0;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, true
		);

		for (size_t x = 0; x < width_in_blocks; ++x) {
			for (size_t c = 0; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				uint8_t bytes[2];

				if (fread(bytes, 2, 1, coefficients_file) != 1) {
					perror("LibC error");
					fputs("Can't read coefficients\n", stderr);

					goto error_input;
				}

				int_fast32_t coefficient = (int_fast32_t) bytes[0] << 8 | bytes[1];

				if ((coefficient >> 15 & 1) == 1) {
					coefficient |= -0x10000;
				}

				if (buffer[0][x][c] != coefficient) {
					++changed_count;
				}

				buffer[0][x][c] = coefficient;
			}
		}
	}

	printf("Changed coefficients: %zu\n", changed_count);

	if (fgetc(coefficients_file) != EOF) {
		fputs("Too many coefficients\n", stderr);

		goto error_input;
	} else if (ferror(coefficients_file)) {
		perror("LibC error");
		fputs("Can't read coefficients\n", stderr);

		goto error_input;
	}

	if (!container_file_write(&image, argv[optind + 2])) {
		fputs("Can't write result\n", stderr);

		goto error_output;
	}

	result = EXIT_SUCCESS;

	error_output: ;
	error_input: ;

	error_apply: if (fclose(coefficients_file) == EOF) {
		perror("LibC error");
		fputs("Can't close coefficients file\n", stderr);

		result = EXIT_FAILURE;
	}

	error_coefficients_file: container_file_destroy(&image);
	error_image: ;
	error_command_line: ;

	return result;
}

int main_container(int argc, char **argv) {
	if (argc < 2) {
		fputs("Command not specified\n", stderr);

		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "read") == 0) {
		return main_read(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "replace") == 0) {
		return main_replace(argc - 1, argv + 1);
	} else {
		fputs("Unknown command\n", stderr);

		return EXIT_FAILURE;
	}
}
