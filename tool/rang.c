#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include <cover/container.h>
#include <cover/rang.h>
#include <Imlib2.h>
#include <jpeglib.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

#include "main.h"
#include "file.h"
#include "container-file.h"

static void remove_alpha(size_t length, uint32_t *data) {
	for (size_t i = 0; i < length; ++i) {
		uint_fast32_t transparent = data[i];
		uint_fast32_t opaque = 0xff000000;

		double foreground_share = (double) (transparent >> 24) / 255;
		double background_share = 1 - foreground_share;

		for (size_t j = 0; j < 3; ++j) {
			opaque |= (uint_fast32_t) ((transparent & 0xff) * foreground_share + 0xff * background_share + .0001) << 8 * j;

			transparent >>= 8;
		}

		data[i] = opaque;
	}
}

static void error_exit(j_common_ptr compressor) {
	char error_message[JMSG_LENGTH_MAX];

	compressor->err->format_message(compressor, error_message);

	fprintf(stderr, "LibJPEG error: %s\n", error_message);

	longjmp(compressor->client_data, 1);
}

static void emit_message(j_common_ptr compressor, int level) {
	if (level == -1) {
		char error_message[JMSG_LENGTH_MAX];

		compressor->err->format_message(compressor, error_message);

		fprintf(stderr, "LibJPEG warning: %s", error_message);
	}
}

static bool save_image(size_t width, size_t height, uint32_t *data, const char *file_name, JSAMPLE *row) {
	bool result = false;

	FILE *file = fopen(file_name, "wb");

	if (file == NULL) {
		perror("LibC error");

		goto error_file;
	}

	struct jpeg_error_mgr error_manager;

	jpeg_std_error(&error_manager);
	error_manager.error_exit = error_exit;
	error_manager.emit_message = emit_message;

	jmp_buf catch;

	struct jpeg_compress_struct compressor;

	compressor.err = &error_manager;
	compressor.client_data = catch;

	if (setjmp(catch) != 0) {
		goto error_compressor;
	}

	jpeg_create_compress(&compressor);
	jpeg_stdio_dest(&compressor, file);

	compressor.image_width = width;
	compressor.image_height = height;
	compressor.input_components = 3;
	compressor.in_color_space = JCS_RGB;

	jpeg_set_defaults(&compressor);
	jpeg_set_quality(&compressor, 95, false);

	jpeg_start_compress(&compressor, true);

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			uint_fast32_t pixel = data[width * y + x];

			row[3 * x] = pixel >> 16 & 0xff;
			row[3 * x + 1] = pixel >> 8 & 0xff;
			row[3 * x + 2] = pixel & 0xff;
		}

		jpeg_write_scanlines(&compressor, &row, 1);
	}

	jpeg_finish_compress(&compressor);

	result = true;

	error_compressor: jpeg_destroy_compress(&compressor);

	if (fclose(file) == EOF) {
		perror("LibC error");

		result = false;
	}

	error_file: ;

	return result;
}

#define SYMLINK_NAME_BUFFER_LENGTH (14 + 3 * sizeof (int) + 1)

static int main_modify(int argc, char **argv) {
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

	// Imlib2 has problems with file names containing a colon

	FILE *file = fopen(argv[optind], "rb");

	if (file == NULL) {
		perror("LibC error");
		fputs("Can't open image file\n", stderr);

		goto error_file;
	}

	char symlink_name[SYMLINK_NAME_BUFFER_LENGTH];

	snprintf(symlink_name, SYMLINK_NAME_BUFFER_LENGTH, "/proc/self/fd/%i", fileno(file));

	Imlib_Image image = imlib_load_image_immediately_without_cache(symlink_name);

	if (image == NULL) {
		fputs("Can't read image\n", stderr);

		goto error_image;
	}

	imlib_context_set_image(image);

	size_t width = imlib_image_get_width();
	size_t height = imlib_image_get_height();

	if (width == 0 || width > 65535 || height > 65535) {
		fputs("Wrong image size\n", stderr);

		goto error_size;
	}

	uint32_t *data = imlib_image_get_data();

	if (data == NULL) {
		fputs("Can't read image\n", stderr);

		goto error_data;
	}

	uint32_t *blured_column = malloc(4 * (height == 0 ? 1 : height));

	if (blured_column == NULL) {
		perror("LibC error");

		goto error_blured;
	}

	uint32_t *untouched_column = malloc(4 * (height == 0 ? 1 : height));

	if (untouched_column == NULL) {
		perror("LibC error");

		goto error_untouched;
	}

	if (SIZE_MAX / sizeof (JSAMPLE) / 3 < width) {
		fputs("Can't allocate memory\n", stderr);

		goto error_row;
	}

	JSAMPLE *row = malloc(sizeof (JSAMPLE) * 3 * width);

	if (row == NULL) {
		perror("LibC error");

		goto error_row;
	}

	if (imlib_image_has_alpha()) {
		remove_alpha(width * height, data);
	}

	if (!save_image(width, height, data, argv[optind + 1], row)) {
		fputs("Can't write clear image\n", stderr);

		goto error_clear;
	}

	Cover_Rang_modify_image(width, height, data, blured_column, untouched_column);

	if (!save_image(width, height, data, argv[optind + 2], row)) {
		fputs("Can't write modified image\n", stderr);

		goto error_modified;
	}

	result = EXIT_SUCCESS;

	error_modified: ;
	error_clear: free(row);
	error_row: free(untouched_column);
	error_untouched: free(blured_column);
	error_blured: imlib_image_put_back_data(data);
	error_data: ;
	error_size: imlib_free_image();

	error_image: if (fclose(file) == EOF) {
		perror("LibC error");
		fputs("Can't close image file\n", stderr);

		result = EXIT_FAILURE;
	}

	error_file: ;
	error_command_line: ;

	return result;
}

static int main_extract(int argc, char **argv) {
	int result = EXIT_FAILURE;

	size_t data_length = 1024;

	const char *short_options = "l:";

	struct option long_options[] = {
		{"length", required_argument, NULL, 'l'},
		{0}
	};

	opterr = 0;

	while (true) {
		int option = getopt_long(argc, argv, short_options, long_options, NULL);

		if (option == -1) {
			break;
		} else if (option == 'l') {
			char *end;
			uintmax_t parsed = strtoumax(optarg, &end, 0);

			if (*optarg == '\0' || *end != '\0' || parsed > SIZE_MAX) {
				fputs("Wrong length\n", stderr);

				goto error_command_line;
			}

			data_length = parsed;
		} else {
			fputs("Wrong option\n", stderr);

			goto error_command_line;
		}
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

	if (setjmp(image.catch) != 0) {
		fputs("Can't read coefficients\n", stderr);

		goto error_coefficients;
	}

	struct Cover_Rang Rang;

	if (!Cover_Rang_initialize(&Rang, container, NULL, NULL)) {
		fputs("Can't allocate memory\n", stderr);

		goto error_Rang;
	}

	printf("Set least significant bits: %zu\n", Rang.set_count);

	uint8_t *data = malloc(data_length == 0 ? 1 : data_length);

	if (data == NULL) {
		perror("LibC error");

		goto error_data;
	}

	Cover_Rang_extract(&Rang, data_length, data);

	if (!file_write(argv[optind + 1], data_length, data)) {
		fputs("Can't write result file\n", stderr);

		goto error_output;
	}

	result = EXIT_SUCCESS;

	error_output: free(data);
	error_data: ;
	error_coefficients: Cover_Rang_destroy(&Rang);
	error_Rang: container_file_destroy(&image);
	error_image: ;
	error_command_line: ;

	return result;
}

static int main_embed(int argc, char **argv) {
	int result = EXIT_FAILURE;

	size_t padding_bits_count = COVER_RANG_DEFAULT_PADDING_BITS_COUNT;
	char *entropy_file_name = NULL;

	const char *short_options = "e:p:z";

	struct option long_options[] = {
		{"entropy", required_argument, NULL, 'e'},
		{"padding-bits-count", required_argument, NULL, 'p'},
		{0}
	};

	opterr = 0;

	while (true) {
		int option = getopt_long(argc, argv, short_options, long_options, NULL);

		if (option == -1) {
			break;
		} else if (option == 'e') {
			entropy_file_name = optarg;
		} else if (option == 'p') {
			char *end;
			uintmax_t parsed = strtoumax(optarg, &end, 0);

			if (*optarg == '\0' || *end != '\0' || parsed > SIZE_MAX) {
				fputs("Wrong padding bits count\n", stderr);

				goto error_command_line;
			}

			padding_bits_count = parsed;
		} else {
			fputs("Wrong option\n", stderr);

			goto error_command_line;
		}
	}

	if (argc - optind != 4) {
		fputs("Wrong number of file arguments\n", stderr);

		goto error_command_line;
	}

	uint8_t entropy[COVER_RANG_ENTROPY_LENGTH];

	if (entropy_file_name == NULL) {
		if (gnutls_rnd(GNUTLS_RND_RANDOM, entropy, COVER_RANG_ENTROPY_LENGTH) != 0) {
			goto error_entropy;
		}
	} else {
		size_t entropy_length = COVER_RANG_ENTROPY_LENGTH;

		if (!file_read(&entropy_length, entropy, entropy_file_name, false)) {
			fputs("Can't read entropy file\n", stderr);

			goto error_entropy;
		}

		if (entropy_length != COVER_RANG_ENTROPY_LENGTH) {
			fputs("Not enough entropy\n", stderr);

			goto error_entropy;
		}
	}

	struct container_file clear;
	struct container_file modified;

	if (!container_file_initialize(&clear, argv[optind + 1])) {
		fputs("Can't read clear image\n", stderr);

		goto error_clear;
	}

	if (!container_file_initialize(&modified, argv[optind + 2])) {
		fputs("Can't read modified image\n", stderr);

		goto error_modified;
	}

	if (
		clear.container.width_in_blocks != modified.container.width_in_blocks ||
		clear.container.height_in_blocks != modified.container.height_in_blocks
	) {
		fputs("Images have different dimensions\n", stderr);

		goto error_size;
	}

	if (setjmp(clear.catch) != 0) {
		fputs("Can't read clear coefficients\n", stderr);

		goto error_clear_coefficients;
	}

	if (setjmp(modified.catch) != 0) {
		fputs("Can't read modified coefficients\n", stderr);

		goto error_modified_coefficients;
	}

	struct Cover_Rang Rang;

	if (!Cover_Rang_initialize(&Rang, &clear.container, &modified.container, entropy)) {
		fputs("Can't allocate memory\n", stderr);

		goto error_Rang;
	}

	if (Rang.usable_count < padding_bits_count) {
		fputs("Too low capacity\n", stderr);

		goto error_capacity;
	}

	size_t length = (Rang.usable_count - padding_bits_count) / 8;

	printf("Available capacity in bytes: %zu\n", length);

	uint8_t *data = malloc(length == 0 ? 1 : length);

	if (data == NULL) {
		perror("LibC error");

		goto error_data;
	}

	if (!file_read(&length, data, argv[optind], true)) {
		fputs("Can't read data\n", stderr);

		goto error_input;
	}

	int embedding_result = Cover_Rang_embed(&Rang, length, data, padding_bits_count);

	if (embedding_result != 0) {
		if (embedding_result == 1) {
			fputs("Can't allocate memory\n", stderr);
		} else {
			fputs("Can't find non-singular matrix\n", stderr);
		}

		goto error_embed;
	}

	if (setjmp(clear.catch) != 0) {
		fputs("Can't write coefficients\n", stderr);

		goto error_apply;
	}

	size_t changed_count = Cover_Rang_apply(&Rang);

	printf("Changed coffiecients: %zu\n", changed_count);

	if (!container_file_write(&clear, argv[optind + 3])) {
		fputs("Can't write result\n", stderr);

		goto error_output;
	}

	result = EXIT_SUCCESS;

	error_output: ;
	error_apply: ;
	error_embed: ;
	error_input: free(data);
	error_data: ;
	error_capacity: ;
	error_modified_coefficients: Cover_Rang_destroy(&Rang);
	error_clear_coefficients: ;
	error_Rang: ;
	error_size: container_file_destroy(&modified);
	error_modified: container_file_destroy(&clear);
	error_clear: ;
	error_entropy: ;
	error_command_line: ;

	return result;
}

int main_rang(int argc, char **argv) {
	if (argc < 2) {
		fputs("Command not specified\n", stderr);

		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "modify") == 0) {
		return main_modify(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "extract") == 0) {
		return main_extract(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "embed") == 0) {
		return main_embed(argc - 1, argv + 1);
	} else {
		fputs("Unknown command\n", stderr);

		return EXIT_FAILURE;
	}
}
